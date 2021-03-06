//#pragma comment(lib,"iphlpapi.lib")
//#pragma comment(lib,"wsock32.lib")
#include "windowutils.h"
#include <windows.h>
#include <Winsock2.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <QNetworkInterface>
#include <QSettings>
#include "PING.h"
#include <IPTypes.h>
#include "pcap.h"
#include "utils.h"


QString GetNICUuidByHumanReadableName(const QString& HumanReadableName)
{
	if (HumanReadableName.isEmpty()) return "";

	QString UUID;
	QString LocalAdpterName(HumanReadableName);

	foreach(QNetworkInterface nif, QNetworkInterface::allInterfaces()){

		if (!nif.isValid() || nif.humanReadableName() != LocalAdpterName) 
			continue;

		UUID = nif.name();
	}

	return UUID;
}

QString ConvertNICUUIDtoPcapName(pcap_if_t* devs, const QString& uuid)
{
	QString pcap_name;
	
	for (pcap_if_t *d = devs; d && d->next; d = d->next)
	{
		QString tmp(d->name);
		if (tmp.contains(uuid, Qt::CaseInsensitive))
			pcap_name = tmp;
	}

	return pcap_name;
}

WindowUtils::WindowUtils()
{
}


bool WindowUtils::isValidNetMacaddress(const QString& macaddress)
{
    return !macaddress.isEmpty() && macaddress != "00:00:00:00:00:00:00:E0"
            && !macaddress.startsWith("00:50:56:C0");
}

void WindowUtils::GetIPfromLocalNIC(std::vector<QString> &IPs)
{
	qDebug() << "GetIPfromLocalNIC start";
	getLocalIPs(IPs);
	qDebug() << "GetIPfromLocalNIC end";
}

void WindowUtils::getLocalIPs(std::vector<QString> &IPs)
{
    getLocalIPs(WindowUtils::getLoacalNetName(), IPs);
}

void WindowUtils::getLocalIPs(const QString& sHostName, std::vector<QString> &IPs){
    IPs.clear();
    QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
    foreach(QNetworkInterface i, list) {
        if (i.isValid() && isValidNetMacaddress(i.hardwareAddress()) &&
            i.humanReadableName() == WindowUtils::getLoacalNetName())
        {
            foreach(QNetworkAddressEntry ae, i.addressEntries())
            {
                auto address = ae.ip();
                if (address.protocol() == QAbstractSocket::IPv4Protocol
                    && !address.isLoopback() && Utils::isInnerIP(address.toString()))
                {
                    
                    if (IPs.end() == std::find(IPs.begin(), IPs.end(), address.toString()))
                    {
                        IPs.push_back(address.toString());
                        qDebug() <<__FILE__ << __FUNCTION__ << __LINE__ << address.toString() << i.humanReadableName();
                    }
                }
            }
        }
        qDebug() << __FUNCTION__ << __LINE__;
    }

    qDebug() << __FUNCTION__ << __LINE__;
}

BOOL FindProcessByName(const char* szFileName, PROCESSENTRY32& pe)
{
    // 采用进程快照枚举进程的方法查找指定名称进程
    HANDLE hProcesses;
    PROCESSENTRY32 lpe =
    {
        sizeof(PROCESSENTRY32)
    };

    QString sFileName(QString::fromLocal8Bit(szFileName).toLower());

    // 创建进程快照
    hProcesses = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcesses == INVALID_HANDLE_VALUE)
        return FALSE;
    // 获取第一个进程实例
    BOOL isExist = ::Process32First(hProcesses, &lpe);
    BOOL isRunning = FALSE;
    QString strName;
    while (isExist)
    {
        strName = QString::fromWCharArray(lpe.szExeFile).toLower();
        if (sFileName.indexOf(strName) >= 0)
        {
            isRunning = TRUE;
            break;
        }
        // 遍历下一个进程实例
        isExist = ::Process32Next(hProcesses, &lpe);
    }

    if (isRunning)
    {
        memcpy(&pe, &lpe, sizeof(PROCESSENTRY32));
    }

    CloseHandle(hProcesses);

    return isRunning;
}

bool WindowUtils::findProcessByName(const char* szFileName){
    PROCESSENTRY32 pe;
    return FindProcessByName(szFileName, pe);
}

