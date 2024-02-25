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
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"

// This file handles the serialization of net::ProxyConfig.

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_PROXY_CONFIG)
    StructTraits<network::mojom::ProxyBypassRulesDataView,
                 net::ProxyBypassRules> {
 public:
  static std::vector<std::string> rules(const net::ProxyBypassRules& r);
  static bool Read(network::mojom::ProxyBypassRulesDataView data,
                   net::ProxyBypassRules* out_proxy_bypass_rules);
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
    StructTraits<network::mojom::ProxyRulesDataView,
                 net::ProxyConfig::ProxyRules> {
 public:
  static const net::ProxyBypassRules& bypass_rules(
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
  static bool Read(network::mojom::ProxyConfigDataView data,
                   net::ProxyConfig* out_proxy_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_MOJOM_TRAITS_H_
