// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/serial_worker.h"
#include "url/url_canon.h"

namespace net {

namespace internal {

namespace {

// Interval between retries to parse config. Used only until parsing succeeds.
const int kRetryIntervalSeconds = 5;

// Registry key paths.
const base::char16 kTcpipPath[] =
    STRING16_LITERAL("SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters");
const base::char16 kTcpip6Path[] =
    STRING16_LITERAL("SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters");
const base::char16 kDnscachePath[] = STRING16_LITERAL(
    "SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters");
const base::char16 kPolicyPath[] =
    STRING16_LITERAL("SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient");
const base::char16 kPrimaryDnsSuffixPath[] =
    STRING16_LITERAL("SOFTWARE\\Policies\\Microsoft\\System\\DNSClient");
const base::char16 kNRPTPath[] = STRING16_LITERAL(
    "SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient\\DnsPolicyConfig");

enum HostsParseWinResult {
  HOSTS_PARSE_WIN_OK = 0,
  HOSTS_PARSE_WIN_UNREADABLE_HOSTS_FILE,
  HOSTS_PARSE_WIN_COMPUTER_NAME_FAILED,
  HOSTS_PARSE_WIN_IPHELPER_FAILED,
  HOSTS_PARSE_WIN_BAD_ADDRESS,
  HOSTS_PARSE_WIN_MAX  // Bounding values for enumeration.
};

// Convenience for reading values using RegKey.
class RegistryReader {
 public:
  explicit RegistryReader(const base::char16* key) {
    // Ignoring the result. |key_.Valid()| will catch failures.
    key_.Open(HKEY_LOCAL_MACHINE, key, KEY_QUERY_VALUE);
  }

  ~RegistryReader() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  bool ReadString(const base::char16* name,
                  DnsSystemSettings::RegString* out) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    out->set = false;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      return true;
    }
    LONG result = key_.ReadValue(name, &out->value);
    if (result == ERROR_SUCCESS) {
      out->set = true;
      return true;
    }
    return (result == ERROR_FILE_NOT_FOUND);
  }

  bool ReadDword(const base::char16* name,
                 DnsSystemSettings::RegDword* out) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    out->set = false;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      return true;
    }
    LONG result = key_.ReadValueDW(name, &out->value);
    if (result == ERROR_SUCCESS) {
      out->set = true;
      return true;
    }
    return (result == ERROR_FILE_NOT_FOUND);
  }

 private:
  base::win::RegKey key_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(RegistryReader);
};

// Wrapper for GetAdaptersAddresses. Returns NULL if failed.
std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> ReadIpHelper(
    ULONG flags) {
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
    rv = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, out.get(), &len);
  }
  if (rv != NO_ERROR)
    out.reset();
  return out;
}

bool ReadDevolutionSetting(const RegistryReader& reader,
                           DnsSystemSettings::DevolutionSetting* setting) {
  return reader.ReadDword(STRING16_LITERAL("UseDomainNameDevolution"),
                          &setting->enabled) &&
         reader.ReadDword(STRING16_LITERAL("DomainNameDevolutionLevel"),
                          &setting->level);
}

