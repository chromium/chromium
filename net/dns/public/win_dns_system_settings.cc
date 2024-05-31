// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/win_dns_system_settings.h"

#include <sysinfoapi.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

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
const wchar_t kPrimaryDnsSuffixPath[] =
    L"SOFTWARE\\Policies\\Microsoft\\System\\DNSClient";
const wchar_t kNrptPath[] =
    L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient\\DnsPolicyConfig";
const wchar_t kControlSetNrptPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\"
    L"DnsPolicyConfig";
const wchar_t kDnsConnectionsPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\"
    L"DnsConnections";
const wchar_t kDnsConnectionsProxies[] =
    L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters\\"
    L"DnsConnectionsProxies";

// Convenience for reading values using RegKey.
class RegistryReader {
 public:
  explicit RegistryReader(const wchar_t key[]) {
    // Ignoring the result. |key_.Valid()| will catch failures.
    (void)key_.Open(HKEY_LOCAL_MACHINE, key, KEY_QUERY_VALUE);
  }

  RegistryReader(const RegistryReader&) = delete;
  RegistryReader& operator=(const RegistryReader&) = delete;

  ~RegistryReader() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // Returns `false` if any error occurs, but not if the value is unset.
  bool ReadString(const wchar_t name[],
                  std::optional<std::wstring>* output) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::wstring reg_string;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      *output = std::nullopt;
      return true;
    }
    LONG result = key_.ReadValue(name, &reg_string);
    if (result == ERROR_SUCCESS) {
      *output = std::move(reg_string);
      return true;
    }

    if (result == ERROR_FILE_NOT_FOUND) {
      *output = std::nullopt;
      return true;
    }

    return false;
  }

  // Returns `false` if any error occurs, but not if the value is unset.
  bool ReadDword(const wchar_t name[], std::optional<DWORD>* output) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DWORD reg_dword;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      *output = std::nullopt;
      return true;
    }

    LONG result = key_.ReadValueDW(name, &reg_dword);
    if (result == ERROR_SUCCESS) {
      *output = reg_dword;
      return true;
    }

    if (result == ERROR_FILE_NOT_FOUND) {
      *output = std::nullopt;
      return true;
    }

    return false;
  }

 private:
  base::win::RegKey key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Wrapper for GetAdaptersAddresses to get DNS addresses.
// Returns nullptr if failed.
std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter>
ReadAdapterDnsAddresses() {
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
                              GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_UNICAST |
                              GAA_FLAG_SKIP_MULTICAST |
                              GAA_FLAG_SKIP_FRIENDLY_NAME,
                              nullptr, out.get(), &len);
  }
  if (rv != NO_ERROR)
    out.reset();
  return out;
}

// Returns `false` if any error occurs, but not if the value is unset.
bool ReadDevolutionSetting(const RegistryReader& reader,
                           WinDnsSystemSettings::DevolutionSetting* output) {
  std::optional<DWORD> enabled;
  std::optional<DWORD> level;
  if (!reader.ReadDword(L"UseDomainNameDevolution", &enabled) ||
      !reader.ReadDword(L"DomainNameDevolutionLevel", &level)) {
    return false;
  }

  *output = {enabled, level};
  return true;
}

}  // namespace

WinDnsSystemSettings::WinDnsSystemSettings() = default;
WinDnsSystemSettings::~WinDnsSystemSettings() = default;

WinDnsSystemSettings::DevolutionSetting::DevolutionSetting() = default;
WinDnsSystemSettings::DevolutionSetting::DevolutionSetting(
    std::optional<DWORD> enabled,
    std::optional<DWORD> level)
    : enabled(enabled), level(level) {}
WinDnsSystemSettings::DevolutionSetting::DevolutionSetting(
    const DevolutionSetting&) = default;
WinDnsSystemSettings::DevolutionSetting&
WinDnsSystemSettings::DevolutionSetting::operator=(
    const WinDnsSystemSettings::DevolutionSetting&) = default;
WinDnsSystemSettings::DevolutionSetting::~DevolutionSetting() = default;

WinDnsSystemSettings::WinDnsSystemSettings(WinDnsSystemSettings&&) = default;
WinDnsSystemSettings& WinDnsSystemSettings::operator=(WinDnsSystemSettings&&) =
    default;

