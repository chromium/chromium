// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/url_matcher_with_bypass.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/schemeful_site.h"
#include "url_matcher_with_bypass.h"

namespace network {

namespace {

bool HasSubdomainCoverage(std::string_view domain) {
  return domain.starts_with(".") || domain.starts_with("*");
}

void AddBypassRulesForDomain(net::SchemeHostPortMatcher& bypass_matcher,
                             std::string_view domain) {
  bypass_matcher.AddAsFirstRule(
      net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain));
  if (!HasSubdomainCoverage(domain)) {
    bypass_matcher.AddAsFirstRule(
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString(
            base::StrCat({".", domain})));
  }
}

net::SchemeHostPortMatcher BuildBypassMatcher(
    const masked_domain_list::ResourceOwner& resource_owner) {
  net::SchemeHostPortMatcher bypass_matcher;

  for (std::string_view property : resource_owner.owned_properties()) {
    AddBypassRulesForDomain(bypass_matcher, property);
  }
  for (const masked_domain_list::Resource& resource :
       resource_owner.owned_resources()) {
    AddBypassRulesForDomain(bypass_matcher, resource.domain());
  }
  return bypass_matcher;
}

void AddRulesToMatcher(net::SchemeHostPortMatcher* matcher,
                       std::string_view domain,
                       bool include_subdomains) {
  auto domain_rule =
      net::SchemeHostPortMatcherRule::FromUntrimmedRawString(domain);

  if (domain_rule) {
    matcher->AddAsLastRule(std::move(domain_rule));
  } else {
    DVLOG(3) << "UrlMatcherWithBypass::UpdateMatcher() - " << domain
             << " is not a valid rule";
    return;
  }

  if (include_subdomains) {
    std::string subdomain = base::StrCat({".", domain});
    auto subdomain_rule =
        net::SchemeHostPortMatcherRule::FromUntrimmedRawString(subdomain);

    if (subdomain_rule) {
      matcher->AddAsLastRule(std::move(subdomain_rule));
    } else {
      DVLOG(3) << "UrlMatcherWithBypass::UpdateMatcher() - " << subdomain
               << " is not a valid rule";
      return;
    }
  }
}

}  // namespace

// static
std::string UrlMatcherWithBypass::PartitionMapKey(std::string_view domain) {
  auto last_dot = domain.rfind(".");
  if (last_dot != std::string::npos) {
    auto penultimate_dot = domain.rfind(".", last_dot - 1);
    if (penultimate_dot != std::string::npos) {
      return std::string(domain.substr(penultimate_dot + 1));
    }
  }
  return std::string(domain);
}

UrlMatcherWithBypass::UrlMatcherWithBypass() = default;
UrlMatcherWithBypass::~UrlMatcherWithBypass() = default;

void UrlMatcherWithBypass::AddDomainWithBypass(
    std::string_view domain,
    net::SchemeHostPortMatcher bypass_matcher,
    bool include_subdomains) {
  net::SchemeHostPortMatcher matcher;
  AddRulesToMatcher(&matcher, domain, include_subdomains);

  if (!matcher.rules().empty()) {
    match_list_with_bypass_map_[PartitionMapKey(domain)].emplace_back(
        std::make_pair(std::move(matcher), std::move(bypass_matcher)));
  }
}

void UrlMatcherWithBypass::AddMaskedDomainListRules(
    const std::vector<std::string>& domains,
    const std::string& partition_key,
    const masked_domain_list::ResourceOwner& resource_owner) {
  net::SchemeHostPortMatcher matcher;
  for (auto domain : domains) {
    CHECK(PartitionMapKey(domain) == partition_key);
    AddRulesToMatcher(&matcher, domain, !HasSubdomainCoverage(domain));
  }

  if (!matcher.rules().empty()) {
    match_list_with_bypass_map_[partition_key].emplace_back(
        std::make_pair(std::move(matcher), BuildBypassMatcher(resource_owner)));
  }
}

void UrlMatcherWithBypass::AddMaskedDomainListRules(
    std::string_view domain,
    const masked_domain_list::ResourceOwner& resource_owner) {
  // Only add rules for subdomains if the provided domain string doesn't support
  // them.
  AddDomainWithBypass(domain, BuildBypassMatcher(resource_owner),
                      !HasSubdomainCoverage(domain));
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

UrlMatcherWithBypass::MatchResult UrlMatcherWithBypass::Matches(
    const GURL& request_url,
    const absl::optional<net::SchemefulSite>& top_frame_site,
    bool skip_bypass_check) {
  auto dvlog = [&](std::string_view message,
                   const UrlMatcherWithBypass::MatchResult& match_result) {
    std::string result_message = base::StrCat(
        {" - matches: ", match_result.matches ? "true" : "false",
         ", third-party: ", match_result.is_third_party ? "true" : "false"});
    DVLOG(3) << "UrlMatcherWithBypass::Matches(" << request_url << ", "
             << top_frame_site.value() << ") - " << message << result_message;
  };
  // Result defaults to {matches = false, is_third_party = false}.
  MatchResult result;

  if (!skip_bypass_check && !top_frame_site.has_value()) {
    NOTREACHED_NORETURN()
        << "top frame site has no value and skip_bypass_check is false";
  }

  if (!IsPopulated()) {
    dvlog("skipped (match list not populated)", result);
    return result;
  }

  net::SchemefulSite request_site(request_url);
  result.is_third_party = skip_bypass_check || (request_site != top_frame_site);

  std::string resource_host_suffix = PartitionMapKey(request_url.host());

  if (!match_list_with_bypass_map_.contains(resource_host_suffix)) {
    dvlog("no suffix match", result);
    return result;
  }

  for (const auto& [matcher, bypass_matcher] :
       match_list_with_bypass_map_.at(resource_host_suffix)) {
    auto rule_result = matcher.Evaluate(request_url);
    if (rule_result == net::SchemeHostPortMatcherResult::kInclude) {
      result.matches = true;
      result.is_third_party =
          skip_bypass_check ||
          bypass_matcher.Evaluate(top_frame_site.value().GetURL()) ==
              net::SchemeHostPortMatcherResult::kNoMatch;
      break;
    }
  }

  dvlog("success", result);
  return result;
}

}  // namespace network