// Reads DnsSystemSettings from IpHelper and registry.
ConfigParseWinResult ReadSystemSettings(DnsSystemSettings* settings) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  settings->addresses = ReadIpHelper(GAA_FLAG_SKIP_ANYCAST |
                                     GAA_FLAG_SKIP_UNICAST |
                                     GAA_FLAG_SKIP_MULTICAST |
                                     GAA_FLAG_SKIP_FRIENDLY_NAME);
  if (!settings->addresses.get())
    return CONFIG_PARSE_WIN_READ_IPHELPER;

  RegistryReader tcpip_reader(kTcpipPath);
  RegistryReader tcpip6_reader(kTcpip6Path);
  RegistryReader dnscache_reader(kDnscachePath);
  RegistryReader policy_reader(kPolicyPath);
  RegistryReader primary_dns_suffix_reader(kPrimaryDnsSuffixPath);

  if (!policy_reader.ReadString(STRING16_LITERAL("SearchList"),
                                &settings->policy_search_list)) {
    return CONFIG_PARSE_WIN_READ_POLICY_SEARCHLIST;
  }

  if (!tcpip_reader.ReadString(STRING16_LITERAL("SearchList"),
                               &settings->tcpip_search_list))
    return CONFIG_PARSE_WIN_READ_TCPIP_SEARCHLIST;

  if (!tcpip_reader.ReadString(STRING16_LITERAL("Domain"),
                               &settings->tcpip_domain))
    return CONFIG_PARSE_WIN_READ_DOMAIN;

  if (!ReadDevolutionSetting(policy_reader, &settings->policy_devolution))
    return CONFIG_PARSE_WIN_READ_POLICY_DEVOLUTION;

  if (!ReadDevolutionSetting(dnscache_reader, &settings->dnscache_devolution))
    return CONFIG_PARSE_WIN_READ_DNSCACHE_DEVOLUTION;

  if (!ReadDevolutionSetting(tcpip_reader, &settings->tcpip_devolution))
    return CONFIG_PARSE_WIN_READ_TCPIP_DEVOLUTION;

  if (!policy_reader.ReadDword(STRING16_LITERAL("AppendToMultiLabelName"),
                               &settings->append_to_multi_label_name)) {
    return CONFIG_PARSE_WIN_READ_APPEND_MULTILABEL;
  }

  if (!primary_dns_suffix_reader.ReadString(
          STRING16_LITERAL("PrimaryDnsSuffix"),
          &settings->primary_dns_suffix)) {
    return CONFIG_PARSE_WIN_READ_PRIMARY_SUFFIX;
  }

  base::win::RegistryKeyIterator nrpt_rules(HKEY_LOCAL_MACHINE, kNRPTPath);
  settings->have_name_resolution_policy = (nrpt_rules.SubkeyCount() > 0);

  return CONFIG_PARSE_WIN_OK;
}

// Default address of "localhost" and local computer name can be overridden
// by the HOSTS file, but if it's not there, then we need to fill it in.
HostsParseWinResult AddLocalhostEntries(DnsHosts* hosts) {
  IPAddress loopback_ipv4 = IPAddress::IPv4Localhost();
  IPAddress loopback_ipv6 = IPAddress::IPv6Localhost();

  // This does not override any pre-existing entries from the HOSTS file.
  hosts->insert(std::make_pair(DnsHostsKey("localhost", ADDRESS_FAMILY_IPV4),
                               loopback_ipv4));
  hosts->insert(std::make_pair(DnsHostsKey("localhost", ADDRESS_FAMILY_IPV6),
                               loopback_ipv6));

  base::char16 buffer[MAX_PATH];
  DWORD size = MAX_PATH;
  std::string localname;
  if (!GetComputerNameExW(ComputerNameDnsHostname,
                          base::as_writable_wcstr(buffer), &size) ||
      !ParseDomainASCII(buffer, &localname)) {
    return HOSTS_PARSE_WIN_COMPUTER_NAME_FAILED;
  }
  localname = base::ToLowerASCII(localname);

  bool have_ipv4 =
      hosts->count(DnsHostsKey(localname, ADDRESS_FAMILY_IPV4)) > 0;
  bool have_ipv6 =
      hosts->count(DnsHostsKey(localname, ADDRESS_FAMILY_IPV6)) > 0;

  if (have_ipv4 && have_ipv6)
    return HOSTS_PARSE_WIN_OK;

  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> addresses =
      ReadIpHelper(GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER |
                   GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME);
  if (!addresses.get())
    return HOSTS_PARSE_WIN_IPHELPER_FAILED;

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
        return HOSTS_PARSE_WIN_BAD_ADDRESS;
      }
      if (!have_ipv4 && (ipe.GetFamily() == ADDRESS_FAMILY_IPV4)) {
        have_ipv4 = true;
        (*hosts)[DnsHostsKey(localname, ADDRESS_FAMILY_IPV4)] = ipe.address();
      } else if (!have_ipv6 && (ipe.GetFamily() == ADDRESS_FAMILY_IPV6)) {
        have_ipv6 = true;
        (*hosts)[DnsHostsKey(localname, ADDRESS_FAMILY_IPV6)] = ipe.address();
      }
    }
  }
  return HOSTS_PARSE_WIN_OK;
}

