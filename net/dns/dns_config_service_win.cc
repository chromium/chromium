// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <sysinfoapi.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/serial_worker.h"
#include "url/url_canon.h"

namespace net {

namespace internal {

namespace {

// Registry key paths.
const wchar_t kTcpipPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";
const wchar_t kTcpip6Path[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters";
const wchar_t kDnscachePath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters";
const wchar_t kPolicyPath[] =
    L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DnsWindowsCompatibility {
  kCompatible = 0,
  kIncompatibleResolutionPolicy = 1,
  kIncompatibleProxy = 1 << 1,
  kIncompatibleVpn = 1 << 2,
  kIncompatibleAdapterSpecificNameserver = 1 << 3,

  KAllIncompatibleFlags = (1 << 4) - 1,
  kMaxValue = KAllIncompatibleFlags
};

inline constexpr DnsWindowsCompatibility operator|(DnsWindowsCompatibility a,
                                                   DnsWindowsCompatibility b) {
  return static_cast<DnsWindowsCompatibility>(static_cast<int>(a) |
                                              static_cast<int>(b));
}

inline DnsWindowsCompatibility& operator|=(DnsWindowsCompatibility& a,
                                           DnsWindowsCompatibility b) {
  return a = a | b;
}

// Wrapper for GetAdaptersAddresses to get unicast addresses.
// Returns nullptr if failed.
std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter>
ReadAdapterUnicastAddresses() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> out;
  ULONG len = 15000;  // As recommended by MSDN for GetAdaptersAddresses.
  UINT rv = ERROR_BUFFER_OVERFLOW;
  // Try up to three times.
  for (unsigned tries = 0; (tries < 3) && (rv == ERROR_BUFFER_OVERFLOW);
       tries++) {
    out.reset(static_cast<PIP_ADAPTER_ADDRESSES>(malloc(len)));
    memset(out.get(), 0, len);
    rv = GetAdaptersAddresses(AF_UNSPEC,
                              GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER |
                              GAA_FLAG_SKIP_MULTICAST |
                              GAA_FLAG_SKIP_FRIENDLY_NAME,
                              nullptr, out.get(), &len);
  }
  if (rv != NO_ERROR)
    out.reset();
  return out;
}

// Default address of "localhost" and local computer name can be overridden
// by the HOSTS file, but if it's not there, then we need to fill it in.
bool AddLocalhostEntriesTo(DnsHosts& in_out_hosts) {
  IPAddress loopback_ipv4 = IPAddress::IPv4Localhost();
  IPAddress loopback_ipv6 = IPAddress::IPv6Localhost();

  // This does not override any pre-existing entries from the HOSTS file.
  in_out_hosts.emplace(DnsHostsKey("localhost", ADDRESS_FAMILY_IPV4),
                       loopback_ipv4);
  in_out_hosts.emplace(DnsHostsKey("localhost", ADDRESS_FAMILY_IPV6),
                       loopback_ipv6);

  wchar_t buffer[MAX_PATH];
  DWORD size = MAX_PATH;
  if (!GetComputerNameExW(ComputerNameDnsHostname, buffer, &size))
    return false;
  std::string localname = ParseDomainASCII(buffer);
  if (localname.empty())
    return false;
  localname = base::ToLowerASCII(localname);

  bool have_ipv4 =
      in_out_hosts.count(DnsHostsKey(localname, ADDRESS_FAMILY_IPV4)) > 0;
  bool have_ipv6 =
      in_out_hosts.count(DnsHostsKey(localname, ADDRESS_FAMILY_IPV6)) > 0;

  if (have_ipv4 && have_ipv6)
    return true;

  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> addresses =
      ReadAdapterUnicastAddresses();
  if (!addresses.get())
    return false;

  // The order of adapters is the network binding order, so stick to the
  // first good adapter for each family.
  for (const IP_ADAPTER_ADDRESSES* adapter = addresses.get();
       adapter != nullptr && (!have_ipv4 || !have_ipv6);
       adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp)
      continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;

    for (const IP_ADAPTER_UNICAST_ADDRESS* address =
             adapter->FirstUnicastAddress;
         address != nullptr; address = address->Next) {
      IPEndPoint ipe;
      if (!ipe.FromSockAddr(address->Address.lpSockaddr,
                            address->Address.iSockaddrLength)) {
        return false;
      }
      if (!have_ipv4 && (ipe.GetFamily() == ADDRESS_FAMILY_IPV4)) {
        have_ipv4 = true;
        in_out_hosts[DnsHostsKey(localname, ADDRESS_FAMILY_IPV4)] =
            ipe.address();
      } else if (!have_ipv6 && (ipe.GetFamily() == ADDRESS_FAMILY_IPV6)) {
        have_ipv6 = true;
        in_out_hosts[DnsHostsKey(localname, ADDRESS_FAMILY_IPV6)] =
            ipe.address();
      }
    }
  }
  return true;
}

// Watches a single registry key for changes.
class RegistryWatcher {
 public:
  typedef base::RepeatingCallback<void(bool succeeded)> CallbackType;
  RegistryWatcher() {}

