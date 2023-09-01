// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "services/network/network_service_proxy_allow_list.h"

#include "base/strings/strcat.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/public/cpp/features.h"

namespace network {
namespace {

void AddBypassRulesForDomain(net::ProxyBypassRules& bypass_rules,
                             const std::string& domain) {
  CHECK(bypass_rules.AddRuleFromString(domain));
  if (!(domain.starts_with(".") || domain.starts_with("*"))) {
    // Also bypass proxy for any subdomains.
    CHECK(bypass_rules.AddRuleFromString("." + domain));
  }
}

net::ProxyBypassRules BuildBypassRules(
    const masked_domain_list::ResourceOwner& resource_owner) {
  net::ProxyBypassRules bypass_rules;
  for (auto resource : resource_owner.owned_resources()) {
    AddBypassRulesForDomain(bypass_rules, resource.domain());
  }
  for (auto property : resource_owner.owned_properties()) {
    AddBypassRulesForDomain(bypass_rules, property);
  }
  return bypass_rules;
}

}  // namespace

NetworkServiceProxyAllowList::NetworkServiceProxyAllowList() = default;
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

// static
mojom::CustomProxyConfigPtr
NetworkServiceProxyAllowList::MakeIpProtectionCustomProxyConfig() {
  auto custom_proxy_config = network::mojom::CustomProxyConfig::New();
  // Indicate to the NetworkServiceProxyDelegate that this is for IP Protection
  // and it should use the allow list. In this situation, the delegate does not
  // use any other fields from the custom proxy config.
  custom_proxy_config->rules.restrict_to_network_service_proxy_allow_list =
      true;
  return custom_proxy_config;
}

// static
std::string NetworkServiceProxyAllowList::PartitionMapKey(std::string domain) {
  auto last_dot = domain.rfind(".");
  if (last_dot != std::string::npos) {
    auto penultimate_dot = domain.rfind(".", last_dot - 1);
    if (penultimate_dot != std::string::npos) {
      return domain.substr(penultimate_dot + 1);
    }
  }
  return domain;
}

void NetworkServiceProxyAllowList::AddDomainRules(
    const std::string& domain,
    const net::ProxyBypassRules& bypass_rules) {
  auto rule = net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);

  std::string domain_suffix = PartitionMapKey(domain);

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
  auto vlog = [&](std::string message) {
    VLOG(3) << "NSPAL::Matches(" << request_url << ", " << top_frame_url
            << ") - " << message;
  };
  // If there is no top frame URL, the request should not be proxied because it
  // is not to a 3P resource.
  if (!IsPopulated() || top_frame_url.is_empty()) {
    vlog("false (not populated or empty top_frame_url)");
    return false;
  }

  net::SchemefulSite request_site(request_url);
  net::SchemefulSite top_site(top_frame_url);

  // First-party requests should not be proxied.
  if (request_site == top_site) {
    VLOG(2) << " -> false (same-site)";
    return false;
  }

  auto partition_map_key = PartitionMapKey(request_url.host());

  if (allow_list_with_bypass_map_.contains(partition_map_key)) {
    for (const auto& [rule, bypass_rules] :
         allow_list_with_bypass_map_.at(partition_map_key)) {
      auto result = rule->Evaluate(request_url);
      if (result == net::SchemeHostPortMatcherResult::kInclude) {
        bool m = bypass_rules.Matches(top_frame_url, true);
        if (m) {
          vlog("true from bypass_rules.Matches");
        } else {
          vlog("false from bypass_rules.Matches");
        }
        return m;
      }
    }
  }

  vlog("false (fall-through)");
  return false;
}

void NetworkServiceProxyAllowList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl) {
  // For quick lookup, map each proxyable resource to the set of top frame
  // domains that will allow proxy bypass.
  allow_list_with_bypass_map_.clear();
  for (auto owner : mdl.resource_owners()) {
    net::ProxyBypassRules bypass_rules = BuildBypassRules(owner);
    for (auto resource : owner.owned_resources()) {
      AddDomainRules(resource.domain(), bypass_rules);
    }
  }
}

}  // namespace network