// Watches a single registry key for changes.
class RegistryWatcher {
 public:
  typedef base::Callback<void(bool succeeded)> CallbackType;
  RegistryWatcher() {}

  ~RegistryWatcher() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  bool Watch(const base::char16* key, const CallbackType& callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    callback_ = callback;
    if (key_.Open(HKEY_LOCAL_MACHINE, key, KEY_NOTIFY) != ERROR_SUCCESS)
      return false;

    return key_.StartWatching(base::Bind(&RegistryWatcher::OnObjectSignaled,
                                         base::Unretained(this)));
  }

  void OnObjectSignaled() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!callback_.is_null());
    if (key_.StartWatching(base::Bind(&RegistryWatcher::OnObjectSignaled,
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

  DISALLOW_COPY_AND_ASSIGN(RegistryWatcher);
};

// Returns true iff |address| is DNS address from IPv6 stateless discovery,
// i.e., matches fec0:0:0:ffff::{1,2,3}.
// http://tools.ietf.org/html/draft-ietf-ipngwg-dns-discovery
bool IsStatelessDiscoveryAddress(const IPAddress& address) {
  if (!address.IsIPv6())
    return false;
  const uint8_t kPrefix[] = {0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  return IPAddressStartsWith(address, kPrefix) && (address.bytes().back() < 4);
}

// Returns the path to the HOSTS file.
base::FilePath GetHostsPath() {
  base::char16 buffer[MAX_PATH];
  UINT rc = GetSystemDirectory(base::as_writable_wcstr(buffer), MAX_PATH);
  DCHECK(0 < rc && rc < MAX_PATH);
  return base::FilePath(buffer).Append(
      FILE_PATH_LITERAL("drivers\\etc\\hosts"));
}

void ConfigureSuffixSearch(const DnsSystemSettings& settings,
                           DnsConfig* config) {
  // SearchList takes precedence, so check it first.
  if (settings.policy_search_list.set) {
    std::vector<std::string> search;
    if (ParseSearchList(settings.policy_search_list.value, &search)) {
      config->search.swap(search);
      return;
    }
    // Even if invalid, the policy disables the user-specified setting below.
  } else if (settings.tcpip_search_list.set) {
    std::vector<std::string> search;
    if (ParseSearchList(settings.tcpip_search_list.value, &search)) {
      config->search.swap(search);
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
  if ((settings.primary_dns_suffix.set &&
       ParseDomainASCII(settings.primary_dns_suffix.value, &primary_suffix)) ||
      (settings.tcpip_domain.set &&
       ParseDomainASCII(settings.tcpip_domain.value, &primary_suffix))) {
    // Primary suffix goes in front.
    config->search.insert(config->search.begin(), primary_suffix);
  } else {
    return;  // No primary suffix, hence no devolution.
  }

  // Devolution is determined by precedence: policy > dnscache > tcpip.
  // |enabled|: UseDomainNameDevolution and |level|: DomainNameDevolutionLevel
  // are overridden independently.
  DnsSystemSettings::DevolutionSetting devolution = settings.policy_devolution;

  if (!devolution.enabled.set)
    devolution.enabled = settings.dnscache_devolution.enabled;
  if (!devolution.enabled.set)
    devolution.enabled = settings.tcpip_devolution.enabled;
  if (devolution.enabled.set && (devolution.enabled.value == 0))
    return;  // Devolution disabled.

  // By default devolution is enabled.

  if (!devolution.level.set)
    devolution.level = settings.dnscache_devolution.level;
  if (!devolution.level.set)
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
  if (!devolution.level.set || devolution.level.value < 2)
    return;  // Devolution disabled.

  // Devolve the primary suffix. This naive logic matches the observed
  // behavior (see also ParseSearchList). If a suffix is not valid, it will be
  // discarded when the fully-qualified name is converted to DNS format.

  unsigned num_dots = std::count(primary_suffix.begin(),
                                 primary_suffix.end(), '.');

  for (size_t offset = 0; num_dots >= devolution.level.value; --num_dots) {
    offset = primary_suffix.find('.', offset + 1);
    config->search.push_back(primary_suffix.substr(offset + 1));
  }
}

}  // namespace

DnsSystemSettings::DnsSystemSettings()
    : policy_search_list(),
      tcpip_search_list(),
      tcpip_domain(),
      primary_dns_suffix(),
      policy_devolution(),
      dnscache_devolution(),
      tcpip_devolution(),
      append_to_multi_label_name(),
      have_name_resolution_policy(false) {
  policy_search_list.set = false;
  tcpip_search_list.set = false;
  tcpip_domain.set = false;
  primary_dns_suffix.set = false;

  policy_devolution.enabled.set = false;
  policy_devolution.level.set = false;
  dnscache_devolution.enabled.set = false;
  dnscache_devolution.level.set = false;
  tcpip_devolution.enabled.set = false;
  tcpip_devolution.level.set = false;

  append_to_multi_label_name.set = false;
}

DnsSystemSettings::~DnsSystemSettings() {
}

bool ParseDomainASCII(base::StringPiece16 widestr, std::string* domain) {
  DCHECK(domain);
  if (widestr.empty())
    return false;

  // Check if already ASCII.
  if (base::IsStringASCII(widestr)) {
    domain->assign(widestr.begin(), widestr.end());
    return true;
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url::RawCanonOutputT<base::char16, kInitialBufferSize> punycode;
  if (!url::IDNToASCII(widestr.data(), widestr.length(), &punycode))
    return false;

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = base::UTF16ToUTF8(punycode.data(), punycode.length(), domain);
  DCHECK(success);
  DCHECK(base::IsStringASCII(*domain));
  return success && !domain->empty();
}

bool ParseSearchList(const base::string16& value,
                     std::vector<std::string>* output) {
  DCHECK(output);
  if (value.empty())
    return false;

  output->clear();

  // If the list includes an empty hostname (",," or ", ,"), it is terminated.
  // Although nslookup and network connection property tab ignore such
  // fragments ("a,b,,c" becomes ["a", "b", "c"]), our reference is getaddrinfo
  // (which sees ["a", "b"]). WMI queries also return a matching search list.
  for (const base::StringPiece16& t :
       base::SplitStringPiece(value, STRING16_LITERAL(","),
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Convert non-ASCII to punycode, although getaddrinfo does not properly
    // handle such suffixes.
    std::string parsed;
    if (!ParseDomainASCII(t, &parsed))
      break;
    output->push_back(parsed);
  }
  return !output->empty();
}

ConfigParseWinResult ConvertSettingsToDnsConfig(
    const DnsSystemSettings& settings,
    DnsConfig* config) {
  *config = DnsConfig();

  // Use GetAdapterAddresses to get effective DNS server order and
  // connection-specific DNS suffix. Ignore disconnected and loopback adapters.
  // The order of adapters is the network binding order, so stick to the
  // first good adapter.
  for (const IP_ADAPTER_ADDRESSES* adapter = settings.addresses.get();
       adapter != nullptr && config->nameservers.empty();
       adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp)
      continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;

    for (const IP_ADAPTER_DNS_SERVER_ADDRESS* address =
             adapter->FirstDnsServerAddress;
         address != nullptr; address = address->Next) {
      IPEndPoint ipe;
      if (ipe.FromSockAddr(address->Address.lpSockaddr,
                           address->Address.iSockaddrLength)) {
        if (IsStatelessDiscoveryAddress(ipe.address()))
          continue;
        // Override unset port.
        if (!ipe.port())
          ipe = IPEndPoint(ipe.address(), dns_protocol::kDefaultPort);
        config->nameservers.push_back(ipe);
      } else {
        return CONFIG_PARSE_WIN_BAD_ADDRESS;
      }
    }

    // IP_ADAPTER_ADDRESSES in Vista+ has a search list at |FirstDnsSuffix|,
    // but it came up empty in all trials.
    // |DnsSuffix| stores the effective connection-specific suffix, which is
    // obtained via DHCP (regkey: Tcpip\Parameters\Interfaces\{XXX}\DhcpDomain)
    // or specified by the user (regkey: Tcpip\Parameters\Domain).
    std::string dns_suffix;
    if (ParseDomainASCII(base::as_u16cstr(adapter->DnsSuffix), &dns_suffix))
      config->search.push_back(dns_suffix);
  }

  if (config->nameservers.empty())
    return CONFIG_PARSE_WIN_NO_NAMESERVERS;  // No point continuing.

  // Windows always tries a multi-label name "as is" before using suffixes.
  config->ndots = 1;

  if (!settings.append_to_multi_label_name.set) {
    config->append_to_multi_label_name = false;
  } else {
    config->append_to_multi_label_name =
        (settings.append_to_multi_label_name.value != 0);
  }

  ConfigParseWinResult result = CONFIG_PARSE_WIN_OK;
  if (settings.have_name_resolution_policy) {
    config->unhandled_options = true;
    // TODO(szym): only set this to true if NRPT has DirectAccess rules.
    config->use_local_ipv6 = true;
    result = CONFIG_PARSE_WIN_UNHANDLED_OPTIONS;
  }

  ConfigureSuffixSearch(settings, config);
  return result;
}

// Watches registry and HOSTS file for changes. Must live on a sequence which
// allows IO.
class DnsConfigServiceWin::Watcher
    : public NetworkChangeNotifier::IPAddressObserver {
 public:
  explicit Watcher(DnsConfigServiceWin* service) : service_(service) {}
  ~Watcher() override { NetworkChangeNotifier::RemoveIPAddressObserver(this); }

  bool Watch() {
    RegistryWatcher::CallbackType callback =
        base::Bind(&DnsConfigServiceWin::OnConfigChanged,
                   base::Unretained(service_));

    bool success = true;

    // The Tcpip key must be present.
    if (!tcpip_watcher_.Watch(kTcpipPath, callback)) {
      LOG(ERROR) << "DNS registry watch failed to start.";
      success = false;
      UMA_HISTOGRAM_ENUMERATION("AsyncDNS.WatchStatus",
                                DNS_CONFIG_WATCH_FAILED_TO_START_CONFIG,
                                DNS_CONFIG_WATCH_MAX);
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

    if (!hosts_watcher_.Watch(GetHostsPath(), false,
                              base::Bind(&Watcher::OnHostsChanged,
                                         base::Unretained(this)))) {
      UMA_HISTOGRAM_ENUMERATION("AsyncDNS.WatchStatus",
                                DNS_CONFIG_WATCH_FAILED_TO_START_HOSTS,
                                DNS_CONFIG_WATCH_MAX);
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    } else {
      // Also need to observe changes to local non-loopback IP for DnsHosts.
      NetworkChangeNotifier::AddIPAddressObserver(this);
    }
    return success;
  }

 private:
  void OnHostsChanged(const base::FilePath& path, bool error) {
    if (error)
      NetworkChangeNotifier::RemoveIPAddressObserver(this);
    service_->OnHostsChanged(!error);
  }

  // NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override {
    // Need to update non-loopback IP of local host.
    service_->OnHostsChanged(true);
  }

  DnsConfigServiceWin* service_;

  RegistryWatcher tcpip_watcher_;
  RegistryWatcher tcpip6_watcher_;
  RegistryWatcher dnscache_watcher_;
  RegistryWatcher policy_watcher_;
  base::FilePathWatcher hosts_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Watcher);
};

// Reads config from registry and IpHelper. All work performed in ThreadPool.
class DnsConfigServiceWin::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServiceWin* service)
      : service_(service),
        success_(false) {}

 private:
  ~ConfigReader() override {}

  void DoWork() override {
    base::TimeTicks start_time = base::TimeTicks::Now();
    DnsSystemSettings settings = {};
    ConfigParseWinResult result = ReadSystemSettings(&settings);
    if (result == CONFIG_PARSE_WIN_OK)
      result = ConvertSettingsToDnsConfig(settings, &dns_config_);
    success_ = (result == CONFIG_PARSE_WIN_OK ||
                result == CONFIG_PARSE_WIN_UNHANDLED_OPTIONS);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ConfigParseWin",
                              result, CONFIG_PARSE_WIN_MAX);
    UMA_HISTOGRAM_TIMES("AsyncDNS.ConfigParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  void OnWorkFinished() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!IsCancelled());
    if (success_) {
      service_->OnConfigRead(dns_config_);
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
      // Try again in a while in case DnsConfigWatcher missed the signal.
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&ConfigReader::WorkNow, this),
          base::TimeDelta::FromSeconds(kRetryIntervalSeconds));
    }
  }

  DnsConfigServiceWin* service_;
  // Written in DoWork(), read in OnWorkFinished(). No locking required.
  DnsConfig dns_config_;
  bool success_;
};

