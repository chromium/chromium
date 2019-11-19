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
      randomize_ports(false),
      ndots(1),
      timeout(kDnsDefaultTimeout),
      attempts(2),
      rotate(false),
      use_local_ipv6(false),
      secure_dns_mode(SecureDnsMode::OFF),
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
         (attempts == d.attempts) && (rotate == d.rotate) &&
         (use_local_ipv6 == d.use_local_ipv6) &&
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
  rotate = d.rotate;
  use_local_ipv6 = d.use_local_ipv6;
  dns_over_https_servers = d.dns_over_https_servers;
  secure_dns_mode = d.secure_dns_mode;
  allow_dns_over_https_upgrade = d.allow_dns_over_https_upgrade;
  disabled_upgrade_providers = d.disabled_upgrade_providers;
}

std::unique_ptr<base::Value> DnsConfig::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();

  auto list = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < nameservers.size(); ++i)
    list->AppendString(nameservers[i].ToString());
  dict->Set("nameservers", std::move(list));

  dict->SetBoolean("dns_over_tls_active", dns_over_tls_active);
  dict->SetString("dns_over_tls_hostname", dns_over_tls_hostname);

  list = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < search.size(); ++i)
    list->AppendString(search[i]);
  dict->Set("search", std::move(list));

  dict->SetBoolean("unhandled_options", unhandled_options);
  dict->SetBoolean("append_to_multi_label_name", append_to_multi_label_name);
  dict->SetInteger("ndots", ndots);
  dict->SetDouble("timeout", timeout.InSecondsF());
  dict->SetInteger("attempts", attempts);
  dict->SetBoolean("rotate", rotate);
  dict->SetBoolean("use_local_ipv6", use_local_ipv6);
  dict->SetInteger("num_hosts", hosts.size());
  list = std::make_unique<base::ListValue>();
  for (auto& server : dns_over_https_servers) {
    base::Value val(base::Value::Type::DICTIONARY);
    base::DictionaryValue* dict;
    val.GetAsDictionary(&dict);
    dict->SetString("server_template", server.server_template);
    dict->SetBoolean("use_post", server.use_post);
    list->Append(std::move(val));
  }
  dict->Set("doh_servers", std::move(list));
  dict->SetInteger("secure_dns_mode", static_cast<int>(secure_dns_mode));
  dict->SetBoolean("allow_dns_over_https_upgrade",
                   allow_dns_over_https_upgrade);

  list = std::make_unique<base::ListValue>();
  for (const auto& provider : disabled_upgrade_providers)
    list->AppendString(provider);
  dict->Set("disabled_upgrade_providers", std::move(list));

  return std::move(dict);
}

DnsConfig::DnsOverHttpsServerConfig::DnsOverHttpsServerConfig(
    const std::string& server_template,
    bool use_post)
    : server_template(server_template), use_post(use_post) {}

bool DnsConfig::DnsOverHttpsServerConfig::operator==(
    const DnsOverHttpsServerConfig& other) const {
  return server_template == other.server_template && use_post == other.use_post;
}

}  // namespace net
