// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_inclusion_rules.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/ip_address.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/scheme_host_port_matcher_result.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/url_util.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session.h"

namespace net::device_bound_sessions {

namespace {

bool IsIncludeSiteAllowed(const url::Origin& origin) {
  // This is eTLD+1
  const std::string domain_and_registry =
      registry_controlled_domains::GetDomainAndRegistry(
          origin, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return !domain_and_registry.empty() && origin.host() == domain_and_registry;
}

SessionInclusionRules::InclusionResult AsInclusionResult(bool should_include) {
  return should_include ? SessionInclusionRules::kInclude
                        : SessionInclusionRules::kExclude;
}

// Types of characters valid in IPv6 addresses.
// Derived from logic in url::DoIPv6AddressToNumber() and url::DoParseIPv6().
bool IsValidIPv6Char(char c) {
  return c == ':' || base::IsHexDigit(c) || c == '.' ||
         // 'x' or 'X' is used in IPv4 to denote hex values, and can be used in
         // parts of IPv6 addresses.
         c == 'x' || c == 'X';
}

proto::RuleType GetRuleTypeProto(
    SessionInclusionRules::InclusionResult result) {
  return result == SessionInclusionRules::InclusionResult::kInclude
             ? proto::RuleType::INCLUDE
             : proto::RuleType::EXCLUDE;
}

std::optional<SessionInclusionRules::InclusionResult> GetInclusionResult(
    proto::RuleType proto) {
  if (proto == proto::RuleType::INCLUDE) {
    return SessionInclusionRules::InclusionResult::kInclude;
  } else if (proto == proto::RuleType::EXCLUDE) {
    return SessionInclusionRules::InclusionResult::kExclude;
  }

  // proto = RULE_TYPE_UNSPECIFIED
  return std::nullopt;
}

}  // namespace

// Encapsulates a single rule which applies to the request URL.
struct SessionInclusionRules::UrlRule {
  // URLs that match the rule will be subject to inclusion or exclusion as
  // specified by the type.
  InclusionResult rule_type;

  // Domain or pattern that the URL must match. This must either be a
  // full domain (host piece) or a pattern containing a wildcard in the
  // most-specific (leftmost) label position followed by a dot and a non-eTLD.
  // The matched strings follow SchemeHostPortMatcherRule's logic, but with
  // some extra requirements for validity:
  // - A leading wildcard * must be followed by a dot, so "*ple.com" is not
  //   acceptable.
  // - "*.com" is not accepted because com is an eTLD. Same with "*.co.uk" and
  //   similar.
  // - Multiple wildcards are not allowed.
  // - Internal wildcards are not allowed, so "sub.*.example.com" does not
  //   work because the wildcard is not the leftmost component.
  // - IP addresses also work if specified as the exact host, as described in
  //   SchemeHostPortMatcherRule.
  std::unique_ptr<SchemeHostPortMatcherRule> host_matcher_rule;

  // Prefix consisting of path components that the URL must match. Must begin
  // with '/'. Wildcards are not allowed. Simply use "/" to match all paths.
  std::string path_prefix;

  friend bool operator==(const UrlRule& lhs, const UrlRule& rhs) {
    return lhs.rule_type == rhs.rule_type &&
           lhs.path_prefix == rhs.path_prefix &&
           lhs.host_matcher_rule->ToString() ==
               rhs.host_matcher_rule->ToString();
  }

