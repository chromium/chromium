// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config.h"

#include <utility>

#include "base/values.h"

namespace net {

// Default values are taken from glibc resolv.h except timeout which is set to
// |kDnsDefaultTimeoutMs|.
DnsConfig::DnsConfig()
    : unhandled_options(false),
      append_to_multi_label_name(true),
      randomize_ports(false),
      ndots(1),
      timeout(kDnsDefaultTimeout),
      attempts(2),
      rotate(false),
      use_local_ipv6(false) {}

DnsConfig::DnsConfig(const DnsConfig& other) = default;

DnsConfig::DnsConfig(DnsConfig&& other) = default;

DnsConfig::~DnsConfig() = default;

DnsConfig& DnsConfig::operator=(const DnsConfig& other) = default;

DnsConfig& DnsConfig::operator=(DnsConfig&& other) = default;

bool DnsConfig::Equals(const DnsConfig& d) const {
  return EqualsIgnoreHosts(d) && (hosts == d.hosts);
}

bool DnsConfig::EqualsIgnoreHosts(const DnsConfig& d) const {
  return (nameservers == d.nameservers) && (search == d.search) &&
         (unhandled_options == d.unhandled_options) &&
         (append_to_multi_label_name == d.append_to_multi_label_name) &&
         (ndots == d.ndots) && (timeout == d.timeout) &&
         (attempts == d.attempts) && (rotate == d.rotate) &&
         (use_local_ipv6 == d.use_local_ipv6) &&
         (dns_over_https_servers == d.dns_over_https_servers);
}

void DnsConfig::CopyIgnoreHosts(const DnsConfig& d) {
  nameservers = d.nameservers;
  search = d.search;
  unhandled_options = d.unhandled_options;
  append_to_multi_label_name = d.append_to_multi_label_name;
  ndots = d.ndots;
  timeout = d.timeout;
  attempts = d.attempts;
  rotate = d.rotate;
  use_local_ipv6 = d.use_local_ipv6;
  dns_over_https_servers = d.dns_over_https_servers;
}

std::unique_ptr<base::Value> DnsConfig::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();

  auto list = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < nameservers.size(); ++i)
    list->AppendString(nameservers[i].ToString());
  dict->Set("nameservers", std::move(list));

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
    list->GetList().push_back(std::move(val));
  }
  dict->Set("doh_servers", std::move(list));

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