// Reads hosts from HOSTS file and fills in localhost and local computer name if
// necessary. All work performed in ThreadPool.
class DnsConfigServiceWin::HostsReader : public SerialWorker {
 public:
  explicit HostsReader(DnsConfigServiceWin* service)
      : path_(GetHostsPath()),
        service_(service),
        success_(false) {
  }

 private:
  ~HostsReader() override {}

  void DoWork() override {
    base::TimeTicks start_time = base::TimeTicks::Now();
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    HostsParseWinResult result = HOSTS_PARSE_WIN_UNREADABLE_HOSTS_FILE;
    if (ParseHostsFile(path_, &hosts_))
      result = AddLocalhostEntries(&hosts_);
    success_ = (result == HOSTS_PARSE_WIN_OK);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.HostsParseWin",
                              result, HOSTS_PARSE_WIN_MAX);
    UMA_HISTOGRAM_BOOLEAN("AsyncDNS.HostParseResult", success_);
    UMA_HISTOGRAM_TIMES("AsyncDNS.HostsParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  void OnWorkFinished() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (success_) {
      service_->OnHostsRead(hosts_);
    } else {
      LOG(WARNING) << "Failed to read DnsHosts.";
    }
  }

  const base::FilePath path_;
  DnsConfigServiceWin* service_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsHosts hosts_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(HostsReader);
};

