// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_inclusion_rules.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/device_bound_sessions/host_patterns.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"

namespace net::device_bound_sessions {

namespace {

bool IsIncludeSiteAllowed(const url::Origin& origin) {
  // This is eTLD+1
  const std::string domain_and_registry =
      registry_controlled_domains::GetDomainAndRegistry(
          origin, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return !domain_and_registry.empty() && origin.host() == domain_and_registry;
}

proto::RuleType GetRuleTypeProto(
    SessionInclusionRules::InclusionResult result) {
  return result == SessionInclusionRules::InclusionResult::kInclude
             ? proto::RuleType::INCLUDE
             : proto::RuleType::EXCLUDE;
}

std::optional<SessionParams::Scope::Specification::Type> GetInclusionResult(
    proto::RuleType proto) {
  if (proto == proto::RuleType::INCLUDE) {
    return SessionParams::Scope::Specification::Type::kInclude;
  } else if (proto == proto::RuleType::EXCLUDE) {
    return SessionParams::Scope::Specification::Type::kExclude;
  }

  // proto = RULE_TYPE_UNSPECIFIED
  return std::nullopt;
}

std::string RuleTypeToString(SessionInclusionRules::InclusionResult rule_type) {
  switch (rule_type) {
    case SessionInclusionRules::InclusionResult::kExclude:
      return "exclude";
    case SessionInclusionRules::InclusionResult::kInclude:
      return "include";
  }
}

}  // namespace

// Encapsulates a single rule which applies to the request URL.
struct SessionInclusionRules::UrlRule {
  // URLs that match the rule will be subject to inclusion or exclusion as
  // specified by the type.
  InclusionResult rule_type;

  // Domain or pattern that the URL must match. This must either be a
  // full domain (host piece) or a pattern containing a wildcard in the
  // most-specific (leftmost) label position followed by a dot.
  // The matched strings follow SchemeHostPortMatcherRule's logic, but with
  // some extra requirements for validity:
  // - If the pattern has a leading wildcard *, it must be "*" itself or
  //   the * must be followed by a dot, so "*ple.com" is not acceptable.
  // - Multiple wildcards are not allowed.
  // - Internal wildcards are not allowed, so "sub.*.example.com" does not
  //   work because the wildcard is not the leftmost component.
  // - IP addresses also work. IPv4 addresses can contain wildcards.
  std::string host_pattern;

  // Prefix consisting of path components that the URL must match. Must begin
  // with '/'. Wildcards are not allowed. Simply use "/" to match all paths.
  std::string path_prefix;

  friend bool operator==(const UrlRule& lhs, const UrlRule& rhs) = default;

