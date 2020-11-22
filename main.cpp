#include <QCoreApplication>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <QFile>
#include <QDir>
#include <QFileInfoList>
#include <Windows.h>
#include <QDebug>
#include <QStandardPaths>
#include <set>
#include <map>
#include <string>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <sstream>
#include <algorithm>

std::set< QString > files;//记录文件是否已被复制过
std::set< QString > HASH;
std::set< QString > except_files;//排除的文件
std::set< QString > Dirs;//记录每科对应文件夹
std::map<QString, QDateTime> Time;//记录文件的修改时间
std::ifstream fin;
std::ofstream fout;
std::ifstream read_hash;
std::ofstream write_hash;
SYSTEMTIME sys;//用于获取系统时间
//std::map< QString , QDateTime> Time;
QString output;//目标目录
QString input;//文件源
int now=0;
int mod;//文件夹更新检测模式
bool other_mod;
bool save_hash;
int StringToInt(std::string str)
{
    int len=str.length();
    int ans=0;
    for(int i=0;i<len;i++)
    {
        if(str[i]>'9'||str[i]<'0') continue;
        ans*=10;
        ans+=str[i]-'0';
    }
    return ans;
}
struct lesson
{
    struct tm
    {
        int hour;
        int minute;
    }from,to;
    QString name;
    bool ip()//读入
    {
        std::string tmp1,tmp2,tmp3,tmp4,tmp5;
        bool flag=(
                    std::getline(fin,tmp1)
                    &&
                    std::getline(fin,tmp2)
                    &&
                    std::getline(fin,tmp3)
                    &&
                    std::getline(fin,tmp4)
                    &&
                    std::getline(fin,tmp5)
                    );
        tmp1=QString::fromStdString(tmp1).toStdString();
        tmp2=QString::fromStdString(tmp2).toStdString();
        tmp3=QString::fromStdString(tmp3).toStdString();
        tmp4=QString::fromStdString(tmp4).toStdString();
        from.hour=StringToInt(tmp1);
        from.minute=StringToInt(tmp2);
        to.hour=StringToInt(tmp3);
        to.minute=StringToInt(tmp4);
        if(flag)
        {
            name=QString::fromStdString(tmp5.c_str());
            Dirs.insert(name);
        }
        return flag;
    }
    bool operator < (const lesson &b) const
    {
        if(from.hour>b.from.hour) return false;
        if(from.hour<b.from.hour) return true;
        if(from.minute>b.from.minute) return false;
        if(from.minute<b.from.minute) return true;
        return false;
    }
}L[100];
//计算文件哈希
QString fileMd5(const QString &sourceFilePath) {

    QFile sourceFile(sourceFilePath);
    qint64 fileSize = sourceFile.size();
    const qint64 bufferSize = 512000;//一次写入500K

    if (sourceFile.open(QIODevice::ReadOnly)) {
        char buffer[bufferSize];
        int bytesRead;
        int readSize = qMin(fileSize, bufferSize);

        QCryptographicHash hash(QCryptographicHash::Md5);

        while (readSize > 0 && (bytesRead = sourceFile.read(buffer, readSize)) > 0) {
            fileSize -= bytesRead;
            hash.addData(buffer, bytesRead);
            readSize = qMin(fileSize, bufferSize);
            Sleep(1);//一秒写入1000次，速度约为50M/s，再大cpu占用就>1%了(尽管实测只有30M/s我也不知道为什么差这么多)
        }

        sourceFile.close();
        return QString(hash.result().toHex());
    }
    return QString();
}
//拷贝文件：
bool copyFileToPath(QString sourceDir ,QString toDir)
{
    toDir.replace("\\","/");
    if (sourceDir == toDir){
        return false;
    }
    if (!QFile::exists(sourceDir)){
        return false;
    }
    QString sourcdMD5=fileMd5(sourceDir);
    if(HASH.find(sourcdMD5)!=HASH.end()) return false;
    else
    {
        HASH.insert(sourcdMD5);
        if(save_hash) write_hash<<sourcdMD5.toStdString()<<'\n';//保存MD5信息以便下次使用
    }
    QDir *createfile = new QDir;
    bool exist = createfile->exists(toDir);
    if (exist)
    {
        if(sourcdMD5!=fileMd5(toDir))//哈希值不一致，新文件
        {
            int cnt=1;
            int point=toDir.length()-1;
            for(;point>=0&&toDir[point]!='/';point--)//寻找文件名的末尾，不包括扩展名
            {
                if(toDir[point]=='.') break;
            }
            if(point==0||toDir[point]=='/')//没有扩展名
            {
                point=toDir.length()-1;
            }
            toDir.insert(point,"(1)");//添加后缀
            exist=createfile->exists(toDir);//更新存在信息
            if(!exist)//不存在冲突即复制
            {
                QFile::copy(sourceDir, toDir);
                fout<<"复制文件："<<toDir.toStdString()<<'\n';
                return true;
            }
            while(exist)//依然冲突
            {
                if(sourcdMD5==fileMd5(toDir))//若为同一文件即终止
                {
                    break;
                }
                cnt++;//后缀+1
                toDir[point+1]='0'+cnt;
                exist=createfile->exists(toDir);
                if(!exist)
                {
                    QFile::copy(sourceDir, toDir);
                    GetLocalTime(&sys);
                    fout<<sys.wHour<<':'<<sys.wMinute<<"  复制文件："<<toDir.toStdString()<<'\n';
                    return true;
                }
            }
        }
    }
    else
    {
        QFile::copy(sourceDir, toDir);//不存在冲突，直接复制
        GetLocalTime(&sys);
        fout<<sys.wHour<<':'<<sys.wMinute<<"  复制文件："<<toDir.toStdString()<<'\n';
        return true;
    }
    return false;
}