DnsConfigServiceWin::DnsConfigServiceWin() {
  // Allow constructing on one sequence and living on another.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DnsConfigServiceWin::~DnsConfigServiceWin() {
  if (config_reader_)
    config_reader_->Cancel();
  if (hosts_reader_)
    hosts_reader_->Cancel();
}

void DnsConfigServiceWin::ReadNow() {
  config_reader_->WorkNow();
  hosts_reader_->WorkNow();
}

bool DnsConfigServiceWin::StartWatching() {
  if (!config_reader_)
    config_reader_ = base::MakeRefCounted<ConfigReader>(this);
  if (!hosts_reader_)
    hosts_reader_ = base::MakeRefCounted<HostsReader>(this);
  // TODO(szym): re-start watcher if that makes sense. http://crbug.com/116139
  watcher_.reset(new Watcher(this));
  UMA_HISTOGRAM_ENUMERATION("AsyncDNS.WatchStatus", DNS_CONFIG_WATCH_STARTED,
                            DNS_CONFIG_WATCH_MAX);
  return watcher_->Watch();
}

void DnsConfigServiceWin::OnConfigChanged(bool succeeded) {
  InvalidateConfig();
  config_reader_->WorkNow();
  if (!succeeded) {
    LOG(ERROR) << "DNS config watch failed.";
    set_watch_failed(true);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.WatchStatus",
                              DNS_CONFIG_WATCH_FAILED_CONFIG,
                              DNS_CONFIG_WATCH_MAX);
  }
}

void DnsConfigServiceWin::OnHostsChanged(bool succeeded) {
  InvalidateHosts();
  if (succeeded) {
    hosts_reader_->WorkNow();
  } else {
    LOG(ERROR) << "DNS hosts watch failed.";
    set_watch_failed(true);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.WatchStatus",
                              DNS_CONFIG_WATCH_FAILED_HOSTS,
                              DNS_CONFIG_WATCH_MAX);
  }
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::unique_ptr<DnsConfigService>(new internal::DnsConfigServiceWin());
}

}  // namespace net
