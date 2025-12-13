// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/proxy_config_mojom_traits.h"

#include "base/debug/dump_without_crashing.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"
#include "url/gurl.h"

namespace mojo {

std::vector<std::string> StructTraits<
    network::mojom::ProxyHostMatchingRulesDataView,
    net::ProxyHostMatchingRules>::rules(const net::ProxyHostMatchingRules& r) {
  std::vector<std::string> out;
  for (const auto& rule : r.rules()) {
    out.push_back(rule->ToString());
  }
  return out;
}

bool StructTraits<network::mojom::ProxyHostMatchingRulesDataView,
                  net::ProxyHostMatchingRules>::
    Read(network::mojom::ProxyHostMatchingRulesDataView data,
         net::ProxyHostMatchingRules* out_proxy_bypass_rules) {
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

bool StructTraits<network::mojom::ProxyListDataView, net::ProxyList>::Read(
    network::mojom::ProxyListDataView data,
    net::ProxyList* out_proxy_list) {
  std::vector<net::ProxyChain> proxy_chains;
  if (!data.ReadProxies(&proxy_chains)) {
    return false;
  }
  for (auto& proxy_chain : proxy_chains) {
    out_proxy_list->AddProxyChain(std::move(proxy_chain));
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

network::mojom::ProxyOverrideRuleResult
EnumTraits<network::mojom::ProxyOverrideRuleResult,
           net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result>::
    ToMojom(
        net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result result) {
  switch (result) {
    case net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound:
      return network::mojom::ProxyOverrideRuleResult::kNotFound;
    case net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved:
      return network::mojom::ProxyOverrideRuleResult::kResolved;
  }
}

bool EnumTraits<
    network::mojom::ProxyOverrideRuleResult,
    net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result>::
    FromMojom(
        network::mojom::ProxyOverrideRuleResult mojom_result,
        net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result* out) {
  switch (mojom_result) {
    case network::mojom::ProxyOverrideRuleResult::kNotFound:
      *out = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::
          kNotFound;
      return true;
    case network::mojom::ProxyOverrideRuleResult::kResolved:
      *out = net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::Result::
          kResolved;
      return true;
  }
  return false;
}

bool StructTraits<network::mojom::ProxyRulesDataView,
                  net::ProxyConfig::ProxyRules>::
    Read(network::mojom::ProxyRulesDataView data,
         net::ProxyConfig::ProxyRules* out_proxy_rules) {
  out_proxy_rules->reverse_bypass = data.reverse_bypass();
  return data.ReadBypassRules(&out_proxy_rules->bypass_rules) &&
         data.ReadType(&out_proxy_rules->type) &&
         data.ReadSingleProxies(&out_proxy_rules->single_proxies) &&
         data.ReadProxiesForHttp(&out_proxy_rules->proxies_for_http) &&
         data.ReadProxiesForHttps(&out_proxy_rules->proxies_for_https) &&
         data.ReadProxiesForFtp(&out_proxy_rules->proxies_for_ftp) &&
         data.ReadFallbackProxies(&out_proxy_rules->fallback_proxies);
}

bool StructTraits<network::mojom::DnsProbeConditionDataView,
                  net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition>::
    Read(network::mojom::DnsProbeConditionDataView data,
         net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition* out) {
  return data.ReadHost(&out->host) && data.ReadResult(&out->result);
}

bool StructTraits<network::mojom::ProxyOverrideRuleDataView,
                  net::ProxyConfig::ProxyOverrideRule>::
    Read(network::mojom::ProxyOverrideRuleDataView data,
         net::ProxyConfig::ProxyOverrideRule* out) {
  return data.ReadDestinationMatchers(&out->destination_matchers) &&
         data.ReadExcludeDestinationMatchers(
             &out->exclude_destination_matchers) &&
         data.ReadProxyList(&out->proxy_list) &&
         data.ReadDnsConditions(&out->dns_conditions) &&
         !out->destination_matchers.rules().empty() &&
         !out->proxy_list.IsEmpty();
}

bool StructTraits<network::mojom::ProxyConfigDataView, net::ProxyConfig>::Read(
    network::mojom::ProxyConfigDataView data,
    net::ProxyConfig* out_proxy_config) {
  std::vector<net::ProxyConfig::ProxyOverrideRule> proxy_override_rules;
  if (!data.ReadProxyOverrideRules(&proxy_override_rules)) {
    return false;
  }
  out_proxy_config->set_proxy_override_rules(std::move(proxy_override_rules));

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
