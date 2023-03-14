// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_H_
#define NET_DNS_DNS_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"

namespace net {

constexpr base::TimeDelta kDnsDefaultFallbackPeriod = base::Seconds(1);

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

  // Returns a Dict representation of |this|. For performance reasons, the
  // Dict only contains the number of hosts rather than the full list.
  base::Value::Dict ToDict() const;

  bool IsValid() const {
    return !nameservers.empty() || !doh_config.servers().empty();
  }

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;

  // Status of system DNS-over-TLS (DoT).
  bool dns_over_tls_active = false;
  std::string dns_over_tls_hostname;

  // Suffix search list; used on first lookup when number of dots in given name
  // is less than |ndots|.
  std::vector<std::string> search;

  DnsHosts hosts;

  // True if there are options set in the system configuration that are not yet
  // supported by DnsClient.
  bool unhandled_options = false;

  // AppendToMultiLabelName: is suffix search performed for multi-label names?
  // True, except on Windows where it can be configured.
  bool append_to_multi_label_name = true;

  // Resolver options; see man resolv.conf.

  // Minimum number of dots before global resolution precedes |search|.
  int ndots = 1;
  // Time between retransmissions, see res_state.retrans.
  // Used by Chrome as the initial transaction attempt fallback period (before
  // exponential backoff and dynamic period determination based on previous
  // attempts.)
  base::TimeDelta fallback_period = kDnsDefaultFallbackPeriod;
  // Maximum number of attempts, see res_state.retry.
  int attempts = 2;
  // Maximum number of times a DoH server is attempted per attempted per DNS
  // transaction. This is separate from the global failure limit.
  int doh_attempts = 1;
  // Round robin entries in |nameservers| for subsequent requests.
  bool rotate = false;

  // Indicates system configuration uses local IPv6 connectivity, e.g.,
  // DirectAccess. This is exposed for HostResolver to skip IPv6 probes,
  // as it may cause them to return incorrect results.
  bool use_local_ipv6 = false;

  // DNS over HTTPS server configuration.
  DnsOverHttpsConfig doh_config;

  // The default SecureDnsMode to use when resolving queries. It can be
  // overridden for individual requests (such as requests to resolve a DoH
  // server hostname) using |HostResolver::ResolveHostParameters::
  // secure_dns_mode_override|.
  SecureDnsMode secure_dns_mode = SecureDnsMode::kOff;

  // If set to |true|, we will attempt to upgrade the user's DNS configuration
  // to use DoH server(s) operated by the same provider(s) when the user is
  // in AUTOMATIC mode and has not pre-specified DoH servers.
  bool allow_dns_over_https_upgrade = false;
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_H_