void WindowUtils::terminateProcess(const char* szFileName){
    HANDLE hProcesses;
    PROCESSENTRY32 lpe =
    {
        sizeof(PROCESSENTRY32)
    };

    QString sFileName(QString::fromLocal8Bit(szFileName).toLower());

    // 创建进程快照
    hProcesses = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcesses == INVALID_HANDLE_VALUE)
    {
        qDebug() << "INVALID_HANDLE_VALUE";
        return;
    }
       
    // 获取第一个进程实例
    BOOL isExist = ::Process32First(hProcesses, &lpe);
    BOOL isRunning = FALSE;
    QString strName;
    while (isExist)
    {
        strName = QString::fromWCharArray(lpe.szExeFile).toLower();
        if (strName.indexOf(sFileName) >= 0)
        {
            HANDLE h = OpenProcess(1, TRUE, lpe.th32ProcessID);   //取进程实例 PROCESS_TERMINATE
            TerminateProcess(h, 0);
        }
        // 遍历下一个进程实例
        isExist = ::Process32Next(hProcesses, &lpe);
    }

    CloseHandle(hProcesses);
}

void WindowUtils::copy(const QFileInfo& source, const QDir& dest, const QString& destName){
    if (!source.exists())
    {
        return;
    }

    QString destFile;
    if (destName.isEmpty())
    {
        destFile = dest.path() + "/" + source.fileName();
    }
    else{
        destFile = dest.path() + "/" + destName;
    }

    if (!dest.exists()){
        dest.mkpath(dest.path());
    }
   
    if (source.isFile())
    {
        CopyFileA(source.filePath().toLocal8Bit().data(),
            destFile.toLocal8Bit().data(), false);
    }
    else if (source.isDir())
    {
        QFileInfoList list = QDir(source.absoluteFilePath()).entryInfoList();
        for (int i = 0; i < list.size(); ++i) {
            const QFileInfo& fileInfo = list.at(i);
            qDebug() << fileInfo.fileName();
            if (!fileInfo.fileName().startsWith("."))
            {
                WindowUtils::copy(fileInfo, QDir(destFile));
            }
        }
    }
}
bool WindowUtils::setNetConfig(const QString& sName, const QString& sIP, const QString& sMask, const QString& sGate, bool bWait, std::shared_ptr<bool> bpCancel){
    QString mask = QString("mask=%1").arg(sMask);
    QString name = QString("name=\"%1\"").arg(sName);
    QString addr = QString("addr=%1").arg(sIP);
    QStringList argList = QStringList() << "interface" << "ip" << "set" << "address" << name << "source=static" << addr << mask;
    if (bpCancel && *bpCancel)
    {
        return false;
    }
    if (!sGate.isEmpty())
    {
        QString gateway = QString("gateway=%1").arg(sGate);
        QProcess::execute("netsh", QStringList() << "interface" << "ip" << "set" << "address" << name << "source=static" << addr << mask << gateway);
    }
    else{
        QProcess::execute("netsh", QStringList() << "interface" << "ip" << "set" << "address" << name << "source=static" << addr << mask );
    }
    if (!bWait)
    {
        return true;
    }
    int maxPingTime = 1000 * 3;
    WindowUtils:sleep(1000, bpCancel);
    if (bpCancel && *bpCancel)
    {
        return false;
    }
    while (maxPingTime > 0 && !CPing::instance().Ping(sIP.toStdString().c_str(), 20)){
        WindowUtils::sleep(1000, bpCancel);
        if (bpCancel && *bpCancel)
        {
            return false;
        }
        maxPingTime -= 1000;
    }
    
    return maxPingTime > 0;
}

bool WindowUtils::setNetDhcp(const QString& sName){
    QString name = QString("name=\"%1\"").arg(sName);
    QProcess::execute("netsh", QStringList() << "interface" << "ip" << "set" << "address" << name << "source=dhcp");
    
    return true;
}

bool WindowUtils::isConnecteTo(const QString& IP, int millSeconds){
    return CPing::instance().Ping(IP.toStdString().c_str(), millSeconds);
}


typedef struct arppkt
{
    unsigned short hdtyp;//硬件类型
    unsigned short protyp;//协议类型
    unsigned char hdsize;//硬件地址长度
    unsigned char prosize;//协议地址长度
    unsigned short op;//（操作类型）操作值:ARP/RARP
    u_char smac[6];//源MAC地址
    u_char sip[4];//源IP地址
    u_char dmac[6];//目的MAC地址
    u_char dip[4];//目的IP地址
}arpp;


