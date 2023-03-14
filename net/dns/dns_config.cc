// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "net/dns/public/dns_over_https_config.h"

namespace net {

// Default values are taken from glibc resolv.h except |fallback_period| which
// is set to |kDnsDefaultFallbackPeriod|.
DnsConfig::DnsConfig() : DnsConfig(std::vector<IPEndPoint>()) {}

DnsConfig::DnsConfig(const DnsConfig& other) = default;

DnsConfig::DnsConfig(DnsConfig&& other) = default;

DnsConfig::DnsConfig(std::vector<IPEndPoint> nameservers)
    : nameservers(std::move(nameservers)) {}

DnsConfig::~DnsConfig() = default;

DnsConfig& DnsConfig::operator=(const DnsConfig& other) = default;

DnsConfig& DnsConfig::operator=(DnsConfig&& other) = default;

bool DnsConfig::Equals(const DnsConfig& d) const {
  return EqualsIgnoreHosts(d) && (hosts == d.hosts);
}

bool DnsConfig::operator==(const DnsConfig& d) const {
  return Equals(d);
}

bool DnsConfig::operator!=(const DnsConfig& d) const {
  return !Equals(d);
}

bool DnsConfig::EqualsIgnoreHosts(const DnsConfig& d) const {
  return (nameservers == d.nameservers) &&
         (dns_over_tls_active == d.dns_over_tls_active) &&
         (dns_over_tls_hostname == d.dns_over_tls_hostname) &&
         (search == d.search) && (unhandled_options == d.unhandled_options) &&
         (append_to_multi_label_name == d.append_to_multi_label_name) &&
         (ndots == d.ndots) && (fallback_period == d.fallback_period) &&
         (attempts == d.attempts) && (doh_attempts == d.doh_attempts) &&
         (rotate == d.rotate) && (use_local_ipv6 == d.use_local_ipv6) &&
         (doh_config == d.doh_config) &&
         (secure_dns_mode == d.secure_dns_mode) &&
         (allow_dns_over_https_upgrade == d.allow_dns_over_https_upgrade);
}

void DnsConfig::CopyIgnoreHosts(const DnsConfig& d) {
  nameservers = d.nameservers;
  dns_over_tls_active = d.dns_over_tls_active;
  dns_over_tls_hostname = d.dns_over_tls_hostname;
  search = d.search;
  unhandled_options = d.unhandled_options;
  append_to_multi_label_name = d.append_to_multi_label_name;
  ndots = d.ndots;
  fallback_period = d.fallback_period;
  attempts = d.attempts;
  doh_attempts = d.doh_attempts;
  rotate = d.rotate;
  use_local_ipv6 = d.use_local_ipv6;
  doh_config = d.doh_config;
  secure_dns_mode = d.secure_dns_mode;
  allow_dns_over_https_upgrade = d.allow_dns_over_https_upgrade;
}

base::Value::Dict DnsConfig::ToDict() const {
  base::Value::Dict dict;

  base::Value::List nameserver_list;
  for (const auto& nameserver : nameservers)
    nameserver_list.Append(nameserver.ToString());
  dict.Set("nameservers", std::move(nameserver_list));

  dict.Set("dns_over_tls_active", dns_over_tls_active);
  dict.Set("dns_over_tls_hostname", dns_over_tls_hostname);

  base::Value::List suffix_list;
  for (const auto& suffix : search)
    suffix_list.Append(suffix);
  dict.Set("search", std::move(suffix_list));
  dict.Set("unhandled_options", unhandled_options);
  dict.Set("append_to_multi_label_name", append_to_multi_label_name);
  dict.Set("ndots", ndots);
  dict.Set("timeout", fallback_period.InSecondsF());
  dict.Set("attempts", attempts);
  dict.Set("doh_attempts", doh_attempts);
  dict.Set("rotate", rotate);
  dict.Set("use_local_ipv6", use_local_ipv6);
  dict.Set("num_hosts", static_cast<int>(hosts.size()));
  dict.Set("doh_config", doh_config.ToValue());
  dict.Set("secure_dns_mode", base::strict_cast<int>(secure_dns_mode));
  dict.Set("allow_dns_over_https_upgrade", allow_dns_over_https_upgrade);

  return dict;
}

}  // namespace net
