// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/network_service_proxy_allow_list.h"
#include <memory>
#include "base/command_line.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {
namespace {

std::string NormalizeHost(std::string s) {
  return s.substr(0, 4) == "www." ? s.substr(4) : s;
}

// Extracts a suffix from the domain that is useful for comparing domains and
// subdomains.
std::string DomainSuffix(std::string domain) {
  auto host_suffix_start = domain.rfind(".", domain.rfind("."));
  return host_suffix_start != std::string::npos
             ? domain.substr(host_suffix_start)
             : domain;
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
}

NetworkServiceProxyAllowList::~NetworkServiceProxyAllowList() = default;

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList(
    const NetworkServiceProxyAllowList&) {}

NetworkServiceProxyAllowList NetworkServiceProxyAllowList::CreateForTesting(
    std::map<std::string, std::set<std::string>> first_party_map) {
  auto allow_list = NetworkServiceProxyAllowList();

  for (auto const& [domain, properties] : first_party_map) {
    net::ProxyBypassRules bypass_rules;
    for (auto property : properties) {
      CHECK(bypass_rules.AddRuleFromString(property));
      CHECK(bypass_rules.AddRuleFromString("." + property));
    }

    allow_list.AddDomainRules(domain, bypass_rules);
  }

  return allow_list;
}

bool NetworkServiceProxyAllowList::IsEnabled() {
  return base::FeatureList::IsEnabled(
             net::features::kEnableIpProtectionProxy) &&
         base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool NetworkServiceProxyAllowList::IsPopulated() {
  return !allow_list_with_bypass_map_.empty();
}

mojom::CustomProxyConfigPtr
NetworkServiceProxyAllowList::GetCustomProxyConfig() {
  return custom_proxy_config_ ? custom_proxy_config_->Clone() : nullptr;
}

void NetworkServiceProxyAllowList::AddDomainRules(
    const std::string& domain,
    const net::ProxyBypassRules& bypass_rules) {
  auto rule = net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);

  std::string domain_suffix = DomainSuffix(domain);

  if (rule) {
    allow_list_with_bypass_map_[domain_suffix][std::move(rule)] = bypass_rules;
  }

  // Only add rules for subdomains if the provided domain string doesn't support
  // them.
  if (!(domain.starts_with(".") || domain.starts_with("*"))) {
    auto subdomain_rule =
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString("." + domain);
    if (subdomain_rule) {
      allow_list_with_bypass_map_[domain_suffix][std::move(subdomain_rule)] =
          bypass_rules;
    }
  }
}

bool NetworkServiceProxyAllowList::Matches(const GURL& request_url,
                                           const GURL& top_frame_url) {
  // If there is no top frame URL, the request should not be proxied because it
  // is not to a 3P resource.
  if (!IsPopulated() || top_frame_url.is_empty()) {
    return false;
  }

  std::string resource_host = NormalizeHost(request_url.host());

  // Same site requests should not be proxied.
  if (resource_host == NormalizeHost(top_frame_url.host())) {
    return false;
  }

  auto resource_host_suffix = DomainSuffix(resource_host);

  if (allow_list_with_bypass_map_.contains(resource_host_suffix)) {
    for (const auto& [rule, bypass_rules] :
         allow_list_with_bypass_map_.at(resource_host_suffix)) {
      auto result = rule->Evaluate(request_url);
      if (result == net::SchemeHostPortMatcherResult::kInclude) {
        return bypass_rules.Matches(top_frame_url, true);
      }
    }
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
        // Also bypass proxy for any subdomains.
        CHECK(bypass_rules.AddRuleFromString("." + property));
      }

      AddDomainRules(resource.domain(), bypass_rules);
    }
  }
}

}  // namespace network