#define  INNDER_SPECIAL_IP "170.151.24.203"
bool WindowUtils::getDirectDevice(QString& ip, QString& netGate, std::shared_ptr<bool> bpCancel)
{
    qDebug() << __FUNCTION__ << __LINE__;
    ip.clear();
    struct tm * timeinfo;
    struct tm *ltime;
    time_t rawtime;

    int result;
    int i = 0, inum;
    pcap_if_t * alldevs;//指向pcap_if_t结构列表指针
    pcap_t * adhandle;//定义包捕捉句柄
    char errbuf[PCAP_ERRBUF_SIZE];//错误缓冲最小为256
    u_int netmask; //定义子网掩码
    char packet_filter[] = "ether proto \\arp";
    struct bpf_program fcode;
    struct pcap_pkthdr * header;
    const u_char * pkt_data;
    //打开日志文件
    if (bpCancel && *bpCancel)
    {
        return false;
    }
    //当前所有可用的网络设备
    if (pcap_findalldevs(&alldevs, errbuf) == -1 || (bpCancel && *bpCancel))
    {
        return false;
    }
    
    if (!alldevs)
    {
        qDebug() << "cannot find net device!  install WinPcap?";
        return false;
    }

    if ((adhandle = pcap_open_live(alldevs->name, 65536, 1, 1000, errbuf)) == NULL || (bpCancel && *bpCancel))
    {
        pcap_freealldevs(alldevs);
        return false;
    }

    if (pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL || (bpCancel && *bpCancel)) {
        return false;
    }

    netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
    pcap_freealldevs(alldevs);

    if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) < 0 || (bpCancel && *bpCancel))
    {
        return false;
    }

    if (pcap_setfilter(adhandle, &fcode) < 0 || (bpCancel && *bpCancel))
    {
        return false;
    }

    std::vector<QString> IPs;
    getLocalIPs(IPs);
    const int nMaxSeconds = 30;
    int start = GetTickCount();
    QString specialIP;
    while (true)
    {
        if (bpCancel && *bpCancel)
        {
            return false;
        }
        if (GetTickCount() - start > 30 * 1000)
        {
            qDebug() << __FUNCTION__ << __LINE__ << "arp time out";
            break;
        }
        //循环解析ARP数据包
        if (pcap_next_ex(adhandle, &header, &pkt_data) == 0){
            continue;
        }
        arpp* arph = (arpp *)(pkt_data + 14);
        if (arph->op == 256) //arp
        {
            if (arph->sip[0] == 0)
            {
                continue;
            }
            if (arph->sip[0] == 169 && arph->sip[1] == 254)
            {
                continue;
            }

            QString source = QString("%1.%2.%3.%4").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]).arg(arph->sip[3]);
            qDebug() << __FUNCTION__ << __LINE__ << "arp" << source;
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }
            netGate = source;
            if (ip.isEmpty())
            {
                start = GetTickCount() - 29 * 1000;
            }

            ip = QString("%1.%2.%3.%4").arg(arph->dip[0]).arg(arph->dip[1]).arg(arph->dip[2]).arg(arph->dip[3]);
            if (arph->dip[0] != arph->sip[0] || arph->dip[1] != arph->sip[1] || arph->dip[2] != arph->sip[2] || ip == netGate)
            {
                if (arph->sip[3] != 44)
                {
                    ip = QString("%1.%2.%3.44").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
                }
                else{
                    ip = QString("%1.%2.%3.254").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
                }
            }

            qDebug() <<__FUNCTION__ << __LINE__ << "arp" << ip << netGate;
        }
        else
        {
            if (arph->dip[0] == 0)
            {
                continue;
            }
            if (arph->dip[0] == 169 && arph->dip[1] == 254)
            {
                continue;
            }

            QString source  = QString("%1.%2.%3.%4").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]).arg(arph->sip[3]);
            qDebug() << __FUNCTION__ << __LINE__ << "arp" << source;
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }

            if (!Utils::isInnerIP(source))
            {
                continue;
            }

            netGate = source;
            if (arph->sip[3] != 44)
            {
                ip = QString("%1.%2.%3.44").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
            }
            else{
                ip = QString("%1.%2.%3.254").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
            }
            qDebug() << __FUNCTION__ << __LINE__ << "aarp" << ip << netGate;
            break;
        }

    }

    return !ip.isEmpty();
}