  // Returns whether the given `url` matches this rule. Note that this
  // function does not check the scheme and port portions of the URL/origin.
  bool MatchesHostAndPath(const GURL& url) const;
};

SessionInclusionRules::SessionInclusionRules(const url::Origin& origin)
    : origin_(origin), may_include_site_(IsIncludeSiteAllowed(origin)) {}

SessionInclusionRules::SessionInclusionRules() = default;

SessionInclusionRules::~SessionInclusionRules() = default;

SessionInclusionRules::SessionInclusionRules(SessionInclusionRules&& other) =
    default;

SessionInclusionRules& SessionInclusionRules::operator=(
    SessionInclusionRules&& other) = default;

bool SessionInclusionRules::operator==(
    const SessionInclusionRules& other) const = default;

void SessionInclusionRules::SetIncludeSite(bool include_site) {
  if (!may_include_site_) {
    return;
  }

  if (!include_site) {
    include_site_.reset();
    return;
  }

  include_site_ = SchemefulSite(origin_);
}

bool SessionInclusionRules::AddUrlRuleIfValid(InclusionResult rule_type,
                                              const std::string& host_pattern,
                                              const std::string& path_prefix) {
  if (path_prefix.empty() || path_prefix.front() != '/') {
    return false;
  }
  if (host_pattern.empty()) {
    return false;
  }

  // If only the origin is allowed, the host_pattern must be precisely its host.
  bool host_pattern_is_host = host_pattern == origin_.host();
  if (!may_include_site_ && !host_pattern_is_host) {
    return false;
  }

  // Don't allow '*' anywhere besides the first character of the pattern.
  size_t star_pos = host_pattern.rfind('*');
  if (star_pos != std::string::npos && star_pos != 0) {
    return false;
  }
  // Only allow wildcard if immediately followed by a dot.
  bool has_initial_wildcard_label = host_pattern.starts_with("*.");
  if (star_pos != std::string::npos && !has_initial_wildcard_label) {
    return false;
  }

  std::string_view hostlike_part{host_pattern};
  if (has_initial_wildcard_label) {
    hostlike_part = hostlike_part.substr(2);
  }

  bool presumed_ipv6 = host_pattern.front() == '[';
  if (presumed_ipv6 && host_pattern.back() != ']') {
    return false;
  }

  // Allow only specific characters into SchemeHostPortMatcherRule parsing.
  if (presumed_ipv6) {
    // Leave out the brackets, but everything else must be a valid char.
    std::string_view ipv6_address{host_pattern.begin() + 1,
                                  host_pattern.end() - 1};
    if (std::find_if_not(ipv6_address.begin(), ipv6_address.end(),
                         &IsValidIPv6Char) != ipv6_address.end()) {
      return false;
    }
  } else {
    // Note that this excludes a ':' character specifying a port number, even
    // though SchemeHostPortMatcherRule supports it. Same for '/' (for the
    // scheme or an IP block).
    // TODO(chlily): Consider supporting port numbers.
    if (!IsCanonicalizedHostCompliant(hostlike_part)) {
      return false;
    }
  }

  // Delegate the rest of the parsing to SchemeHostPortMatcherRule.
  std::unique_ptr<SchemeHostPortMatcherRule> host_matcher_rule =
      SchemeHostPortMatcherRule::FromUntrimmedRawString(host_pattern);
  if (!host_matcher_rule) {
    return false;
  }

  // Now that we know the host_pattern is at least the right shape, validate the
  // remaining restrictions.

  // Skip the eTLD lookups if the host pattern is an exact match.
  if (host_pattern_is_host) {
    url_rules_.emplace_back(rule_type, std::move(host_matcher_rule),
                            path_prefix);
    return true;
  }

  std::string hostlike_part_domain =
      registry_controlled_domains::GetDomainAndRegistry(
          hostlike_part,
          registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // If there is a wildcard, we require the pattern to be a normal domain and
  // not an eTLD.
  if (has_initial_wildcard_label && hostlike_part_domain.empty()) {
    return false;
  }

  // Validate that the host pattern is on the right origin/site.
  // TODO(chlily): Perhaps we should use a cached value, but surely URL rule
  // parsing only happens a small number of times.
  std::string domain_and_registry =
      registry_controlled_domains::GetDomainAndRegistry(
          origin_, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // The origin_ must have an eTLD+1, because if it didn't, then we'd know that
  // !may_include_site_, and that would mean we'd have already returned early
  // and would never get here.
  CHECK(!domain_and_registry.empty());
  if (hostlike_part_domain != domain_and_registry) {
    return false;
  }

  url_rules_.emplace_back(rule_type, std::move(host_matcher_rule), path_prefix);
  return true;
}

SessionInclusionRules::InclusionResult
SessionInclusionRules::EvaluateRequestUrl(const GURL& url) const {
  bool same_origin = origin_.IsSameOriginWith(url);
  if (!may_include_site_ && !same_origin) {
    return SessionInclusionRules::kExclude;
  }

  // Evaluate against specific rules, most-recently-added first.
  for (const UrlRule& rule : base::Reversed(url_rules_)) {
    // The rule covers host and path, and scheme is checked too. We don't check
    // port here, because in the !may_include_site_ case that's already covered
    // by being same-origin, and in the may_include_site_ case it's ok for the
    // port to differ.
    if (rule.MatchesHostAndPath(url) &&
        url.scheme_piece() == origin_.scheme()) {
      return rule.rule_type;
    }
  }

  // None of the specific rules apply. Evaluate against the basic include rule.
  if (include_site_) {
    return AsInclusionResult(SchemefulSite(url) == *include_site_);
  }
  return AsInclusionResult(same_origin);
}

bool SessionInclusionRules::UrlRule::MatchesHostAndPath(const GURL& url) const {
  if (host_matcher_rule->Evaluate(url) ==
      SchemeHostPortMatcherResult::kNoMatch) {
    return false;
  }

  std::string_view url_path = url.path_piece();
  if (!url_path.starts_with(path_prefix)) {
    return false;
  }
  // We must check the following to prevent a path prefix like "/foo" from
  // erroneously matching a URL path like "/foobar/baz". There are 2 possible
  // cases: `url_path` may be the same length as `path_prefix`, or `url_path`
  // may be longer than `path_prefix`. In the first case, the two paths are
  // equal and a match has been found. In the second case, we want to know
  // whether the end of the `path_prefix` represents a full label in the path.
  // Either the path_prefix string ends in '/' and is explicitly the end of a
  // label, or the next character of `url_path` beyond the identical portion is
  // '/'. Otherwise, reject the path as a false (incomplete label) prefix match.
  CHECK(url_path.length() >= path_prefix.length());
  if (url_path.length() > path_prefix.length() && path_prefix.back() != '/' &&
      url_path[path_prefix.length()] != '/') {
    return false;
  }

  return true;
}

size_t SessionInclusionRules::num_url_rules_for_testing() const {
  return url_rules_.size();
}

proto::SessionInclusionRules SessionInclusionRules::ToProto() const {
  proto::SessionInclusionRules proto;
  proto.set_origin(origin_.Serialize());
  proto.set_do_include_site(include_site_.has_value());

  // Note that the ordering of the rules (in terms of when they were added to
  // the session) is preserved in the proto. Preserving the ordering is
  // important to handle rules overlap - the latest rule wins.
  for (auto& rule : url_rules_) {
    proto::UrlRule rule_proto;
    rule_proto.set_rule_type(GetRuleTypeProto(rule.rule_type));
    rule_proto.set_host_matcher_rule(rule.host_matcher_rule->ToString());
    rule_proto.set_path_prefix(rule.path_prefix);
    proto.mutable_url_rules()->Add(std::move(rule_proto));
  }

  return proto;
}

// static:
std::unique_ptr<SessionInclusionRules> SessionInclusionRules::CreateFromProto(
    const proto::SessionInclusionRules& proto) {
  if (!proto.has_origin() || !proto.has_do_include_site()) {
    return nullptr;
  }
  url::Origin origin = url::Origin::Create(GURL(proto.origin()));
  if (origin.opaque()) {
    DLOG(ERROR) << "proto origin parse error: " << origin.GetDebugString();
    return nullptr;
  }

  auto result = std::make_unique<SessionInclusionRules>(origin);
  result->SetIncludeSite(proto.do_include_site());
  for (const auto& rule_proto : proto.url_rules()) {
    std::optional<InclusionResult> rule_type =
        GetInclusionResult(rule_proto.rule_type());
    if (!rule_type.has_value() ||
        !result->AddUrlRuleIfValid(*rule_type, rule_proto.host_matcher_rule(),
                                   rule_proto.path_prefix())) {
      DLOG(ERROR) << "proto rule parse error: " << "type:"
                  << proto::RuleType_Name(rule_proto.rule_type()) << " "
                  << "matcher:" << rule_proto.host_matcher_rule() << " "
                  << "prefix:" << rule_proto.path_prefix();
      return nullptr;
    }
  }

  return result;
}

}  // namespace net::device_bound_sessions
