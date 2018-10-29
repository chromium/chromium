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
  ~DnsConfig();

  DnsConfig& operator=(const DnsConfig& other);
  DnsConfig& operator=(DnsConfig&& other);

  bool Equals(const DnsConfig& d) const;

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

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;
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
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_H_