//拷贝文件夹：
bool copyDirectoryFiles(const QString &fromDir, const QString &toDir)
{
    bool empty=true;
    QDir sourceDir(fromDir);
    QDir targetDir(toDir);
    if(!targetDir.exists())
    {    /**< 如果目标目录不存在，则进行创建 */
        if(!targetDir.mkdir(targetDir.absolutePath()))
            return false;
    }

    QFileInfoList fileInfoList = sourceDir.entryInfoList();
    foreach(QFileInfo fileInfo, fileInfoList)
    {
        if(fileInfo.fileName() == "." || fileInfo.fileName() == "..")
            continue;

        if(fileInfo.isDir())
        {    /**< 当为目录时，递归的进行copy */
            if(copyDirectoryFiles(fileInfo.filePath(),targetDir.filePath(fileInfo.fileName())))
                empty=false;
        }
        else
        {
            /// 进行文件copy
            if(copyFileToPath(fileInfo.filePath(),targetDir.filePath(fileInfo.fileName()))) empty=false;
        }
    }
    if(empty)
    {
        targetDir.rmdir(targetDir.absolutePath());
        //fout<<"移除空文件夹："<<targetDir.absolutePath().toStdString()<<'\n';
    }
    return empty;
}

void TraceDir(QString path)//遍历目录以复制文件
{
    QDateTime modified_time;
    QString file_name;
    QString file_path;
    QDir dir(path);
    if(!dir.exists() || !dir.makeAbsolute())//文件源不存在,退出
    {
        fout<<sys.wHour<<":"<<sys.wMinute<<"文件源不存在！\n";
        return ;
    }
    QFileInfoList list = dir.entryInfoList();
    for(QFileInfo info : list)
    {
        modified_time=info.lastModified();
        file_name=info.fileName();
        file_path=info.filePath();
        if(info.isDir()&&(file_name.length()>=4&&file_name.mid(file_name.length()-4,4)==".lnk")) continue;
        //qDebug()<<info.fileName();
        if(file_name == "." || file_name == ".."||except_files.find(file_name)!=except_files.end())
        {
            //排除 .和..
            continue;
        }
        switch(mod)
        {
        case 1:
        {
            if(info.isFile())
            {
                if(files.find(file_path)==files.end()||Time[file_path]!=modified_time)//文件名不同或修改时间不同即复制
                {
                    files.insert(file_path);//更新文件名及修改时间
                    Time[file_path]=modified_time;
                    if(other_mod) copyFileToPath(file_path,output+"/Others/"+file_name);
                    else copyFileToPath(file_path,output+"/"+L[now].name+"/"+file_name);
                }
            }
            else//文件夹使用哈希判断
            {
                if(other_mod) copyDirectoryFiles(file_path,output+"/Others/"+file_name);
                else copyDirectoryFiles(file_path,output+"/"+L[now].name+"/"+file_name);
            }
            break;
        }
        case 2:
        {
            if(files.find(file_path)==files.end()||Time[file_path]!=modified_time)//文件(夹)名不同或修改时间不同即复制
            {
                files.insert(file_name);
                Time[file_name]=modified_time;
                if(info.isFile())
                {
                    if(other_mod) copyFileToPath(file_path,output+"/Others/"+file_name);
                    else copyFileToPath(file_path,output+"/"+L[now].name+"/"+file_name);
                }
                else
                {
                    if(other_mod) copyDirectoryFiles(file_path,output+"/Others/"+file_name);
                    else copyDirectoryFiles(file_path,output+"/"+L[now].name+"/"+file_name);
                }
            }
            break;
        }
        }


    }
}
void TestDir(QString path)//判断目标目录下，科目对应文件夹是否存在，如不存在则创建
{
    QString file_name_T;
    QDir dir(path);
    if(!dir.exists() || !dir.makeAbsolute())//目标目录不存在,退出
    {
        fout<<sys.wHour<<":"<<sys.wMinute<<" 目标目录不存在！\n";
        return;
    }
    QFileInfoList list = dir.entryInfoList();
    for(QFileInfo info : list)
    {
        file_name_T=info.fileName();
        if(file_name_T == "." || file_name_T == "..")
        {
            //排除 .和..
            continue;
        }
        if(info.isDir())
        {
            if(Dirs.find(file_name_T)!=Dirs.end())//一个一个目录找,逐一排除
            {
                Dirs.erase(file_name_T);
            }
        }
    }
    Dirs.insert("Others");
    while(!Dirs.empty())//创建不存在的文件夹
    {
        dir.mkdir(path+"/"+*Dirs.begin());
        Dirs.erase(Dirs.begin());
    }
}
void scan(QString path,int space)
{
    QString file_name_T;
    QDir dir(path);
    QString MD5;
    if(!dir.exists() || !dir.makeAbsolute())//目标目录不存在,退出
    {
        fout<<sys.wHour<<":"<<sys.wMinute<<" 目标目录不存在！\n";
        return;
    }
    QFileInfoList list = dir.entryInfoList();
    for(QFileInfo info : list)
    {
        file_name_T=info.fileName();
        if(file_name_T == "." || file_name_T == "..")
        {
            //排除 .和..
            continue;
        }
        if(info.isDir()&&(file_name_T.length()>=4&&file_name_T.mid(file_name_T.length()-4,4)==".lnk")) continue;//无法复制其中的文件，故排除*.lnk文件夹
        if(info.isDir())
        {
            for(int i=0;i<space;i++) fout<<"   ";
            fout<<"正在扫描："<<info.filePath().toStdString()<<'\n';
            scan(info.filePath(),space+1);
        }
        else
        {
            MD5=fileMd5(info.filePath());
            if(HASH.find(MD5)==HASH.end())
            {
                HASH.insert(MD5);
                if(save_hash) write_hash<<MD5.toStdString()<<'\n';
            }
            for(int i=0;i<space;i++) fout<<"   ";
            fout<<"已存在的文件："<<info.filePath().toStdString()<<'\n';
        }
    }
}
int main(int argc, char *argv[])
{
    bool scan_flag;
    QString temp;
    bool desktop=false;
    QCoreApplication a(argc, argv);
    fout.open("log.txt",std::ios::app);
    int sleep;//等待时间
    std:: stringstream ss;
    //qDebug()<<"current currentPath: "<<QDir::currentPath();
    GetLocalTime(&sys);
    fout<<"\n\n"<<sys.wYear<<"年"<<sys.wMonth<<"月"<<sys.wDay<<"日"<<sys.wHour<<":"<<sys.wMinute<<" 程序起动\n";
    //------------------------------读入配置-----------------------------------
    fin.open("settings.txt");
    fout<<" 读入配置 settings.txt\n";
    std::string tmp;
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    input=QString::fromStdString(tmp.c_str());
    while(input[0]=='#')
    {
        std::getline(fin,tmp);
        input=QString::fromStdString(tmp.c_str());
    }
    fout<<"   文件源："<<tmp<<"\n";
    if (input=="Desktop") desktop=true;
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    output=QString::fromStdString(tmp.c_str());
    while(output[0]=='#')
    {
        std::getline(fin,tmp);
        output=QString::fromStdString(tmp.c_str());
    }
    fout<<"   目标目录："<<tmp<<"\n";
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    temp=QString::fromStdString(tmp);
    while(temp[0]=='#')
    {
        std::getline(fin,tmp);
        temp=QString::fromStdString(tmp);
    }
    ss<<tmp;
    ss>>sleep;
    fout<<"   等待时间："<<sleep<<"\n";
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    while(tmp[0]=='#')
    {
        std::getline(fin,tmp);
    }
    ss.clear();
    ss<<tmp;
    ss>>mod;
    fout<<"   文件夹更新检测方式："<<mod<<"\n";
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    temp=QString::fromStdString(tmp);
    while(temp[0]=='#')
    {
        std::getline(fin,tmp);
        temp=QString::fromStdString(tmp);
    }
    ss.clear();
    ss<<tmp;
    ss>>scan_flag;
    /*--------------------------------------------------------------------------*/
    std::getline(fin,tmp);
    temp=QString::fromStdString(tmp);
    while(temp[0]=='#')
    {
        std::getline(fin,tmp);
        temp=QString::fromStdString(tmp);
    }
    ss.clear();
    ss<<tmp;
    ss>>save_hash;
    /*--------------------------------------------------------------------------*/
    fout<<"   排除的文件："<<"\n";
    while(std::getline(fin,tmp))
    {
        temp=QString::fromStdString(tmp);
        while(temp[0]=='#')
        {
            std::getline(fin,tmp);
            temp=QString::fromStdString(tmp);
        }
        except_files.insert(QString::fromStdString(tmp));
        fout<<"      "<<tmp<<"\n";
    }
    fin.close();
    fout<<"配置读入完成\n";
    //----------------------------读入课程表-----------------------------------
    tmp=std::to_string(sys.wDayOfWeek)+".txt";
    fout<<"读入课程表："<<tmp<<"\n";
    fin.open(tmp);
    if(!fin)
    {
        fout<<"读取课程表失败！";
        fout.close();
        return 0;
    }
    int tot=0;
    while(L[tot].ip())
    {
        fout<<"   "<<L[tot].from.hour<<':'<<L[tot].from.minute<<'~'
            <<L[tot].to.hour<<':'<<L[tot].to.minute<<" "<<L[tot].name.toStdString()<<'\n';
        tot++;
    }
    std::sort(L,L+tot);
    if(save_hash)
    {
        read_hash.open("hash.txt");//读取保存的HASH
        if(!read_hash)//没有HASH文件
        {
            if(scan_flag)//扫描并保存HASH信息
            {
                read_hash.close();
                write_hash.open("hash.txt",std::ios::app);
                fout<<"正在扫描目标目录\n";
                scan(output,1);
                fout<<"扫描完成\n";
            }
        }
        else
        {
            while(std::getline(read_hash,tmp))//读取HASH
                HASH.insert(QString::fromStdString(tmp));
            read_hash.close();
            write_hash.open("hash.txt",std::ios::app);
        }
    }
    else if(scan_flag)
    {
        fout<<"正在扫描目标目录\n";
        scan(output,1);
        fout<<"扫描完成\n";
    }
    TestDir(output);//检查输出目录
    //TraceDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    other_mod=false;
    while(1)
    {
        GetLocalTime(&sys);
        if(sys.wHour>L[now].to.hour||(sys.wHour==L[now].to.hour&&sys.wMinute>L[now].to.minute))//当前科目已结束
        {
            //while(sys.wHour>=L[now].to.hour&&sys.wMinute>L[now].to.minute) now++;//好蠢啊。。。。。。
            while(now<=tot&&(sys.wHour>L[now].to.hour||(sys.wHour==L[now].to.hour&&sys.wMinute>L[now].to.minute))) now++;//更新科目信息
            other_mod=false;
        }
        if(other_mod&&now<=tot&&(sys.wHour>L[now].from.hour||(sys.wHour==L[now].from.hour&&sys.wMinute>=L[now].from.minute)))//当前科目已开始
        {
            other_mod=false;
        }
        if(now>tot||(sys.wHour<L[now].from.hour||(sys.wHour==L[now].from.hour&&sys.wMinute<L[now].from.minute)))//当前科目未开始
        {
            other_mod=true;
        }
        if(desktop) TraceDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        else TraceDir(input);//检测文件更新
        Sleep(sleep);//等待下一更新时间
        fout.close();//更新日志
        fout.open("log.txt",std::ios::app);
        if(save_hash)//写入HASH到文件
        {
            write_hash.close();
            write_hash.open("hash.txt",std::ios::app);
        }
    }
    fout.close();
    return 0;
    //return a.exec();
}
