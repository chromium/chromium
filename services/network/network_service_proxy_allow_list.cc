
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/network_service_proxy_allow_list.h"
#include <memory>
#include "base/command_line.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {
namespace {
// The temporary header name expected by the envoy proxy configuration.
const char kIPAnonymizationProxyHeaderName[] = "password";
std::string NormalizeHost(std::string s) {
  return s.substr(0, 4) == "www." ? s.substr(4) : s;
}
}  // namespace
NetworkServiceProxyAllowList::NetworkServiceProxyAllowList() {
  custom_proxy_config_ = network::mojom::CustomProxyConfig::New();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string ip_protection_proxy_server =
      command_line.HasSwitch(network::switches::kIPAnonymizationProxyServer)
          ? command_line.GetSwitchValueASCII(
                network::switches::kIPAnonymizationProxyServer)
          : net::features::kIpPrivacyProxyServer.Get();

  custom_proxy_config_->rules.ParseFromString(ip_protection_proxy_server);

  custom_proxy_config_->rules.restrict_to_network_service_proxy_allow_list =
      true;
  custom_proxy_config_->should_replace_direct = true;
  custom_proxy_config_->should_override_existing_config = false;
  custom_proxy_config_->allow_non_idempotent_methods = true;

  custom_proxy_config_->connect_tunnel_headers.SetHeader(
      kIPAnonymizationProxyHeaderName,
      command_line.GetSwitchValueASCII(
          network::switches::kIPAnonymizationProxyPassword));
}

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList(
    std::map<std::string, net::ProxyBypassRules> first_party_exclusion_map)
    : allow_list_with_bypass_map_(first_party_exclusion_map) {}

NetworkServiceProxyAllowList::~NetworkServiceProxyAllowList() = default;

NetworkServiceProxyAllowList NetworkServiceProxyAllowList::CreateForTesting(
    std::map<std::string, std::set<std::string>> first_party_map) {
  std::map<std::string, net::ProxyBypassRules> allow_list_with_bypass_map;

  for (auto const& entry : first_party_map) {
    net::ProxyBypassRules bypass_rules;
    for (auto property : first_party_map.at(entry.first)) {
      CHECK(bypass_rules.AddRuleFromString(property));
      CHECK(bypass_rules.AddRuleFromString("." + property));
    }

    allow_list_with_bypass_map[entry.first] = bypass_rules;
  }

  return NetworkServiceProxyAllowList(allow_list_with_bypass_map);
}

bool NetworkServiceProxyAllowList::IsEnabled() {
  return !allow_list_with_bypass_map_.empty() &&
         base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy);
}

mojom::CustomProxyConfigPtr
NetworkServiceProxyAllowList::GetCustomProxyConfig() {
  return custom_proxy_config_ ? custom_proxy_config_->Clone() : nullptr;
}

bool NetworkServiceProxyAllowList::Matches(const GURL& request_url,
                                           const GURL& top_frame_url) {
  // If there is no top frame URL, the request should not be proxied because it
  // is not to a 3P resource.
  if (top_frame_url.is_empty()) {
    return false;
  }

  std::string resource_host = NormalizeHost(request_url.host());

  // Same site requests should not be proxied.
  if (resource_host == NormalizeHost(top_frame_url.host())) {
    return false;
  }

  if (allow_list_with_bypass_map_.contains(resource_host)) {
    // TODO(aakallam): match subdomains
    return allow_list_with_bypass_map_.at(resource_host)
        .Matches(top_frame_url, true);
  }

  return false;
}

void NetworkServiceProxyAllowList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl) {
  // For quick lookup, map each proxyable resource to the set of top frame
  // domains that will allow proxy bypass.
  allow_list_with_bypass_map_.clear();
  for (auto owner : mdl.resource_owners()) {
    net::ProxyBypassRules bypass_rules;
    for (auto resource : owner.owned_resources()) {
      for (auto property : owner.owned_properties()) {
        CHECK(bypass_rules.AddRuleFromString(property));
        // We also want to proxy any subdomains
        CHECK(bypass_rules.AddRuleFromString("." + property));
      }

      allow_list_with_bypass_map_[resource.domain()] = bypass_rules;
    }
  }
}

}  // namespace network
