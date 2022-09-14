// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_config_overrides.h"

#include "net/dns/dns_config.h"

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
  return nameservers == other.nameservers &&
         dns_over_tls_active == other.dns_over_tls_active &&
         dns_over_tls_hostname == other.dns_over_tls_hostname &&
         search == other.search &&
         append_to_multi_label_name == other.append_to_multi_label_name &&
         ndots == other.ndots && fallback_period == other.fallback_period &&
         attempts == other.attempts && doh_attempts == other.doh_attempts &&
         rotate == other.rotate && use_local_ipv6 == other.use_local_ipv6 &&
         dns_over_https_config == other.dns_over_https_config &&
         secure_dns_mode == other.secure_dns_mode &&
         allow_dns_over_https_upgrade == other.allow_dns_over_https_upgrade &&
         clear_hosts == other.clear_hosts;
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
  overrides.dns_over_tls_active = defaults.dns_over_tls_active;
  overrides.dns_over_tls_hostname = defaults.dns_over_tls_hostname;
  overrides.search = defaults.search;
  overrides.append_to_multi_label_name = defaults.append_to_multi_label_name;
  overrides.ndots = defaults.ndots;
  overrides.fallback_period = defaults.fallback_period;
  overrides.attempts = defaults.attempts;
  overrides.doh_attempts = defaults.doh_attempts;
  overrides.rotate = defaults.rotate;
  overrides.use_local_ipv6 = defaults.use_local_ipv6;
  overrides.dns_over_https_config = defaults.doh_config;
  overrides.secure_dns_mode = defaults.secure_dns_mode;
  overrides.allow_dns_over_https_upgrade =
      defaults.allow_dns_over_https_upgrade;
  overrides.clear_hosts = true;

  return overrides;
}

bool DnsConfigOverrides::OverridesEverything() const {
  return nameservers && dns_over_tls_active && dns_over_tls_hostname &&
         search && append_to_multi_label_name && ndots && fallback_period &&
         attempts && doh_attempts && rotate && use_local_ipv6 &&
         dns_over_https_config && secure_dns_mode &&
         allow_dns_over_https_upgrade && clear_hosts;
}

DnsConfig DnsConfigOverrides::ApplyOverrides(const DnsConfig& config) const {
  DnsConfig overridden;

  if (!OverridesEverything())
    overridden = config;

  if (nameservers)
    overridden.nameservers = nameservers.value();
  if (dns_over_tls_active)
    overridden.dns_over_tls_active = dns_over_tls_active.value();
  if (dns_over_tls_hostname)
    overridden.dns_over_tls_hostname = dns_over_tls_hostname.value();
  if (search)
    overridden.search = search.value();
  if (append_to_multi_label_name)
    overridden.append_to_multi_label_name = append_to_multi_label_name.value();
  if (ndots)
    overridden.ndots = ndots.value();
  if (fallback_period)
    overridden.fallback_period = fallback_period.value();
  if (attempts)
    overridden.attempts = attempts.value();
  if (doh_attempts)
    overridden.doh_attempts = doh_attempts.value();
  if (rotate)
    overridden.rotate = rotate.value();
  if (use_local_ipv6)
    overridden.use_local_ipv6 = use_local_ipv6.value();
  if (dns_over_https_config)
    overridden.doh_config = dns_over_https_config.value();
  if (secure_dns_mode)
    overridden.secure_dns_mode = secure_dns_mode.value();
  if (allow_dns_over_https_upgrade) {
    overridden.allow_dns_over_https_upgrade =
        allow_dns_over_https_upgrade.value();
  }
  if (clear_hosts)
    overridden.hosts.clear();

  return overridden;
}

}  // namespace net
