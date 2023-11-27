// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/proxy_config_mojom_traits.h"

#include "base/debug/dump_without_crashing.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "net/base/proxy_string_util.h"
#include "url/gurl.h"

namespace mojo {

std::vector<std::string>
StructTraits<network::mojom::ProxyBypassRulesDataView,
             net::ProxyBypassRules>::rules(const net::ProxyBypassRules& r) {
  std::vector<std::string> out;
  for (const auto& rule : r.rules()) {
    out.push_back(rule->ToString());
  }
  return out;
}

bool StructTraits<network::mojom::ProxyBypassRulesDataView,
                  net::ProxyBypassRules>::
    Read(network::mojom::ProxyBypassRulesDataView data,
         net::ProxyBypassRules* out_proxy_bypass_rules) {
  std::vector<std::string> rules;
  if (!data.ReadRules(&rules))
    return false;
  for (const auto& rule : rules) {
    if (!out_proxy_bypass_rules->AddRuleFromString(rule)) {
      mojo::debug::ScopedMessageErrorCrashKey crash_key_value(
          "AddRuleFromString fault");
      base::debug::DumpWithoutCrashing();
      return false;
    }
  }
  return true;
}

std::vector<std::vector<std::string>>
StructTraits<network::mojom::ProxyListDataView, net::ProxyList>::proxies(
    const net::ProxyList& r) {
  std::vector<std::vector<std::string>> out;
  for (const auto& proxy_chain : r.AllChains()) {
    std::vector<std::string> proxy_servers;
    for (const auto& proxy : proxy_chain.proxy_servers()) {
      proxy_servers.push_back(net::ProxyServerToPacResultElement(proxy));
    }
    out.push_back(std::move(proxy_servers));
  }
  return out;
}

bool StructTraits<network::mojom::ProxyListDataView, net::ProxyList>::Read(
    network::mojom::ProxyListDataView data,
    net::ProxyList* out_proxy_list) {
  std::vector<std::vector<std::string>> proxy_chains;
  if (!data.ReadProxies(&proxy_chains)) {
    return false;
  }
  for (const auto& proxy_chain : proxy_chains) {
    std::vector<net::ProxyServer> proxy_servers;
    for (const auto& proxy : proxy_chain) {
      net::ProxyServer proxy_server = net::PacResultElementToProxyServer(proxy);
      if (!proxy_server.is_valid()) {
        mojo::debug::ScopedMessageErrorCrashKey crash_key_value(
            "!proxy_server.is_valid()");
        base::debug::DumpWithoutCrashing();
        return false;
      }
      proxy_servers.push_back(std::move(proxy_server));
    }
    out_proxy_list->AddProxyChain(net::ProxyChain(proxy_servers));
  }
  return true;
}

network::mojom::ProxyRulesType
EnumTraits<network::mojom::ProxyRulesType, net::ProxyConfig::ProxyRules::Type>::
    ToMojom(net::ProxyConfig::ProxyRules::Type net_proxy_rules_type) {
  switch (net_proxy_rules_type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return network::mojom::ProxyRulesType::EMPTY;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      return network::mojom::ProxyRulesType::PROXY_LIST;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      return network::mojom::ProxyRulesType::PROXY_LIST_PER_SCHEME;
  }
  return network::mojom::ProxyRulesType::EMPTY;
}

bool EnumTraits<network::mojom::ProxyRulesType,
                net::ProxyConfig::ProxyRules::Type>::
    FromMojom(network::mojom::ProxyRulesType mojo_proxy_rules_type,
              net::ProxyConfig::ProxyRules::Type* out) {
  switch (mojo_proxy_rules_type) {
    case network::mojom::ProxyRulesType::EMPTY:
      *out = net::ProxyConfig::ProxyRules::Type::EMPTY;
      return true;
    case network::mojom::ProxyRulesType::PROXY_LIST:
      *out = net::ProxyConfig::ProxyRules::Type::PROXY_LIST;
      return true;
    case network::mojom::ProxyRulesType::PROXY_LIST_PER_SCHEME:
      *out = net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
      return true;
  }
  return false;
}

bool StructTraits<network::mojom::ProxyRulesDataView,
                  net::ProxyConfig::ProxyRules>::
    Read(network::mojom::ProxyRulesDataView data,
         net::ProxyConfig::ProxyRules* out_proxy_rules) {
  out_proxy_rules->reverse_bypass = data.reverse_bypass();
  out_proxy_rules->restrict_to_network_service_proxy_allow_list =
      data.restrict_to_network_service_proxy_allow_list();
  return data.ReadBypassRules(&out_proxy_rules->bypass_rules) &&
         data.ReadType(&out_proxy_rules->type) &&
         data.ReadSingleProxies(&out_proxy_rules->single_proxies) &&
         data.ReadProxiesForHttp(&out_proxy_rules->proxies_for_http) &&
         data.ReadProxiesForHttps(&out_proxy_rules->proxies_for_https) &&
         data.ReadProxiesForFtp(&out_proxy_rules->proxies_for_ftp) &&
         data.ReadFallbackProxies(&out_proxy_rules->fallback_proxies);
}


bool StructTraits<network::mojom::ProxyConfigDataView, net::ProxyConfig>::Read(
    network::mojom::ProxyConfigDataView data,
    net::ProxyConfig* out_proxy_config) {
  std::string pac_url;
  if (!data.ReadPacUrl(&pac_url) ||
      !data.ReadProxyRules(&out_proxy_config->proxy_rules())) {
    return false;
  }
  out_proxy_config->set_pac_url(GURL(pac_url));

  out_proxy_config->set_auto_detect(data.auto_detect());
  out_proxy_config->set_pac_mandatory(data.pac_mandatory());
  out_proxy_config->set_from_system(data.from_system());
  return true;
}

}  // namespace mojo
