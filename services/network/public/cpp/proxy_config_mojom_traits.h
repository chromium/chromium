// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "url/mojom/scheme_host_port_mojom_traits.h"

// This file handles the serialization of net::ProxyConfig.

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyHostMatchingRulesDataView,
                 net::ProxyHostMatchingRules> {
 public:
  static std::vector<std::string> rules(const net::ProxyHostMatchingRules& r);
  static bool Read(network::mojom::ProxyHostMatchingRulesDataView data,
                   net::ProxyHostMatchingRules* out_proxy_bypass_rules);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyListDataView, net::ProxyList> {
 public:
  static const std::vector<net::ProxyChain>& proxies(const net::ProxyList& r) {
    return r.AllChains();
  }
  static bool Read(network::mojom::ProxyListDataView data,
                   net::ProxyList* out_proxy_list);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    EnumTraits<network::mojom::ProxyRulesType,
               net::ProxyConfig::ProxyRules::Type> {
 public:
  static network::mojom::ProxyRulesType ToMojom(
      net::ProxyConfig::ProxyRules::Type net_proxy_rules_type);
  static bool FromMojom(network::mojom::ProxyRulesType mojo_proxy_rules_type,
                        net::ProxyConfig::ProxyRules::Type* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    EnumTraits<network::mojom::ProxyOverrideRuleResult,
               net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result> {
 public:
  static network::mojom::ProxyOverrideRuleResult ToMojom(
      net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result result);
  static bool FromMojom(
      network::mojom::ProxyOverrideRuleResult mojom_result,
      net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyRulesDataView,
                 net::ProxyConfig::ProxyRules> {
 public:
  static const net::ProxyHostMatchingRules& bypass_rules(
      const net::ProxyConfig::ProxyRules& r) {
    return r.bypass_rules;
  }
  static bool reverse_bypass(const net::ProxyConfig::ProxyRules& r) {
    return r.reverse_bypass;
  }
  static net::ProxyConfig::ProxyRules::Type type(
      const net::ProxyConfig::ProxyRules& r) {
    return r.type;
  }
  static const net::ProxyList& single_proxies(
      const net::ProxyConfig::ProxyRules& r) {
    return r.single_proxies;
  }
  static const net::ProxyList& proxies_for_http(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_http;
  }
  static const net::ProxyList& proxies_for_https(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_https;
  }
  static const net::ProxyList& proxies_for_ftp(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_ftp;
  }
  static const net::ProxyList& fallback_proxies(
      const net::ProxyConfig::ProxyRules& r) {
    return r.fallback_proxies;
  }

  static bool Read(network::mojom::ProxyRulesDataView data,
                   net::ProxyConfig::ProxyRules* out_proxy_rules);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::DnsProbeConditionDataView,
                 net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition> {
  static const url::SchemeHostPort& host(
      const net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition& r) {
    return r.host;
  }
  static net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result result(
      const net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition& r) {
    return r.result;
  }

  static bool Read(network::mojom::DnsProbeConditionDataView data,
                   net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyOverrideRuleDataView,
                 net::ProxyConfig::ProxyOverrideRule> {
  static net::ProxyHostMatchingRules destination_matchers(
      const net::ProxyConfig::ProxyOverrideRule& r) {
    return r.destination_matchers;
  }
  static net::ProxyHostMatchingRules exclude_destination_matchers(
      const net::ProxyConfig::ProxyOverrideRule& r) {
    return r.exclude_destination_matchers;
  }
  static const std::vector<
      net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition>&
  dns_conditions(const net::ProxyConfig::ProxyOverrideRule& r) {
    return r.dns_conditions;
  }
  static const net::ProxyList& proxy_list(
      const net::ProxyConfig::ProxyOverrideRule& r) {
    return r.proxy_list;
  }

  static bool Read(network::mojom::ProxyOverrideRuleDataView data,
                   net::ProxyConfig::ProxyOverrideRule* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyConfigDataView, net::ProxyConfig> {
 public:
  static bool auto_detect(const net::ProxyConfig& r) { return r.auto_detect(); }
  static bool from_system(const net::ProxyConfig& r) { return r.from_system(); }
  static const std::string& pac_url(const net::ProxyConfig& r) {
    return r.pac_url().possibly_invalid_spec();
  }
  static bool pac_mandatory(const net::ProxyConfig& r) {
    return r.pac_mandatory();
  }
  static const net::ProxyConfig::ProxyRules& proxy_rules(
      const net::ProxyConfig& r) {
    return r.proxy_rules();
  }
  static const std::vector<net::ProxyConfig::ProxyOverrideRule>&
  proxy_override_rules(const net::ProxyConfig& r) {
    return r.proxy_override_rules();
  }
  static bool Read(network::mojom::ProxyConfigDataView data,
                   net::ProxyConfig* out_proxy_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_MOJOM_TRAITS_H_
