// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_H_
#define NET_DNS_DNS_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/dns_hosts.h"

namespace base {
class Value;
}

namespace net {

// Default to 1 second timeout (before exponential backoff).
constexpr base::TimeDelta kDnsDefaultTimeout = base::TimeDelta::FromSeconds(1);

// DnsConfig stores configuration of the system resolver.
struct NET_EXPORT DnsConfig {
  DnsConfig();
  DnsConfig(const DnsConfig& other);
  DnsConfig(DnsConfig&& other);
  explicit DnsConfig(std::vector<IPEndPoint> nameservers);
  ~DnsConfig();

  DnsConfig& operator=(const DnsConfig& other);
  DnsConfig& operator=(DnsConfig&& other);

  bool Equals(const DnsConfig& d) const;
  bool operator==(const DnsConfig& d) const;
  bool operator!=(const DnsConfig& d) const;

  bool EqualsIgnoreHosts(const DnsConfig& d) const;

  void CopyIgnoreHosts(const DnsConfig& src);

  // Returns a Value representation of |this|. For performance reasons, the
  // Value only contains the number of hosts rather than the full list.
  std::unique_ptr<base::Value> ToValue() const;

  bool IsValid() const { return !nameservers.empty(); }

  struct NET_EXPORT DnsOverHttpsServerConfig {
    DnsOverHttpsServerConfig(const std::string& server_template, bool use_post);

    bool operator==(const DnsOverHttpsServerConfig& other) const;

    std::string server_template;
    bool use_post;
  };

  // The SecureDnsMode specifies what types of lookups (secure/insecure) should
  // be performed and in what order when resolving a specific query. The int
  // values should not be changed as they are logged.
  enum class SecureDnsMode : int {
    // In OFF mode, no DoH lookups should be performed.
    OFF = 0,
    // In AUTOMATIC mode, DoH lookups should be performed first if DoH is
    // available, and insecure DNS lookups should be performed as a fallback.
    AUTOMATIC = 1,
    // In SECURE mode, only DoH lookups should be performed.
    SECURE = 2,
  };

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;

  // Status of system DNS-over-TLS (DoT).
  bool dns_over_tls_active;
  std::string dns_over_tls_hostname;

  // Suffix search list; used on first lookup when number of dots in given name
  // is less than |ndots|.
  std::vector<std::string> search;

  DnsHosts hosts;

  // True if there are options set in the system configuration that are not yet
  // supported by DnsClient.
  bool unhandled_options;

  // AppendToMultiLabelName: is suffix search performed for multi-label names?
  // True, except on Windows where it can be configured.
  bool append_to_multi_label_name;

  // Indicates that source port randomization is required. This uses additional
  // resources on some platforms.
  bool randomize_ports;

  // Resolver options; see man resolv.conf.

  // Minimum number of dots before global resolution precedes |search|.
  int ndots;
  // Time between retransmissions, see res_state.retrans.
  base::TimeDelta timeout;
  // Maximum number of attempts, see res_state.retry.
  int attempts;
  // Round robin entries in |nameservers| for subsequent requests.
  bool rotate;

  // Indicates system configuration uses local IPv6 connectivity, e.g.,
  // DirectAccess. This is exposed for HostResolver to skip IPv6 probes,
  // as it may cause them to return incorrect results.
  bool use_local_ipv6;

  // List of servers to query over HTTPS, queried in order
  // (https://tools.ietf.org/id/draft-ietf-doh-dns-over-https-12.txt).
  std::vector<DnsOverHttpsServerConfig> dns_over_https_servers;

  // The default SecureDnsMode to use when resolving queries. It can be
  // overridden for individual requests (such as requests to resolve a DoH
  // server hostname) using |HostResolver::ResolveHostParameters::
  // secure_dns_mode_override|.
  SecureDnsMode secure_dns_mode;

  // If set to |true|, we will attempt to upgrade the user's DNS configuration
  // to use DoH server(s) operated by the same provider(s) when the user is
  // in AUTOMATIC mode and has not pre-specified DoH servers.
  bool allow_dns_over_https_upgrade;

  // List of providers to exclude from upgrade mapping. See the
  // mapping in net/dns/dns_util.cc for provider ids.
  std::vector<std::string> disabled_upgrade_providers;
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_H_