  // Returns whether the given `url` matches this rule. Note that this
  // function does not check the scheme and port portions of the URL/origin.
  bool MatchesHostAndPath(const GURL& url) const;
};

// static
base::expected<SessionInclusionRules, SessionError>
SessionInclusionRules::Create(const url::Origin& origin,
                              const SessionParams::Scope& scope_params,
                              const GURL& refresh_endpoint) {
  SessionInclusionRules rules(origin);

  if (scope_params.include_site && !rules.may_include_site_) {
    return base::unexpected(
        SessionError{SessionError::kInvalidScopeIncludeSite});
  }

  rules.SetIncludeSite(scope_params.include_site);

  for (const auto& spec : scope_params.specifications) {
    const auto inclusion_result =
        spec.type == SessionParams::Scope::Specification::Type::kExclude
            ? SessionInclusionRules::InclusionResult::kExclude
            : SessionInclusionRules::InclusionResult::kInclude;
    SessionError::ErrorType add_url_rule_result =
        rules.AddUrlRuleIfValid(inclusion_result, spec.domain, spec.path);
    if (add_url_rule_result != SessionError::kSuccess) {
      return base::unexpected(SessionError{add_url_rule_result});
    }
  }

  if (refresh_endpoint.is_valid()) {
    // Sessions should never include the refresh endpoint, since that would
    // prevent them from ever refreshing when a cookie expires. We intentionally
    // don't return an error if the rule is not valid or add a CHECK, because a
    // refresh URL is allowed to be outside an origin-scoped session.
    rules.AddUrlRuleIfValid(SessionInclusionRules::InclusionResult::kExclude,
                            refresh_endpoint.GetHost(),
                            refresh_endpoint.GetPath());
  }

  return rules;
}

SessionInclusionRules::SessionInclusionRules(const url::Origin& origin)
    : origin_(origin), may_include_site_(IsIncludeSiteAllowed(origin)) {}

SessionInclusionRules::~SessionInclusionRules() = default;

SessionInclusionRules::SessionInclusionRules(SessionInclusionRules&& other) =
    default;

SessionInclusionRules& SessionInclusionRules::operator=(
    SessionInclusionRules&& other) = default;

bool SessionInclusionRules::operator==(
    const SessionInclusionRules& other) const = default;

void SessionInclusionRules::SetIncludeSite(bool include_site) {
  if (!include_site) {
    include_site_.reset();
    return;
  }

  include_site_ = SchemefulSite(origin_);
}

SessionError::ErrorType SessionInclusionRules::AddUrlRuleIfValid(
    InclusionResult rule_type,
    const std::string& host_pattern,
    const std::string& path_prefix) {
  if (path_prefix.empty() || path_prefix.front() != '/') {
    return SessionError::kInvalidScopeRulePath;
  }

  if (!IsValidHostPattern(host_pattern)) {
    return SessionError::kInvalidScopeRuleHostPattern;
  }

  // Return early if the rule can't match anything. For origin-scoped
  // sessions, the origin must match the host pattern.
  if (!include_site_ && !MatchesHostPattern(host_pattern, origin_.host())) {
    return SessionError::kScopeRuleOriginScopedHostPatternMismatch;
  }

  // For site-scoped sessions, either the site itself matches the
  // pattern (e.g. a pattern of "*") or the hostlike part of the pattern
  // is same-site.
  if (include_site_ && !MatchesHostPattern(host_pattern, origin_.host())) {
    std::string_view hostlike_part = host_pattern;
    if (hostlike_part.starts_with("*.")) {
      hostlike_part = hostlike_part.substr(2);
    }

    std::string hostlike_part_domain =
        registry_controlled_domains::GetDomainAndRegistry(
            hostlike_part,
            registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    std::string domain_and_registry =
        registry_controlled_domains::GetDomainAndRegistry(
            origin_, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (hostlike_part_domain != domain_and_registry) {
      return SessionError::kScopeRuleSiteScopedHostPatternMismatch;
    }
  }

  url_rules_.emplace_back(rule_type, host_pattern, path_prefix);
  return SessionError::kSuccess;
}

SessionInclusionRules::InclusionResult
SessionInclusionRules::EvaluateRequestUrl(const GURL& url) const {
  bool same_origin = origin_.IsSameOriginWith(url);
  if (include_site_ && !include_site_->IsSameSiteWith(url)) {
    return SessionInclusionRules::kExclude;
  }

  if (!include_site_ && !same_origin) {
    return SessionInclusionRules::kExclude;
  }

  // Evaluate against specific rules, most-recently-added first.
  for (const UrlRule& rule : base::Reversed(url_rules_)) {
    // The rule covers host and path, and scheme is checked too. We don't check
    // port here, because in the !may_include_site_ case that's already covered
    // by being same-origin, and in the may_include_site_ case it's ok for the
    // port to differ.
    if (rule.MatchesHostAndPath(url) && url.scheme() == origin_.scheme()) {
      return rule.rule_type;
    }
  }

  return SessionInclusionRules::kInclude;
}

bool SessionInclusionRules::AllowsRefreshForInitiator(
    const url::Origin& initiator) const {
  if (include_site_ && include_site_->IsSameSiteWith(initiator)) {
    return true;
  }

  if (!include_site_ && origin_.IsSameOriginWith(initiator)) {
    return true;
  }

  return false;
}

bool SessionInclusionRules::UrlRule::MatchesHostAndPath(const GURL& url) const {
  if (!MatchesHostPattern(host_pattern, url.GetHost())) {
    return false;
  }

  std::string_view url_path = url.path();
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
    rule_proto.set_host_pattern(rule.host_pattern);
    rule_proto.set_path_prefix(rule.path_prefix);
    proto.mutable_url_rules()->Add(std::move(rule_proto));
  }

  return proto;
}

// static:
std::optional<SessionInclusionRules> SessionInclusionRules::CreateFromProto(
    const proto::SessionInclusionRules& proto) {
  if (!proto.has_origin() || !proto.has_do_include_site()) {
    return std::nullopt;
  }
  url::Origin origin = url::Origin::Create(GURL(proto.origin()));
  if (origin.opaque()) {
    DLOG(ERROR) << "proto origin parse error: " << origin.GetDebugString();
    return std::nullopt;
  }

  SessionParams::Scope params;
  params.include_site = proto.do_include_site();
  for (const auto& rule_proto : proto.url_rules()) {
    std::optional<SessionParams::Scope::Specification::Type> rule_type =
        GetInclusionResult(rule_proto.rule_type());
    if (!rule_type.has_value()) {
      return std::nullopt;
    }

    params.specifications.emplace_back(*rule_type, rule_proto.host_pattern(),
                                       rule_proto.path_prefix());
  }

  // We use an empty refresh URL because the implicit refresh rule is already
  // among those in `url_rules()`.
  auto inclusion_rules_or_error =
      Create(origin, std::move(params), /*refresh_endpoint=*/GURL());
  if (!inclusion_rules_or_error.has_value()) {
    return std::nullopt;
  }

  return std::move(*inclusion_rules_or_error);
}

std::string SessionInclusionRules::DebugString() const {
  std::string result;
  for (const UrlRule& rule : url_rules_) {
    base::StrAppend(&result, {"Type=", RuleTypeToString(rule.rule_type),
                              "; Domain=", rule.host_pattern,
                              "; Path=", rule.path_prefix, "\n"});
  }
  return result;
}

}  // namespace net::device_bound_sessions