  RegistryWatcher(const RegistryWatcher&) = delete;
  RegistryWatcher& operator=(const RegistryWatcher&) = delete;

  ~RegistryWatcher() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  bool Watch(const wchar_t key[], const CallbackType& callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    callback_ = callback;
    if (key_.Open(HKEY_LOCAL_MACHINE, key, KEY_NOTIFY) != ERROR_SUCCESS)
      return false;

    return key_.StartWatching(base::BindOnce(&RegistryWatcher::OnObjectSignaled,
                                             base::Unretained(this)));
  }

  void OnObjectSignaled() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!callback_.is_null());
    if (key_.StartWatching(base::BindOnce(&RegistryWatcher::OnObjectSignaled,
                                          base::Unretained(this)))) {
      callback_.Run(true);
    } else {
      key_.Close();
      callback_.Run(false);
    }
  }

 private:
  CallbackType callback_;
  base::win::RegKey key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Returns the path to the HOSTS file.
base::FilePath GetHostsPath() {
  wchar_t buffer[MAX_PATH];
  UINT rc = GetSystemDirectory(buffer, MAX_PATH);
  DCHECK(0 < rc && rc < MAX_PATH);
  return base::FilePath(buffer).Append(
      FILE_PATH_LITERAL("drivers\\etc\\hosts"));
}

void ConfigureSuffixSearch(const WinDnsSystemSettings& settings,
                           DnsConfig& in_out_config) {
  // SearchList takes precedence, so check it first.
  if (settings.policy_search_list.has_value()) {
    std::vector<std::string> search =
        ParseSearchList(settings.policy_search_list.value());
    if (!search.empty()) {
      in_out_config.search = std::move(search);
      return;
    }
    // Even if invalid, the policy disables the user-specified setting below.
  } else if (settings.tcpip_search_list.has_value()) {
    std::vector<std::string> search =
        ParseSearchList(settings.tcpip_search_list.value());
    if (!search.empty()) {
      in_out_config.search = std::move(search);
      return;
    }
  }

  // In absence of explicit search list, suffix search is:
  // [primary suffix, connection-specific suffix, devolution of primary suffix].
  // Primary suffix can be set by policy (primary_dns_suffix) or
  // user setting (tcpip_domain).
  //
  // The policy (primary_dns_suffix) can be edited via Group Policy Editor
  // (gpedit.msc) at Local Computer Policy => Computer Configuration
  // => Administrative Template => Network => DNS Client => Primary DNS Suffix.
  //
  // The user setting (tcpip_domain) can be configurred at Computer Name in
  // System Settings
  std::string primary_suffix;
  if (settings.primary_dns_suffix.has_value())
    primary_suffix = ParseDomainASCII(settings.primary_dns_suffix.value());
  if (primary_suffix.empty() && settings.tcpip_domain.has_value())
    primary_suffix = ParseDomainASCII(settings.tcpip_domain.value());
  if (primary_suffix.empty())
    return;  // No primary suffix, hence no devolution.
  // Primary suffix goes in front.
  in_out_config.search.insert(in_out_config.search.begin(), primary_suffix);

  // Devolution is determined by precedence: policy > dnscache > tcpip.
  // |enabled|: UseDomainNameDevolution and |level|: DomainNameDevolutionLevel
  // are overridden independently.
  WinDnsSystemSettings::DevolutionSetting devolution =
      settings.policy_devolution;

  if (!devolution.enabled.has_value())
    devolution.enabled = settings.dnscache_devolution.enabled;
  if (!devolution.enabled.has_value())
    devolution.enabled = settings.tcpip_devolution.enabled;
  if (devolution.enabled.has_value() && (devolution.enabled.value() == 0))
    return;  // Devolution disabled.

  // By default devolution is enabled.

  if (!devolution.level.has_value())
    devolution.level = settings.dnscache_devolution.level;
  if (!devolution.level.has_value())
    devolution.level = settings.tcpip_devolution.level;

  // After the recent update, Windows will try to determine a safe default
  // value by comparing the forest root domain (FRD) to the primary suffix.
  // See http://support.microsoft.com/kb/957579 for details.
  // For now, if the level is not set, we disable devolution, assuming that
  // we will fallback to the system getaddrinfo anyway. This might cause
  // performance loss for resolutions which depend on the system default
  // devolution setting.
  //
  // If the level is explicitly set below 2, devolution is disabled.
  if (!devolution.level.has_value() || devolution.level.value() < 2)
    return;  // Devolution disabled.

  // Devolve the primary suffix. This naive logic matches the observed
  // behavior (see also ParseSearchList). If a suffix is not valid, it will be
  // discarded when the fully-qualified name is converted to DNS format.

  unsigned num_dots = base::ranges::count(primary_suffix, '.');

  for (size_t offset = 0; num_dots >= devolution.level.value(); --num_dots) {
    offset = primary_suffix.find('.', offset + 1);
    in_out_config.search.push_back(primary_suffix.substr(offset + 1));
  }
}

std::optional<std::vector<IPEndPoint>> GetNameServers(
    const IP_ADAPTER_ADDRESSES* adapter) {
  std::vector<IPEndPoint> nameservers;
  for (const IP_ADAPTER_DNS_SERVER_ADDRESS* address =
           adapter->FirstDnsServerAddress;
       address != nullptr; address = address->Next) {
    IPEndPoint ipe;
    if (ipe.FromSockAddr(address->Address.lpSockaddr,
                         address->Address.iSockaddrLength)) {
      if (WinDnsSystemSettings::IsStatelessDiscoveryAddress(ipe.address()))
        continue;
      // Override unset port.
      if (!ipe.port())
        ipe = IPEndPoint(ipe.address(), dns_protocol::kDefaultPort);
      nameservers.push_back(ipe);
    } else {
      return std::nullopt;
    }
  }
  return nameservers;
}

bool CheckAndRecordCompatibility(bool have_name_resolution_policy,
                                 bool have_proxy,
                                 bool uses_vpn,
                                 bool has_adapter_specific_nameservers) {
  DnsWindowsCompatibility compatibility = DnsWindowsCompatibility::kCompatible;
  if (have_name_resolution_policy)
    compatibility |= DnsWindowsCompatibility::kIncompatibleResolutionPolicy;
  if (have_proxy)
    compatibility |= DnsWindowsCompatibility::kIncompatibleProxy;
  if (uses_vpn)
    compatibility |= DnsWindowsCompatibility::kIncompatibleVpn;
  if (has_adapter_specific_nameservers) {
    compatibility |=
        DnsWindowsCompatibility::kIncompatibleAdapterSpecificNameserver;
  }
  base::UmaHistogramEnumeration("Net.DNS.DnsConfig.Windows.Compatibility",
                                compatibility);
  return compatibility == DnsWindowsCompatibility::kCompatible;
}

}  // namespace