bool WindowUtils::getDirectDevice(QString& ip, QString& netGate, std::set<QString>& otherIPS, int secondsWait, std::shared_ptr<bool> bpCancel){
    if (bpCancel && *bpCancel)
    {
        return false;
    }
    ip.clear();
    struct tm * timeinfo;
    struct tm *ltime;
    time_t rawtime;

    int result;
    int i = 0, inum;
    pcap_if_t * alldevs;//指向pcap_if_t结构列表指针
    pcap_t * adhandle;//定义包捕捉句柄
    char errbuf[PCAP_ERRBUF_SIZE];//错误缓冲最小为256
    u_int netmask; //定义子网掩码
    char packet_filter[] = "ether proto \\arp";
    struct bpf_program fcode;
    struct pcap_pkthdr * header;
    const u_char * pkt_data;

    //当前所有可用的网络设备
    if (pcap_findalldevs(&alldevs, errbuf) == -1 || (bpCancel && *bpCancel))
    {
        return false;
    }

    if (!alldevs)
    {
        return false;
    }

	QString uuid = GetNICUuidByHumanReadableName(WindowUtils::getLoacalNetName());
	QString pcap_name = ConvertNICUUIDtoPcapName(alldevs, uuid);

    if ((adhandle = pcap_open_live(pcap_name.toStdString().data(), 65536, 1, 1000, errbuf)) == NULL || (bpCancel && *bpCancel))
    {
        pcap_freealldevs(alldevs);
        return false;
    }

    if (pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL || (bpCancel && *bpCancel)) {
        qDebug() << "kevin : pcap_datalink(adhandle) != DLT_EN10MB || alldevs->addresses == NULL";
        return false;
    }


    netmask = ((struct sockaddr_in *)(alldevs->addresses->netmask))->sin_addr.S_un.S_addr;
    pcap_freealldevs(alldevs);
    std::map<QString, std::set<QString>> mpDestSource;

    //编译过滤器，只捕获ARP包
    if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) < 0 || (bpCancel && *bpCancel))
    {
        qDebug() << "unable to compile the packet filter.Check the syntax.";
        return false;
    }

    //设置过滤器
    if (pcap_setfilter(adhandle, &fcode) < 0)
    {
        qDebug() << "Error setting the filter.";
        return false;
    }
    QString specialIP;
    std::vector<QString> IPs;
    getLocalIPs(IPs);
    int start = GetTickCount();
    const int MAX_WAIT_OTHER_IP_SECONDS = 10;
    while (true)
    {
        if (bpCancel && *bpCancel){
            return false;
        }
        if (GetTickCount() - start > secondsWait * 1000)
        {
            qDebug() << "arp time out";
            break;
        }
        //循环解析ARP数据包
        if (pcap_next_ex(adhandle, &header, &pkt_data) == 0){
            continue;
        }

        arpp* arph = (arpp *)(pkt_data + 14);
        if (arph->op == 256) //arp
        {
            if (arph->sip[0] == 0)
            {
                continue;
            }
            if (arph->sip[0] == 169 && arph->sip[1] == 254)
            {
                continue;
            }

            QString source = QString("%1.%2.%3.%4").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]).arg(arph->sip[3]);
            QString destIP = QString("%1.%2.%3.%4").arg(arph->dip[0]).arg(arph->dip[1]).arg(arph->dip[2]).arg(arph->dip[3]);
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (source == INNDER_SPECIAL_IP)
            {
                specialIP = destIP;
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }
            
            if (netGate.isEmpty())
            {
                netGate = source;
            }
            
            if (ip.isEmpty() && secondsWait * 1000 - GetTickCount() + start > MAX_WAIT_OTHER_IP_SECONDS * 1000)
            {
                start = GetTickCount() - (secondsWait - MAX_WAIT_OTHER_IP_SECONDS) * 1000;
                ip = destIP;
                if (arph->dip[0] != arph->sip[0] || arph->dip[1] != arph->sip[1] || arph->dip[2] != arph->sip[2] || ip == netGate)
                {
                    if (arph->sip[3] != 1)
                    {
                        ip = QString("%1.%2.%3.1").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
                    }
                    else{
                        ip = QString("%1.%2.%3.254").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]);
                    }
                }
            }

            
            otherIPS.insert(source);
            if (Utils::isInnerIP(destIP))
            {
                mpDestSource[destIP].insert(source);
            }
            
 
            qDebug() << "arp" << source << destIP;
        }
        else
        {
            if (arph->dip[0] == 0)
            {
                continue;
            }
            if (arph->dip[0] == 169 && arph->dip[1] == 254)
            {
                continue;
            }
            
            QString source = QString("%1.%2.%3.%4").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]).arg(arph->sip[3]);
            QString dest = QString("%1.%2.%3.%4").arg(arph->dip[0]).arg(arph->dip[1]).arg(arph->dip[2]).arg(arph->dip[3]);
            if (IPs.end() != std::find(IPs.begin(), IPs.end(), source)){
                continue;
            }
            if (!Utils::isInnerIP(source))
            {
                continue;
            }

            netGate = source;
            otherIPS.insert(source);
            
            if (ip.isEmpty() && secondsWait * 1000 - GetTickCount() + start > MAX_WAIT_OTHER_IP_SECONDS * 1000)
            {
                start = GetTickCount() - (secondsWait - MAX_WAIT_OTHER_IP_SECONDS) * 1000;
            }

            ip = QString("%1.%2.%3.%4").arg(arph->sip[0]).arg(arph->sip[1]).arg(arph->sip[2]).arg(44);
            qDebug() << "aarp" << source << dest;
           // break;
        }

    }
    
    std::map<QString, std::set<QString>>::iterator itNetgate = mpDestSource.end();
    for (auto it = mpDestSource.begin(); it != mpDestSource.end(); it++)
    {
        if (it->second.size() > 1 &&
            (itNetgate == mpDestSource.end() || itNetgate->second.size() < it->second.size()))
        {
            itNetgate = it;
        }
    }
    if (itNetgate != mpDestSource.end())
    {
        netGate = itNetgate->first;
        QString s = netGate.left(netGate.lastIndexOf(".") + 1);
        for (int i  = 2; i < 255; i++)
        {
            ip = s + QString::number(i);
            if (ip != netGate && itNetgate->second.find(ip) == itNetgate->second.end())
            {
                break;
            }
        }
    }
    else{
        if (netGate.isEmpty() && !specialIP.isEmpty())
        {
            netGate = specialIP;
        }
        if (!netGate.isEmpty())
        {
            netGate = netGate.left(netGate.lastIndexOf(".") + 1) + "1";
            for (int i = 2; i < 255; i++)
            {
                ip = netGate.left(netGate.lastIndexOf(".") + 1) + QString::number(i);
                if (ip != specialIP)
                {
                    break;
                }
            }
        }
    }


    return !ip.isEmpty();
}
bool WindowUtils::setIPByDHCP(QString& ip, QString& mask, QString& netGate, std::shared_ptr<bool> bpCancel){
    qDebug() << __FUNCTION__ << __LINE__;
    QString sName = WindowUtils::getLoacalNetName();
    WindowUtils::setNetDhcp(sName);
    std::vector<QString> ips;
    int maxPingTime = 1000 * 3;
    WindowUtils::sleep(2000, bpCancel);
    for (WindowUtils::getLocalIPs(sName, ips); ips.size() == 0 && maxPingTime > 0; WindowUtils::getLocalIPs(sName, ips)){
        if (bpCancel && *bpCancel)
        {
            return false;
        }

        WindowUtils::sleep(1000, bpCancel);
        maxPingTime -= 1000;
    }
    bool r = false;
    if (ips.size() > 0)
    {
        _IP_ADAPTER_INFO* pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(sizeof(PIP_ADAPTER_INFO));
        ULONG uOutBufLen = sizeof(PIP_ADAPTER_INFO);//存放网卡信息的缓冲区大小
        //第一次调用GetAdapterInfo获取ulOutBufLen大小
        int errorNo = GetAdaptersInfo(pIpAdapterInfo, &uOutBufLen);
        if (errorNo == ERROR_BUFFER_OVERFLOW)
        {
            //获取需要的缓冲区大小
            free(pIpAdapterInfo);
            pIpAdapterInfo = (PIP_ADAPTER_INFO)malloc(uOutBufLen);//分配所需要的内存
            errorNo = (GetAdaptersInfo(pIpAdapterInfo, &uOutBufLen));
        }

        if (NO_ERROR == errorNo)
        {
            _IP_ADAPTER_INFO* pNext = pIpAdapterInfo;
            while (pNext && (!r))
            {
                if (bpCancel && *bpCancel)
                {
                    return false;
                }
                for (IP_ADDR_STRING *pIpAddrString = &(pNext->IpAddressList);
                    pIpAddrString != NULL && (!r); pIpAddrString = pIpAddrString->Next){
                    if (*ips.begin() == pIpAddrString->IpAddress.String)
                    {
//                         if (!WindowUtils::setNetConfig(sName, *ips.begin(), pIpAddrString->IpMask.String, pNext->GatewayList.IpAddress.String, true))
//                         {
//                             break;
//                         }
                        ip = *ips.begin();
                        mask = pIpAddrString->IpMask.String;
                        netGate = pNext->GatewayList.IpAddress.String;
                        r = true;
                        break;
                    }
                }
                pNext = pNext->Next;
            }
        }
        free(pIpAdapterInfo);
    }

    return r;
}
const QString& WindowUtils::getLoacalNetName(){
    static QString localNetName;
    if (localNetName.isEmpty())
    {
         QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
        foreach(QNetworkInterface i, list) {
            if (i.isValid() && isValidNetMacaddress(i.hardwareAddress()))
            {
                qDebug() << i.name() << i.humanReadableName();
                if (i.humanReadableName().contains(QStringLiteral("本地连接")))
                {
                    localNetName = i.humanReadableName();
                    break;
                }
            }

        }
    }

    return localNetName;
}
bool WindowUtils::isOnLine(){
    
    QString localNetName(WindowUtils::getLoacalNetName());
    if (localNetName.isEmpty())
    {
        return false;
    }

    QString name;
    QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
    foreach(QNetworkInterface i, list) {
        if (i.humanReadableName() == localNetName)
        {
            name = i.name();
            break;
        }

    }

    DWORD dwSize = sizeof (MIB_IFTABLE);
    DWORD dwRetVal = 0;

    unsigned int i, j;

    /* variables used for GetIfTable and GetIfEntry */
    char *pIfTable = new char[dwSize];
    MIB_IFROW *pIfRow;

    if (GetIfTable((MIB_IFTABLE *)pIfTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
        delete pIfTable;
        pIfTable = new char[dwSize];
    }
   
    bool r = false;
    if ((dwRetVal = GetIfTable((MIB_IFTABLE *)pIfTable, &dwSize, 0)) == NO_ERROR) {
        MIB_IFTABLE *pTable = (MIB_IFTABLE *)pIfTable;
        if (pTable->dwNumEntries > 0) {
            MIB_IFROW IfRow;
            for (i = 0; i < pTable->dwNumEntries; i++) {
                IfRow.dwIndex = pTable->table[i].dwIndex;
                if ((dwRetVal = GetIfEntry(&IfRow)) == NO_ERROR) {
                                      
                    if (IfRow.dwType != MIB_IF_TYPE_ETHERNET || !QString::fromStdWString(std::wstring(IfRow.wszName)).contains(name))
                    {
                        continue;
                    }
                    
                    switch (IfRow.dwOperStatus) {
                    case IF_OPER_STATUS_NON_OPERATIONAL:
                        break;
                    case IF_OPER_STATUS_UNREACHABLE:
                         break;
                    case IF_OPER_STATUS_DISCONNECTED:
                        break;
                    case IF_OPER_STATUS_CONNECTING:
                        r = true;
                        break;
                    case IF_OPER_STATUS_CONNECTED:
                        r = true;
                        break;
                    case IF_OPER_STATUS_OPERATIONAL:
                         r = true;
                        break;
                    default:
                        qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "Unknown status" << IfRow.dwAdminStatus;
                        break;
                    }
                }

                else {
                    qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "GetIfEntry failed for index with error:" <<
                         dwRetVal;
                }
            }
        }
        else {
            qDebug() << __FILE__ << __FUNCTION__ << __LINE__ << "GetIfTable failed with error:" << dwRetVal;
        }

    }
    delete pIfTable;
    return r;
}

void WindowUtils::disableWindowMsg(){
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\Windows Error Reporting", QSettings::NativeFormat);
    reg.setValue("Disabled", 1);
    reg.setValue("DontShowUI", 1);
}

void WindowUtils::sleep(int milliSeconds, std::shared_ptr<bool> bpCancel){
    if (!bpCancel)
    {
        ::Sleep(milliSeconds);
    }
    else{
        while (milliSeconds > 0 && !*bpCancel){
            ::Sleep(10);
            milliSeconds -= 10;
        }
    }
}