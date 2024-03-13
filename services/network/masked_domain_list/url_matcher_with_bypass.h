// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MASKED_DOMAIN_LIST_URL_MATCHER_WITH_BYPASS_H_
#define SERVICES_NETWORK_MASKED_DOMAIN_LIST_URL_MATCHER_WITH_BYPASS_H_

#include <map>
#include <string_view>

#include "base/types/optional_ref.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/scheme_host_port_matcher.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace network {

// This is a helper class for creating URL match lists for subresource request
// that can be bypassed with additional sets of rules based on the top frame
// URL.
class COMPONENT_EXPORT(NETWORK_SERVICE) UrlMatcherWithBypass {
 public:
  UrlMatcherWithBypass();
  ~UrlMatcherWithBypass();

  // Returns true if there are entries in the match list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated();

  // TODO(aakallam): Refactor this to not rely on concept of "third party"
  struct MatchResult {
    // Whether a resource URL matches the list.
    bool matches = false;
    // Whether a resource is requested in a first or third-party context.
    bool is_third_party = false;

    bool operator==(const MatchResult& rhs) const {
      return matches == rhs.matches && is_third_party == rhs.is_third_party;
    }
  };

  // Determines if the pair of URLs are a match by first trying to match on the
  // resource_url and then checking if the top_frame_site matches the bypass
  // match rules. If skip_bypass_check is true, the top_frame_site will not be
  // used to determine the outcome of the match.
  // top_frame_site should have a value if skip_bypass_check is false.
  MatchResult Matches(const GURL& resource_url,
                      const std::optional<net::SchemefulSite>& top_frame_site,
                      bool skip_bypass_check = false);

  // Builds a pair of matcher and bypass rules for the each partition needed for
  // the set of domains. If a ResourceOwner is not provided then no bypass rules
  // will be created.
  void AddMaskedDomainListRules(
      const std::set<std::string>& domains,
      base::optional_ref<masked_domain_list::ResourceOwner> resource_owner);

  // Builds a matcher for each partition needed that does not have any bypass
  // rules.
  void AddRulesWithoutBypass(const std::set<std::string>& domains);

  void Clear();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  static net::SchemeHostPortMatcher BuildBypassMatcher(
      const masked_domain_list::ResourceOwner& resource_owner);

  // Determine the partition of the `match_list_with_bypass_map_` that contains
  // the given domain.
  static std::string PartitionMapKey(std::string_view domain);

 private:
  // Maps partition map keys to smaller maps of domains eligible for the match
  // list and the top frame domains that allow the match list to be bypassed.
  std::map<std::string,
           std::vector<std::pair<net::SchemeHostPortMatcher,
                                 net::SchemeHostPortMatcher>>>
      match_list_with_bypass_map_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MASKED_DOMAIN_LIST_URL_MATCHER_WITH_BYPASS_H_
