// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_WIN_DNS_SYSTEM_SETTINGS_H_
#define NET_DNS_PUBLIC_WIN_DNS_SYSTEM_SETTINGS_H_

#include <winsock2.h>
#include <iphlpapi.h>
#include <iptypes.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/free_deleter.h"
#include "base/strings/string_piece_forward.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This is an aggregate representation of Windows system DNS configuration and
// can be easily built manually in tests.
struct NET_EXPORT WinDnsSystemSettings {
  struct NET_EXPORT DevolutionSetting {
    DevolutionSetting();
    DevolutionSetting(absl::optional<DWORD> enabled,
                      absl::optional<DWORD> level);
    DevolutionSetting(const DevolutionSetting&);
    DevolutionSetting& operator=(const DevolutionSetting&);
    ~DevolutionSetting();

    // UseDomainNameDevolution
    absl::optional<DWORD> enabled;
    // DomainNameDevolutionLevel
    absl::optional<DWORD> level;
  };

  // Returns true iff |address| is DNS address from IPv6 stateless discovery,
  // i.e., matches fec0:0:0:ffff::{1,2,3}.
  // http://tools.ietf.org/html/draft-ietf-ipngwg-dns-discovery
  static bool IsStatelessDiscoveryAddress(const IPAddress& address);

  WinDnsSystemSettings();
  ~WinDnsSystemSettings();

  WinDnsSystemSettings(WinDnsSystemSettings&&);
  WinDnsSystemSettings& operator=(WinDnsSystemSettings&&);

  // List of nameserver IP addresses.
  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> addresses;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient\SearchList
  absl::optional<std::wstring> policy_search_list;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters\SearchList
  absl::optional<std::wstring> tcpip_search_list;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters\Domain
  absl::optional<std::wstring> tcpip_domain;
  // SOFTWARE\Policies\Microsoft\System\DNSClient\PrimaryDnsSuffix
  absl::optional<std::wstring> primary_dns_suffix;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient
  DevolutionSetting policy_devolution;
  // SYSTEM\CurrentControlSet\Dnscache\Parameters
  DevolutionSetting dnscache_devolution;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters
  DevolutionSetting tcpip_devolution;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient\AppendToMultiLabelName
  absl::optional<DWORD> append_to_multi_label_name;

  // True when the Name Resolution Policy Table (NRPT) has at least one rule:
  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient\DnsPolicyConfig\Rule*
  // (or)
  // SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig\Rule*
  bool have_name_resolution_policy = false;

  // True when a proxy is configured via at least one rule:
  // SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsConnections
  // (or)
  // SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsActiveIfs
  // (or)
  // SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsConnectionsProxies
  bool have_proxy = false;

  // Gets Windows configured DNS servers from all network adapters, with the
  // exception of stateless discovery addresses (see IsStatelessDiscoveryAddress
  // above).
  absl::optional<std::vector<IPEndPoint>> GetAllNameservers();
};

// Reads WinDnsSystemSettings from IpHelper and the registry, or nullopt on
// errors reading them.
NET_EXPORT absl::optional<WinDnsSystemSettings> ReadWinSystemDnsSettings();

}  // namespace net

#endif  // NET_DNS_PUBLIC_WIN_DNS_SYSTEM_SETTINGS_H_
