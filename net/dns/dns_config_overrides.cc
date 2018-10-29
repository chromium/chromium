// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_overrides.h"

namespace net {

DnsConfigOverrides::DnsConfigOverrides() = default;

DnsConfigOverrides::DnsConfigOverrides(const DnsConfigOverrides& other) =
    default;

DnsConfigOverrides::~DnsConfigOverrides() = default;

DnsConfigOverrides& DnsConfigOverrides::operator=(
    const DnsConfigOverrides& other) = default;

bool DnsConfigOverrides::operator==(const DnsConfigOverrides& other) const {
  return nameservers == other.nameservers && search == other.search &&
         hosts == other.hosts &&
         append_to_multi_label_name == other.append_to_multi_label_name &&
         randomize_ports == other.randomize_ports && ndots == other.ndots &&
         timeout == other.timeout && attempts == other.attempts &&
         rotate == other.rotate && use_local_ipv6 == other.use_local_ipv6 &&
         dns_over_https_servers == other.dns_over_https_servers;
}

bool DnsConfigOverrides::operator!=(const DnsConfigOverrides& other) const {
  return !(*this == other);
}

DnsConfig DnsConfigOverrides::ApplyOverrides(const DnsConfig& config) const {
  DnsConfig overridden(config);

  if (nameservers)
    overridden.nameservers = nameservers.value();
  if (search)
    overridden.search = search.value();
  if (hosts)
    overridden.hosts = hosts.value();
  if (append_to_multi_label_name)
    overridden.append_to_multi_label_name = append_to_multi_label_name.value();
  if (randomize_ports)
    overridden.randomize_ports = randomize_ports.value();
  if (ndots)
    overridden.ndots = ndots.value();
  if (timeout)
    overridden.timeout = timeout.value();
  if (attempts)
    overridden.attempts = attempts.value();
  if (rotate)
    overridden.rotate = rotate.value();
  if (use_local_ipv6)
    overridden.use_local_ipv6 = use_local_ipv6.value();
  if (dns_over_https_servers)
    overridden.dns_over_https_servers = dns_over_https_servers.value();

  return overridden;
}

}  // namespace net