// static
bool WinDnsSystemSettings::IsStatelessDiscoveryAddress(
    const IPAddress& address) {
  if (!address.IsIPv6())
    return false;
  const uint8_t kPrefix[] = {0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  return IPAddressStartsWith(address, kPrefix) && (address.bytes().back() < 4);
}

std::optional<std::vector<IPEndPoint>>
WinDnsSystemSettings::GetAllNameservers() {
  std::vector<IPEndPoint> nameservers;
  for (const IP_ADAPTER_ADDRESSES* adapter = addresses.get();
       adapter != nullptr; adapter = adapter->Next) {
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
        nameservers.push_back(ipe);
      } else {
        return std::nullopt;
      }
    }
  }
  return nameservers;
}

base::expected<WinDnsSystemSettings, ReadWinSystemDnsSettingsError>
ReadWinSystemDnsSettings() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  WinDnsSystemSettings settings;

  // Filled in by GetAdapterAddresses. Note that the alternative
  // GetNetworkParams does not include IPv6 addresses.
  settings.addresses = ReadAdapterDnsAddresses();
  if (!settings.addresses.get()) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadAdapterDnsAddressesFailed);
  }

  RegistryReader tcpip_reader(kTcpipPath);
  RegistryReader tcpip6_reader(kTcpip6Path);
  RegistryReader dnscache_reader(kDnscachePath);
  RegistryReader policy_reader(kPolicyPath);
  RegistryReader primary_dns_suffix_reader(kPrimaryDnsSuffixPath);

  std::optional<std::wstring> reg_string;
  if (!policy_reader.ReadString(L"SearchList", &reg_string)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadPolicySearchListFailed);
  }
  settings.policy_search_list = std::move(reg_string);

  if (!tcpip_reader.ReadString(L"SearchList", &reg_string)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadTcpipSearchListFailed);
  }
  settings.tcpip_search_list = std::move(reg_string);

  if (!tcpip_reader.ReadString(L"Domain", &reg_string)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadTcpipDomainFailed);
  }
  settings.tcpip_domain = std::move(reg_string);

  WinDnsSystemSettings::DevolutionSetting devolution_setting;
  if (!ReadDevolutionSetting(policy_reader, &devolution_setting)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadPolicyDevolutionSettingFailed);
  }
  settings.policy_devolution = devolution_setting;

  if (!ReadDevolutionSetting(dnscache_reader, &devolution_setting)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadDnscacheDevolutionSettingFailed);
  }
  settings.dnscache_devolution = devolution_setting;

  if (!ReadDevolutionSetting(tcpip_reader, &devolution_setting)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadTcpipDevolutionSettingFailed);
  }
  settings.tcpip_devolution = devolution_setting;

  std::optional<DWORD> reg_dword;
  if (!policy_reader.ReadDword(L"AppendToMultiLabelName", &reg_dword)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadPolicyAppendToMultiLabelNameFailed);
  }
  settings.append_to_multi_label_name = reg_dword;

  if (!primary_dns_suffix_reader.ReadString(L"PrimaryDnsSuffix", &reg_string)) {
    return base::unexpected(
        ReadWinSystemDnsSettingsError::kReadPrimaryDnsSuffixPathFailed);
  }
  settings.primary_dns_suffix = std::move(reg_string);

  base::win::RegistryKeyIterator nrpt_rules(HKEY_LOCAL_MACHINE, kNrptPath);
  base::win::RegistryKeyIterator cs_nrpt_rules(HKEY_LOCAL_MACHINE,
                                               kControlSetNrptPath);
  settings.have_name_resolution_policy =
      (nrpt_rules.SubkeyCount() > 0 || cs_nrpt_rules.SubkeyCount() > 0);

  base::win::RegistryKeyIterator dns_connections(HKEY_LOCAL_MACHINE,
                                                 kDnsConnectionsPath);
  base::win::RegistryKeyIterator dns_connections_proxies(
      HKEY_LOCAL_MACHINE, kDnsConnectionsProxies);
  settings.have_proxy = (dns_connections.SubkeyCount() > 0 ||
                         dns_connections_proxies.SubkeyCount() > 0);

  return settings;
}

}  // namespace net
