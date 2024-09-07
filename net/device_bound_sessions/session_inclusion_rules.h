// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_H_

#include <memory>
#include <optional>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace proto {
class SessionInclusionRules;
}

// This class represents a set of rules that define which network requests may
// potentially be deferred on account of an active DBSC session. It is derived
// from parameters specified in the session config. Note that this scope is a
// distinct concept from the "scope" of a cookie (or CookieCraving), which is
// the set of requests for which that cookie should be included.
//
// The SessionInclusionRules consists of a basic include rule and a number of
// specific include/exclude rules.
// 1. The basic include rule defaults to including the origin that created/set
//    this session's config, but can be expanded to include the whole site
//    (eTLD+1) if allowed.
// 2. A session is allowed to include requests beyond its setting origin if the
//    setting origin's host is the root eTLD+1 (not a subdomain).
// 3. Specific include and exclude rules specify URL patterns that are included
//    or excluded from deferral by the session.
//
// A request URL is evaluated for inclusion by matching with the specific rules
// in reverse order of addition, and then following the basic include rule if no
// specific rules match. Once established, a SessionInclusionRules only cares
// about the request URL, not any other properties of the request.
class NET_EXPORT SessionInclusionRules final {
 public:
  enum InclusionResult {
    // Definitely do not defer a request on behalf of this DBSC session.
    kExclude,
    // Consider a request eligible for deferral on behalf of this session, if
    // other conditions are met.
    kInclude,
  };

  // Initializes a default rule for the given origin. Does not do any checks
  // on the origin; caller should enforce semantic checks on the origin such as
  // desired schemes.
  explicit SessionInclusionRules(const url::Origin& origin);

  // Default, matches nothing.
  SessionInclusionRules();

  SessionInclusionRules(const SessionInclusionRules& other) = delete;
  SessionInclusionRules& operator=(const SessionInclusionRules& other) = delete;

  SessionInclusionRules(SessionInclusionRules&& other);
  SessionInclusionRules& operator=(SessionInclusionRules&& other);

  ~SessionInclusionRules();

  bool operator==(const SessionInclusionRules& other) const;

  // Sets the basic include rule underlying the more specific URL rules. This
  // should be derived from the "include_site" param in the config. If not set
  // explicitly, the default is false (meaning an origin-scoped session). If
  // called with true: expands the basic include rule to include the whole site
  // of the setting origin, if allowed. If called with false: restricts the
  // basic rule to the setting origin only (any specific URL rules that are
  // present will still apply).
  void SetIncludeSite(bool include_site);

  // Adds a specific URL rule that includes/excludes certain URLs based on their
  // host part matching `host_pattern` and the path matching `path_prefix`. Any
  // matching rule takes precedence over the basic scope. Does some validity
  // checks on the inputs first. The `host_pattern` must either be a full domain
  // (host piece) or a pattern containing a wildcard ('*' character) in the
  // most-specific (leftmost) label position followed by a dot and a non-eTLD.
  // The `path_prefix` must begin with '/' and cannot contain wildcards, and
  // will match paths that start with the same path components. Returns whether
  // the specified rule was added.
  bool AddUrlRuleIfValid(InclusionResult rule_type,
                         const std::string& host_pattern,
                         const std::string& path_prefix);

  // Evaluates `url` to determine whether a request to `url` may be included
  // (i.e. potentially deferred on account of this DBSC session, if other
  // conditions are met).
  InclusionResult EvaluateRequestUrl(const GURL& url) const;

  bool may_include_site_for_testing() const { return may_include_site_; }
  const url::Origin& origin() const { return origin_; }

  size_t num_url_rules_for_testing() const;

  proto::SessionInclusionRules ToProto() const;
  static std::unique_ptr<SessionInclusionRules> CreateFromProto(
      const proto::SessionInclusionRules& proto);

 private:
  struct UrlRule;

  // The origin that created/set the session that this applies to. By default,
  // sessions are origin-scoped unless specified otherwise.
  url::Origin origin_;

  // Whether the setting origin is allowed to include the whole site in its
  // rules. This is equivalent to whether the origin's domain is the root eTLD+1
  // (not a subdomain). It is cached here to avoid repeated eTLD lookups.
  bool may_include_site_ = false;

  // If non-nullopt: The site of `origin_`, when the config has specified
  // "include_site" to make the session include any request URL on the setting
  // origin's whole eTLD+1. This is only allowed if the origin's host is the
  // root eTLD+1 (not a subdomain). We cache it here for efficiency rather than
  // repeatedly constructing it from the `origin_` from which it's derived.
  // If nullopt: Either the config has not specified "include_site", or the
  // `origin_` is not allowed to include anything outside its origin.
  // Invariant: If `may_include_site_` is false, then this must also be nullopt.
  // This shouldn't ever be an opaque site.
  std::optional<SchemefulSite> include_site_;

  // A list of rules that modify the basic include rule (specified by `origin_`
  // or `include_site_`), which may specify inclusion or exclusion for URLs that
  // match. If any rules overlap, the latest rule takes precedence over earlier
  // rules.
  std::vector<UrlRule> url_rules_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_INCLUSION_RULES_H_
