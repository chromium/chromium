// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config.h"

#include <utility>

#include "base/values.h"

namespace net {

// Default values are taken from glibc resolv.h except timeout which is set to
// |kDnsDefaultTimeoutMs|.
DnsConfig::DnsConfig() : DnsConfig(std::vector<IPEndPoint>()) {}

DnsConfig::DnsConfig(const DnsConfig& other) = default;

DnsConfig::DnsConfig(DnsConfig&& other) = default;

DnsConfig::DnsConfig(std::vector<IPEndPoint> nameservers)
    : nameservers(std::move(nameservers)),
      dns_over_tls_active(false),
      dns_over_tls_hostname(std::string()),
      unhandled_options(false),
      append_to_multi_label_name(true),
      ndots(1),
      timeout(kDnsDefaultTimeout),
      attempts(2),
      doh_attempts(1),
      rotate(false),
      use_local_ipv6(false),
      secure_dns_mode(SecureDnsMode::kOff),
      allow_dns_over_https_upgrade(false) {}

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
         (ndots == d.ndots) && (timeout == d.timeout) &&
         (attempts == d.attempts) && (doh_attempts == d.doh_attempts) &&
         (rotate == d.rotate) && (use_local_ipv6 == d.use_local_ipv6) &&
         (dns_over_https_servers == d.dns_over_https_servers) &&
         (secure_dns_mode == d.secure_dns_mode) &&
         (allow_dns_over_https_upgrade == d.allow_dns_over_https_upgrade) &&
         (disabled_upgrade_providers == d.disabled_upgrade_providers);
}

void DnsConfig::CopyIgnoreHosts(const DnsConfig& d) {
  nameservers = d.nameservers;
  dns_over_tls_active = d.dns_over_tls_active;
  dns_over_tls_hostname = d.dns_over_tls_hostname;
  search = d.search;
  unhandled_options = d.unhandled_options;
  append_to_multi_label_name = d.append_to_multi_label_name;
  ndots = d.ndots;
  timeout = d.timeout;
  attempts = d.attempts;
  doh_attempts = d.doh_attempts;
  rotate = d.rotate;
  use_local_ipv6 = d.use_local_ipv6;
  dns_over_https_servers = d.dns_over_https_servers;
  secure_dns_mode = d.secure_dns_mode;
  allow_dns_over_https_upgrade = d.allow_dns_over_https_upgrade;
  disabled_upgrade_providers = d.disabled_upgrade_providers;
}

base::Value DnsConfig::ToValue() const {
  base::Value dict(base::Value::Type::DICTIONARY);

  base::Value list(base::Value::Type::LIST);
  for (const auto& nameserver : nameservers)
    list.Append(nameserver.ToString());
  dict.SetKey("nameservers", std::move(list));

  dict.SetBoolKey("dns_over_tls_active", dns_over_tls_active);
  dict.SetStringKey("dns_over_tls_hostname", dns_over_tls_hostname);

  list = base::Value(base::Value::Type::LIST);
  for (const auto& suffix : search)
    list.Append(suffix);
  dict.SetKey("search", std::move(list));
  dict.SetBoolKey("unhandled_options", unhandled_options);
  dict.SetBoolKey("append_to_multi_label_name", append_to_multi_label_name);
  dict.SetIntKey("ndots", ndots);
  dict.SetDoubleKey("timeout", timeout.InSecondsF());
  dict.SetIntKey("attempts", attempts);
  dict.SetIntKey("doh_attempts", doh_attempts);
  dict.SetBoolKey("rotate", rotate);
  dict.SetBoolKey("use_local_ipv6", use_local_ipv6);
  dict.SetIntKey("num_hosts", hosts.size());
  list = base::Value(base::Value::Type::LIST);
  for (auto& server : dns_over_https_servers) {
    base::Value val(base::Value::Type::DICTIONARY);
    val.SetStringKey("server_template", server.server_template);
    val.SetBoolKey("use_post", server.use_post);
    list.Append(std::move(val));
  }
  dict.SetKey("doh_servers", std::move(list));
  dict.SetIntKey("secure_dns_mode", static_cast<int>(secure_dns_mode));
  dict.SetBoolKey("allow_dns_over_https_upgrade", allow_dns_over_https_upgrade);

  list = base::Value(base::Value::Type::LIST);
  for (const auto& provider : disabled_upgrade_providers)
    list.Append(provider);
  dict.SetKey("disabled_upgrade_providers", std::move(list));

  return dict;
}

}  // namespace net