std::string ParseDomainASCII(std::wstring_view widestr) {
  if (widestr.empty())
    return "";

  // Check if already ASCII.
  if (base::IsStringASCII(base::AsStringPiece16(widestr))) {
    return std::string(widestr.begin(), widestr.end());
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url::RawCanonOutputT<char16_t, kInitialBufferSize> punycode;
  if (!url::IDNToASCII(base::AsStringPiece16(widestr), &punycode)) {
    return "";
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  std::string converted;
  bool success =
      base::UTF16ToUTF8(punycode.data(), punycode.length(), &converted);
  DCHECK(success);
  DCHECK(base::IsStringASCII(converted));
  return converted;
}

std::vector<std::string> ParseSearchList(std::wstring_view value) {
  if (value.empty())
    return {};

  std::vector<std::string> output;

  // If the list includes an empty hostname (",," or ", ,"), it is terminated.
  // Although nslookup and network connection property tab ignore such
  // fragments ("a,b,,c" becomes ["a", "b", "c"]), our reference is getaddrinfo
  // (which sees ["a", "b"]). WMI queries also return a matching search list.
  for (std::wstring_view t : base::SplitStringPiece(
           value, L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Convert non-ASCII to punycode, although getaddrinfo does not properly
    // handle such suffixes.
    std::string parsed = ParseDomainASCII(t);
    if (parsed.empty())
      break;
    output.push_back(std::move(parsed));
  }
  return output;
}

base::expected<DnsConfig, ReadWinSystemDnsSettingsError>
ConvertSettingsToDnsConfig(
    const base::expected<WinDnsSystemSettings, ReadWinSystemDnsSettingsError>&
        settings_or_error) {
  if (!settings_or_error.has_value()) {
    return base::unexpected(settings_or_error.error());
  }
  const WinDnsSystemSettings& settings = *settings_or_error;
  bool uses_vpn = false;
  bool has_adapter_specific_nameservers = false;

  DnsConfig dns_config;

  std::set<IPEndPoint> previous_nameservers_set;

  // Use GetAdapterAddresses to get effective DNS server order and
  // connection-specific DNS suffix. Ignore disconnected and loopback adapters.
  // The order of adapters is the network binding order, so stick to the
  // first good adapter.
  for (const IP_ADAPTER_ADDRESSES* adapter = settings.addresses.get();
       adapter != nullptr; adapter = adapter->Next) {
    // Check each adapter for a VPN interface. Even if a single such interface
    // is present, treat this as an unhandled configuration.
    if (adapter->IfType == IF_TYPE_PPP) {
      uses_vpn = true;
    }

    std::optional<std::vector<IPEndPoint>> nameservers =
        GetNameServers(adapter);
    if (!nameservers) {
      return base::unexpected(
          ReadWinSystemDnsSettingsError::kGetNameServersFailed);
    }

    if (!nameservers->empty() && (adapter->OperStatus == IfOperStatusUp)) {
      // Check if the |adapter| has adapter specific nameservers.
      std::set<IPEndPoint> nameservers_set(nameservers->begin(),
                                           nameservers->end());
      if (!previous_nameservers_set.empty() &&
          (previous_nameservers_set != nameservers_set)) {
        has_adapter_specific_nameservers = true;
      }
      previous_nameservers_set = std::move(nameservers_set);
    }

    // Skip disconnected and loopback adapters. If a good configuration was
    // previously found, skip processing another adapter.
    if (adapter->OperStatus != IfOperStatusUp ||
        adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
        !dns_config.nameservers.empty())
      continue;

    dns_config.nameservers = std::move(*nameservers);

    // IP_ADAPTER_ADDRESSES in Vista+ has a search list at |FirstDnsSuffix|,
    // but it came up empty in all trials.
    // |DnsSuffix| stores the effective connection-specific suffix, which is
    // obtained via DHCP (regkey: Tcpip\Parameters\Interfaces\{XXX}\DhcpDomain)
    // or specified by the user (regkey: Tcpip\Parameters\Domain).
    std::string dns_suffix = ParseDomainASCII(adapter->DnsSuffix);
    if (!dns_suffix.empty())
      dns_config.search.push_back(std::move(dns_suffix));
  }

  if (dns_config.nameservers.empty()) {
    return base::unexpected(ReadWinSystemDnsSettingsError::kNoNameServerFound);
  }

  // Windows always tries a multi-label name "as is" before using suffixes.
  dns_config.ndots = 1;

  if (!settings.append_to_multi_label_name.has_value()) {
    dns_config.append_to_multi_label_name = false;
  } else {
    dns_config.append_to_multi_label_name =
        (settings.append_to_multi_label_name.value() != 0);
  }

  if (settings.have_name_resolution_policy) {
    // TODO(szym): only set this to true if NRPT has DirectAccess rules.
    dns_config.use_local_ipv6 = true;
  }

  if (!CheckAndRecordCompatibility(settings.have_name_resolution_policy,
                                   settings.have_proxy, uses_vpn,
                                   has_adapter_specific_nameservers)) {
    dns_config.unhandled_options = true;
  }

  ConfigureSuffixSearch(settings, dns_config);
  return dns_config;
}

// Watches registry and HOSTS file for changes. Must live on a sequence which
// allows IO.
class DnsConfigServiceWin::Watcher
    : public NetworkChangeNotifier::IPAddressObserver,
      public DnsConfigService::Watcher {
 public:
  explicit Watcher(DnsConfigServiceWin& service)
      : DnsConfigService::Watcher(service) {}

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  ~Watcher() override { NetworkChangeNotifier::RemoveIPAddressObserver(this); }

  bool Watch() override {
    CheckOnCorrectSequence();

    RegistryWatcher::CallbackType callback =
        base::BindRepeating(&Watcher::OnConfigChanged, base::Unretained(this));

    bool success = true;

    // The Tcpip key must be present.
    if (!tcpip_watcher_.Watch(kTcpipPath, callback)) {
      LOG(ERROR) << "DNS registry watch failed to start.";
      success = false;
    }

    // Watch for IPv6 nameservers.
    tcpip6_watcher_.Watch(kTcpip6Path, callback);

    // DNS suffix search list and devolution can be configured via group
    // policy which sets this registry key. If the key is missing, the policy
    // does not apply, and the DNS client uses Tcpip and Dnscache settings.
    // If a policy is installed, DnsConfigService will need to be restarted.
    // BUG=99509

    dnscache_watcher_.Watch(kDnscachePath, callback);
    policy_watcher_.Watch(kPolicyPath, callback);

    if (!hosts_watcher_.Watch(
            GetHostsPath(), base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(&Watcher::OnHostsFilePathWatcherChange,
                                base::Unretained(this)))) {
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    } else {
      // Also need to observe changes to local non-loopback IP for DnsHosts.
      NetworkChangeNotifier::AddIPAddressObserver(this);
    }
    return success;
  }

 private:
  void OnHostsFilePathWatcherChange(const base::FilePath& path, bool error) {
    if (error)
      NetworkChangeNotifier::RemoveIPAddressObserver(this);
    OnHostsChanged(!error);
  }

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override {
    // Need to update non-loopback IP of local host.
    OnHostsChanged(true);
  }

  RegistryWatcher tcpip_watcher_;
  RegistryWatcher tcpip6_watcher_;
  RegistryWatcher dnscache_watcher_;
  RegistryWatcher policy_watcher_;
  base::FilePathWatcher hosts_watcher_;
};

// Reads config from registry and IpHelper. All work performed in ThreadPool.
class DnsConfigServiceWin::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServiceWin& service)
      : SerialWorker(/*max_number_of_retries=*/3), service_(&service) {}
  ~ConfigReader() override {}

  // SerialWorker::
  std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
    return std::make_unique<WorkItem>();
  }

  bool OnWorkFinished(std::unique_ptr<SerialWorker::WorkItem>
                          serial_worker_work_item) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(serial_worker_work_item);
    DCHECK(!IsCancelled());

    WorkItem* work_item = static_cast<WorkItem*>(serial_worker_work_item.get());
    base::UmaHistogramEnumeration(
        base::StrCat({"Net.DNS.DnsConfig.Windows.ReadSystemSettings",
                      base::NumberToString(GetFailureCount())}),
        work_item->dns_config_or_error_.has_value()
            ? ReadWinSystemDnsSettingsError::kOk
            : work_item->dns_config_or_error_.error());

    if (work_item->dns_config_or_error_.has_value()) {
      service_->OnConfigRead(
          std::move(work_item->dns_config_or_error_).value());
      return true;
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
      return false;
    }
  }

 private:
  class WorkItem : public SerialWorker::WorkItem {
   public:
    ~WorkItem() override = default;

    void DoWork() override {
      dns_config_or_error_ =
          ConvertSettingsToDnsConfig(ReadWinSystemDnsSettings());
    }

   private:
    friend DnsConfigServiceWin::ConfigReader;
    base::expected<DnsConfig, ReadWinSystemDnsSettingsError>
        dns_config_or_error_;
  };

  raw_ptr<DnsConfigServiceWin> service_;
  // Written in DoWork(), read in OnWorkFinished(). No locking required.
};

