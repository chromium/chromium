// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/url_matcher_with_bypass.h"

#include "base/logging.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/schemeful_site.h"

namespace network {

namespace {

bool HasSubdomainCoverage(std::string domain) {
  return domain.starts_with(".") || domain.starts_with("*");
}

void AddBypassRulesForDomain(net::SchemeHostPortMatcher& bypass_matcher,
                             const std::string& domain) {
  bypass_matcher.AddAsFirstRule(
      net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain));
  if (!HasSubdomainCoverage(domain)) {
    bypass_matcher.AddAsFirstRule(
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString("." + domain));
  }
}

net::SchemeHostPortMatcher BuildBypassMatcher(
    masked_domain_list::ResourceOwner resource_owner) {
  net::SchemeHostPortMatcher bypass_matcher;

  for (auto property : resource_owner.owned_properties()) {
    AddBypassRulesForDomain(bypass_matcher, property);
  }
  for (auto resource : resource_owner.owned_resources()) {
    AddBypassRulesForDomain(bypass_matcher, resource.domain());
  }
  return bypass_matcher;
}
}  // namespace

// static
std::string UrlMatcherWithBypass::PartitionMapKey(std::string domain) {
  auto last_dot = domain.rfind(".");
  if (last_dot != std::string::npos) {
    auto penultimate_dot = domain.rfind(".", last_dot - 1);
    if (penultimate_dot != std::string::npos) {
      return domain.substr(penultimate_dot + 1);
    }
  }
  return domain;
}

UrlMatcherWithBypass::UrlMatcherWithBypass() = default;
UrlMatcherWithBypass::~UrlMatcherWithBypass() = default;

void UrlMatcherWithBypass::AddDomainWithBypass(
    const std::string& domain,
    net::SchemeHostPortMatcher bypass_matcher) {
  auto rule = net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);
  std::string domain_suffix = PartitionMapKey(domain);

  if (rule) {
    match_list_with_bypass_map_[domain_suffix][std::move(rule)] =
        std::move(bypass_matcher);
  }
}

void UrlMatcherWithBypass::AddMaskedDomainListRules(
    const std::string& domain,
    masked_domain_list::ResourceOwner resource_owner) {
  AddDomainWithBypass(domain, BuildBypassMatcher(resource_owner));

  // Only add rules for subdomains if the provided domain string doesn't support
  // them.
  if (!HasSubdomainCoverage(domain)) {
    AddDomainWithBypass("." + domain, BuildBypassMatcher(resource_owner));
  }
}

void UrlMatcherWithBypass::Clear() {
  match_list_with_bypass_map_.clear();
}

size_t UrlMatcherWithBypass::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(match_list_with_bypass_map_);
}

bool UrlMatcherWithBypass::IsPopulated() {
  return !match_list_with_bypass_map_.empty();
}

bool UrlMatcherWithBypass::Matches(const GURL& request_url,
                                   const GURL& top_frame_url) {
  auto vlog = [&](std::string message) {
    VLOG(3) << "UrlMatcherWithBypass::Matches(" << request_url << ", "
            << top_frame_url << ") - " << message;
  };

  // If there is no top frame URL, the matches cannot be performed.
  if (!IsPopulated() || top_frame_url.is_empty()) {
    vlog("false (not populated or empty top_frame_url)");
    return false;
  }

  net::SchemefulSite request_site(request_url);
  net::SchemefulSite top_site(top_frame_url);

  // First-party requests are not proxied/blocked.
  if (request_site == top_site) {
    vlog("false (same-site)");
    return false;
  }

  auto resource_host_suffix = PartitionMapKey(request_url.host());

  if (match_list_with_bypass_map_.contains(resource_host_suffix)) {
    for (const auto& [rule, bypass_matcher] :
         match_list_with_bypass_map_.at(resource_host_suffix)) {
      auto result = rule->Evaluate(request_url);
      if (result == net::SchemeHostPortMatcherResult::kInclude) {
        bool m = bypass_matcher.Evaluate(top_frame_url) ==
                 net::SchemeHostPortMatcherResult::kNoMatch;
        if (m) {
          vlog("true from bypass_matcher.Matches");
        } else {
          vlog("false from bypass_matcher.Matches");
        }
        return m;
      }
    }
  }

  vlog("false (fall-through)");
  return false;
}

}  // namespace network
