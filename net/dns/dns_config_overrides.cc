// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_overrides.h"

namespace net {

DnsConfigOverrides::DnsConfigOverrides() = default;

DnsConfigOverrides::DnsConfigOverrides(const DnsConfigOverrides& other) =
    default;

DnsConfigOverrides::DnsConfigOverrides(DnsConfigOverrides&& other) = default;

DnsConfigOverrides::~DnsConfigOverrides() = default;

DnsConfigOverrides& DnsConfigOverrides::operator=(
    const DnsConfigOverrides& other) = default;

DnsConfigOverrides& DnsConfigOverrides::operator=(DnsConfigOverrides&& other) =
    default;

bool DnsConfigOverrides::operator==(const DnsConfigOverrides& other) const {
  return nameservers == other.nameservers && search == other.search &&
         hosts == other.hosts &&
         append_to_multi_label_name == other.append_to_multi_label_name &&
         randomize_ports == other.randomize_ports && ndots == other.ndots &&
         timeout == other.timeout && attempts == other.attempts &&
         rotate == other.rotate && use_local_ipv6 == other.use_local_ipv6 &&
         dns_over_https_servers == other.dns_over_https_servers &&
         secure_dns_mode == other.secure_dns_mode &&
         allow_dns_over_https_upgrade == other.allow_dns_over_https_upgrade &&
         disabled_upgrade_providers == other.disabled_upgrade_providers;
}

bool DnsConfigOverrides::operator!=(const DnsConfigOverrides& other) const {
  return !(*this == other);
}

// static
DnsConfigOverrides
DnsConfigOverrides::CreateOverridingEverythingWithDefaults() {
  DnsConfig defaults;

  DnsConfigOverrides overrides;
  overrides.nameservers = defaults.nameservers;
  overrides.search = defaults.search;
  overrides.hosts = defaults.hosts;
  overrides.append_to_multi_label_name = defaults.append_to_multi_label_name;
  overrides.randomize_ports = defaults.randomize_ports;
  overrides.ndots = defaults.ndots;
  overrides.timeout = defaults.timeout;
  overrides.attempts = defaults.attempts;
  overrides.rotate = defaults.rotate;
  overrides.use_local_ipv6 = defaults.use_local_ipv6;
  overrides.dns_over_https_servers = defaults.dns_over_https_servers;
  overrides.secure_dns_mode = defaults.secure_dns_mode;
  overrides.allow_dns_over_https_upgrade =
      defaults.allow_dns_over_https_upgrade;
  overrides.disabled_upgrade_providers = defaults.disabled_upgrade_providers;

  return overrides;
}

bool DnsConfigOverrides::OverridesEverything() const {
  return nameservers && search && hosts && append_to_multi_label_name &&
         randomize_ports && ndots && timeout && attempts && rotate &&
         use_local_ipv6 && dns_over_https_servers && secure_dns_mode &&
         allow_dns_over_https_upgrade && disabled_upgrade_providers;
}

DnsConfig DnsConfigOverrides::ApplyOverrides(const DnsConfig& config) const {
  DnsConfig overridden;

  if (!OverridesEverything())
    overridden = config;

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
  if (secure_dns_mode)
    overridden.secure_dns_mode = secure_dns_mode.value();
  if (allow_dns_over_https_upgrade) {
    overridden.allow_dns_over_https_upgrade =
        allow_dns_over_https_upgrade.value();
  }
  if (disabled_upgrade_providers)
    overridden.disabled_upgrade_providers = disabled_upgrade_providers.value();

  return overridden;
}

}  // namespace net