// Extension of DnsConfigService::HostsReader that fills in localhost and local
// computer name if necessary.
class DnsConfigServiceWin::HostsReader : public DnsConfigService::HostsReader {
 public:
  explicit HostsReader(DnsConfigServiceWin& service)
      : DnsConfigService::HostsReader(GetHostsPath().value(), service) {}

  ~HostsReader() override = default;

  HostsReader(const HostsReader&) = delete;
  HostsReader& operator=(const HostsReader&) = delete;

  // SerialWorker:
  std::unique_ptr<SerialWorker::WorkItem> CreateWorkItem() override {
    return std::make_unique<WorkItem>(GetHostsPath());
  }

 private:
  class WorkItem : public DnsConfigService::HostsReader::WorkItem {
   public:
    explicit WorkItem(base::FilePath hosts_file_path)
        : DnsConfigService::HostsReader::WorkItem(
              std::make_unique<DnsHostsFileParser>(
                  std::move(hosts_file_path))) {}

    ~WorkItem() override = default;

    bool AddAdditionalHostsTo(DnsHosts& in_out_dns_hosts) override {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      return AddLocalhostEntriesTo(in_out_dns_hosts);
    }
  };
};

DnsConfigServiceWin::DnsConfigServiceWin()
    : DnsConfigService(GetHostsPath().value(),
                       std::nullopt /* config_change_delay */) {
  // Allow constructing on one sequence and living on another.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigServiceWin::~DnsConfigServiceWin() {
  if (config_reader_)
    config_reader_->Cancel();
  if (hosts_reader_)
    hosts_reader_->Cancel();
}

void DnsConfigServiceWin::ReadConfigNow() {
  if (!config_reader_)
    config_reader_ = std::make_unique<ConfigReader>(*this);
  config_reader_->WorkNow();
}

void DnsConfigServiceWin::ReadHostsNow() {
  if (!hosts_reader_)
    hosts_reader_ = std::make_unique<HostsReader>(*this);
  hosts_reader_->WorkNow();
}

bool DnsConfigServiceWin::StartWatching() {
  DCHECK(!watcher_);
  // TODO(szym): re-start watcher if that makes sense. http://crbug.com/116139
  watcher_ = std::make_unique<Watcher>(*this);
  return watcher_->Watch();
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceWin>();
}

}  // namespace net
