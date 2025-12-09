// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_inclusion_rules.h"

#include <initializer_list>

#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "net/device_bound_sessions/session_error.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::test::ErrorIs;

namespace net::device_bound_sessions {

namespace {

using Result = SessionInclusionRules::InclusionResult;
using RuleType = SessionParams::Scope::Specification::Type;

// These tests depend on the registry_controlled_domains code, so assert ahead
// of time that the eTLD+1 is what we expect, for clarity and to avoid confusing
// test failures.
void AssertDomainAndRegistry(const url::Origin& origin,
                             const std::string& expected_domain_and_registry) {
  ASSERT_EQ(
      registry_controlled_domains::GetDomainAndRegistry(
          origin, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES),
      expected_domain_and_registry)
      << "Unexpected domain and registry.";
}

}  // namespace

class SessionInclusionRulesTest : public ::testing::Test {
 public:
  struct EvaluateUrlTestCase {
    const char* url;
    Result expected_result;
  };

  struct AddUrlRuleTestCase {
    RuleType rule_type;
    const char* host_pattern;
    const char* path_prefix;
    // This is kSuccess if there is no error expected when adding.
    SessionError::ErrorType expected_is_added_result;
  };

  SessionInclusionRulesTest() = default;

  void SetOrigin(const url::Origin& origin) { origin_ = origin; }

  void SetIncludeSite(bool include_site) {
    params_.include_site = include_site;
  }

  void CheckMayIncludeSite(bool expected_may_include_site) {
    ASSERT_OK_AND_ASSIGN(
        auto rules, SessionInclusionRules::Create(origin_, params_, GURL()));
    EXPECT_EQ(rules.may_include_site_for_testing(), expected_may_include_site);
  }

  void CheckEvaluateUrlTestCases(
      std::initializer_list<EvaluateUrlTestCase> test_cases) {
    ASSERT_OK_AND_ASSIGN(
        auto rules, SessionInclusionRules::Create(origin_, params_, GURL()));

    for (const auto& test_case : test_cases) {
      SCOPED_TRACE(test_case.url);
      EXPECT_EQ(rules.EvaluateRequestUrl(GURL(test_case.url)),
                test_case.expected_result);
    }
  }

  void CheckAddUrlRuleTestCases(
      std::initializer_list<AddUrlRuleTestCase> test_cases) {
    EXPECT_OK(SessionInclusionRules::Create(origin_, params_, GURL()));

    for (const auto& test_case : test_cases) {
      SCOPED_TRACE(base::JoinString(
          {test_case.host_pattern, test_case.path_prefix}, ", "));
      params_.specifications.emplace_back(
          test_case.rule_type, test_case.host_pattern, test_case.path_prefix);
      auto inclusion_rules_or_error =
          SessionInclusionRules::Create(origin_, params_, GURL());
      if (test_case.expected_is_added_result == SessionError::kSuccess) {
        EXPECT_OK(inclusion_rules_or_error);
      } else {
        EXPECT_THAT(inclusion_rules_or_error,
                    ErrorIs(SessionError(test_case.expected_is_added_result)));
      }

      if (!inclusion_rules_or_error.has_value()) {
        // Forget about this rule so that future rules can be evaluated.
        params_.specifications.pop_back();
      }
    }
  }

  const SessionParams::Scope& params() { return params_; }

 private:
  SessionParams::Scope params_;
  url::Origin origin_;
};

TEST_F(SessionInclusionRulesTest, DefaultIncludeOriginMayNotIncludeSite) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  AssertDomainAndRegistry(subdomain_origin, "site.test");

  SetOrigin(subdomain_origin);

  CheckMayIncludeSite(false);

  CheckEvaluateUrlTestCases(
      {// URL not valid.
       {"", Result::kExclude},
       // Origins match.
       {"https://some.site.test", Result::kInclude},
       // Path is allowed.
       {"https://some.site.test/path", Result::kInclude},
       // Not same scheme.
       {"http://some.site.test", Result::kExclude},
       // Not same host (same-site subdomain).
       {"https://some.other.site.test", Result::kExclude},
       // Not same host (superdomain).
       {"https://site.test", Result::kExclude},
       // Unrelated site.
       {"https://unrelated.test", Result::kExclude},
       // Not same port.
       {"https://some.site.test:8888", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, DefaultIncludeOriginThoughMayIncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);

  // All expectations are as above. Even though including the site is allowed,
  // because the origin's host is its root eTLD+1, it is still limited to a
  // default origin inclusion_rules because it did not set include_site.
  CheckEvaluateUrlTestCases({// URL not valid.
                             {"", Result::kExclude},
                             // Origins match.
                             {"https://site.test", Result::kInclude},
                             // Path is allowed.
                             {"https://site.test/path", Result::kInclude},
                             // Not same scheme.
                             {"http://site.test", Result::kExclude},
                             // Not same host (same-site subdomain).
                             {"https://other.site.test", Result::kExclude},
                             // Not same host (superdomain).
                             {"https://test", Result::kExclude},
                             // Unrelated site.
                             {"https://unrelated.test", Result::kExclude},
                             // Not same port.
                             {"https://site.test:8888", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, IncludeSiteAttemptedButNotAllowed) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  AssertDomainAndRegistry(subdomain_origin, "site.test");

  SessionParams::Scope params;
  params.include_site = true;
  EXPECT_THAT(
      SessionInclusionRules::Create(subdomain_origin, params,
                                    GURL("https://some.site.test/refresh")),
      ErrorIs(SessionError(SessionError::kInvalidScopeIncludeSite)));
}

TEST_F(SessionInclusionRulesTest, IncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);

  SetIncludeSite(true);

  CheckEvaluateUrlTestCases(
      {// URL not valid.
       {"", Result::kExclude},
       // Origins match.
       {"https://site.test", Result::kInclude},
       // Path is allowed.
       {"https://site.test/path", Result::kInclude},
       // Not same scheme (site is schemeful).
       {"http://site.test", Result::kExclude},
       // Same-site subdomain is allowed.
       {"https://some.site.test", Result::kInclude},
       {"https://some.other.site.test", Result::kInclude},
       // Not same host (superdomain).
       {"https://test", Result::kExclude},
       // Unrelated site.
       {"https://unrelated.test", Result::kExclude},
       // Other port is allowed because whole site is included.
       {"https://site.test:8888", Result::kInclude}});
}

TEST_F(SessionInclusionRulesTest, AddUrlRuleToOriginOnly) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  AssertDomainAndRegistry(subdomain_origin, "site.test");

  SetOrigin(subdomain_origin);
  CheckMayIncludeSite(false);

  CheckAddUrlRuleTestCases(
      {// Host pattern equals origin's host. Path is valid.
       {RuleType::kExclude, "some.site.test", "/static",
        SessionError::kSuccess},
       // Add an opposite rule to check later.
       {RuleType::kInclude, "some.site.test", "/static/included",
        SessionError::kSuccess},
       // Path not valid.
       {RuleType::kExclude, "some.site.test", "NotAPath",
        SessionError::kInvalidScopeRulePath},
       // Has a valid wildcard, but the origin scoping ensures it will
       // only match some.site.test.
       {RuleType::kInclude, "*.site.test", "/static/wildcard_match/",
        SessionError::kSuccess},
       // Other host patterns are not accepted.
       {RuleType::kInclude, "unrelated.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "other.site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "https://some.site.test", "/static/https_rule/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "some.site.test:8000", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});

  EXPECT_EQ(params().specifications.size(), 3u);

  CheckEvaluateUrlTestCases(
      {// Matches the rule.
       {"https://some.site.test/static", Result::kExclude},
       // A path under the rule's path prefix is subject to the rule.
       {"https://some.site.test/static/some/thing", Result::kExclude},
       // These do not match the rule, so are subject to the basic rules (the
       // origin).
       {"https://some.site.test/staticcccccccc", Result::kInclude},
       {"https://other.site.test/static", Result::kExclude},
       // The more recently added rule wins out.
       {"https://some.site.test/static/included", Result::kInclude},
       {"https://some.site.test/valid_path", Result::kInclude},
       // The wildcard matching is valid, but only matches the origin.
       {"https://some.site.test/static/wildcard_match/", Result::kInclude},
       // The origin scoping takes precedence over the rule.
       {"https://subdomain.site.test/static/wildcard_match/", Result::kExclude},
       {"https://unrelated.test/", Result::kExclude},
       {"https://site.test/", Result::kExclude},
       {"https://other.test/", Result::kExclude},
       {"https://some.site.test/static/https_rule/", Result::kExclude},
       {"https://some.site.test:8000/", Result::kExclude}});

  // Note that what matters is when the rule was added, not how specific the URL
  // path prefix is. Let's add another rule now to show that.
  CheckAddUrlRuleTestCases(
      {{RuleType::kExclude, "some.site.test", "/", SessionError::kSuccess}});
  CheckEvaluateUrlTestCases(
      {{"https://some.site.test/static/included", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, AddUrlRuleToOriginThatMayIncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);

  // Without any rules yet, the basic rules is just the origin, because
  // include_site was not set.
  CheckEvaluateUrlTestCases({{"https://site.test/static", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});

  CheckAddUrlRuleTestCases(
      {{RuleType::kExclude, "excluded.site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "included.site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "site.test", "/static", SessionError::kSuccess},
       {RuleType::kInclude, "unrelated.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});

  EXPECT_EQ(params().specifications.size(), 1u);

  CheckEvaluateUrlTestCases(
      {// Path is excluded by rule.
       {"https://site.test/static", Result::kExclude},
       // Session is origin-scoped, so this rule is ignored.
       {"https://excluded.site.test", Result::kExclude},
       // Session is origin-scoped, so this rule is ignored.
       {"https://included.site.test", Result::kExclude},
       // Rule does not apply to wrong scheme.
       {"http://included.site.test", Result::kExclude},
       // No rules applies to these URLs, so the basic
       // rules (origin) applies.
       {"https://other.site.test", Result::kExclude},
       {"https://site.test/stuff", Result::kInclude},
       // Origin scoping takes precedence over rule.
       {"https://unrelated.test/", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, AddUrlRuleToRulesIncludingSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);
  SetIncludeSite(true);

  // Without any rules yet, the basic rules is the site.
  CheckEvaluateUrlTestCases({{"https://site.test/static", Result::kInclude},
                             {"https://other.site.test", Result::kInclude}});

  // Since the origin's host is the root eTLD+1, it is allowed to set rules that
  // affect URLs other than the setting origin (but still within the site).
  CheckAddUrlRuleTestCases(
      {{RuleType::kExclude, "excluded.site.test", "/", SessionError::kSuccess},
       {RuleType::kInclude, "included.site.test", "/", SessionError::kSuccess},
       {RuleType::kExclude, "site.test", "/static", SessionError::kSuccess},
       {RuleType::kInclude, "unrelated.test", "/",
        SessionError::kScopeRuleSiteScopedHostPatternMismatch}});

  EXPECT_EQ(params().specifications.size(), 3u);

  CheckEvaluateUrlTestCases(
      {// Path is excluded by rule.
       {"https://site.test/static", Result::kExclude},
       // Rule excludes URL explicitly.
       {"https://excluded.site.test", Result::kExclude},
       // Rule includes URL explicitly.
       {"https://included.site.test", Result::kInclude},
       // Rule does not apply to wrong scheme.
       {"http://included.site.test", Result::kExclude},
       // No rule applies to these URLs, so the basic rules (site) applies.
       {"https://other.site.test", Result::kInclude},
       {"https://site.test/stuff", Result::kInclude},
       // Site scoping takes precedence over rule.
       {"https://unrelated.test/", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, AddUrlRuleToRulesIncludingOrigin) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);
  SetIncludeSite(false);

  CheckAddUrlRuleTestCases(
      {{RuleType::kExclude, "excluded.site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kInclude, "included.site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "site.test", "/static", SessionError::kSuccess},
       {RuleType::kInclude, "unrelated.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});

  EXPECT_EQ(params().specifications.size(), 1u);

  CheckEvaluateUrlTestCases({// Path is excluded by rule.
                             {"https://site.test/static", Result::kExclude},
                             // Rejected rules
                             {"https://excluded.site.test", Result::kExclude},
                             {"https://included.site.test", Result::kExclude},
                             // No rules applies to these URLs, so the basic
                             // rules (which is only the origin) applies.
                             {"https://other.site.test", Result::kExclude},
                             {"https://site.test/stuff", Result::kInclude},
                             // Site scoping takes precedence over rule.
                             {"https://unrelated.test/", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, UrlRuleParsing) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  AssertDomainAndRegistry(root_site_origin, "site.test");

  // Use the most permissive type of inclusion_rules, to hit the interesting
  // edge cases.
  SetOrigin(root_site_origin);
  CheckMayIncludeSite(true);

  CheckAddUrlRuleTestCases(
      {// Empty host pattern not permitted.
       {RuleType::kExclude, "", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       // Host pattern that is only whitespace is not permitted.
       {RuleType::kExclude, " ", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Forbidden characters in host_pattern.
       {RuleType::kExclude, "https://site.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "site.test:8888", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "site.test,other.test", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Non-IPv6-allowable characters within the brackets.
       {RuleType::kExclude, "[*.:abcd::3:4:ff]", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       {RuleType::kExclude, "[1:ab+cd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "[[1:abcd::3:4:ff]]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Internal wildcard characters are forbidden in the host pattern.
       {RuleType::kExclude, "sub.*.site.test", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       // Multiple wildcard characters are forbidden in the host pattern.
       {RuleType::kExclude, "*.sub.*.site.test", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       // Wildcard must be followed by a dot.
       {RuleType::kExclude, "*site.test", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       // Wildcard may be followed by an eTLD, but will only match
       // for requests matching the scope origin or requests that are
       // subdomains of the site (depending on `include_site`).
       {RuleType::kExclude, "*.test", "/", SessionError::kSuccess},
       // Other sites are not allowed.
       {RuleType::kExclude, "unrelated.site", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "4.31.198.44", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "[1:abcd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "co.uk", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "com", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});
}

TEST_F(SessionInclusionRulesTest, UrlRuleParsingTopLevelDomain) {
  url::Origin tld_origin = url::Origin::Create(GURL("https://com"));

  AssertDomainAndRegistry(tld_origin, "");

  SetOrigin(tld_origin);
  CheckMayIncludeSite(false);

  CheckAddUrlRuleTestCases(
      {// Exact host is allowed.
       {RuleType::kExclude, "com", "/", SessionError::kSuccess},
       // Wildcards are not permitted.
       {RuleType::kExclude, "*.com", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Other hosts with no registrable domain are not allowed.
       {RuleType::kExclude, "4.31.198.44", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "[1:abcd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "co.uk", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});
}

TEST_F(SessionInclusionRulesTest, UrlRuleParsingIPv4Address) {
  url::Origin ip_origin = url::Origin::Create(GURL("https://4.31.198.44"));

  AssertDomainAndRegistry(ip_origin, "");

  SetOrigin(ip_origin);
  CheckMayIncludeSite(false);

  CheckAddUrlRuleTestCases(
      {// Exact host is allowed.
       {RuleType::kExclude, "4.31.198.44", "/", SessionError::kSuccess},
       // Wildcards are not permitted for IPv4 addresses.
       {RuleType::kExclude, "*.31.198.44", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "*.4.31.198.44", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Other hosts with no registrable domain are not allowed.
       {RuleType::kExclude, "[1:abcd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "co.uk", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "com", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});
}

TEST_F(SessionInclusionRulesTest, UrlRuleParsingIPv6Address) {
  url::Origin ipv6_origin =
      url::Origin::Create(GURL("https://[1:abcd::3:4:ff]"));

  AssertDomainAndRegistry(ipv6_origin, "");

  SetOrigin(ipv6_origin);
  CheckMayIncludeSite(false);

  CheckAddUrlRuleTestCases(
      {// Exact host is allowed.
       {RuleType::kExclude, "[1:abcd::3:4:ff]", "/", SessionError::kSuccess},
       // Wildcards are not permitted.
       {RuleType::kExclude, "*.[1:abcd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Brackets mismatched is not allowed.
       {RuleType::kExclude, "[1:abcd::3:4:ff", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "1:abcd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Non-IPv6-allowable characters within the brackets is not allowed.
       {RuleType::kExclude, "[*.:abcd::3:4:ff]", "/",
        SessionError::kInvalidScopeRuleHostPattern},
       {RuleType::kExclude, "[1:ab+cd::3:4:ff]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "[[1:abcd::3:4:ff]]", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       // Other hosts with no registrable domain are not allowed.
       {RuleType::kExclude, "4.31.198.44", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "co.uk", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch},
       {RuleType::kExclude, "com", "/",
        SessionError::kScopeRuleOriginScopedHostPatternMismatch}});
}

// This test is more to document the current behavior than anything else. We may
// discover a need for more comprehensive support for port numbers in the
// future.
TEST_F(SessionInclusionRulesTest, NonstandardPort) {
  url::Origin nonstandard_port_origin =
      url::Origin::Create(GURL("https://site.test:8888"));

  AssertDomainAndRegistry(nonstandard_port_origin, "site.test");

  SetOrigin(nonstandard_port_origin);
  CheckMayIncludeSite(true);

  // Without any URL rules, the default origin rule allows only the same origin.
  CheckEvaluateUrlTestCases({{"https://site.test", Result::kExclude},
                             {"https://site.test:8888", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});

  // If we include_site, then same-site URLs regardless of port number are
  // included.
  SetIncludeSite(true);
  CheckEvaluateUrlTestCases({{"https://site.test", Result::kInclude},
                             {"https://site.test:8888", Result::kInclude},
                             {"https://site.test:1234", Result::kInclude},
                             {"https://other.site.test", Result::kInclude}});

  // However, adding URL rules to an inclusion_rules based on such an origin may
  // lead to unintuitive outcomes. It is not possible to specify a rule that
  // applies to the same origin as the setting origin if the setting origin has
  // a nonstandard port.
  CheckAddUrlRuleTestCases(
      {// The pattern is not accepted
       {RuleType::kInclude, "site.test:8888", "/",
        SessionError::kScopeRuleSiteScopedHostPatternMismatch},
       // A rule with the same host without port specified is accepted.
       // This rule applies to any URL with the specified host.
       {RuleType::kExclude, "site.test", "/", SessionError::kSuccess},
       // The pattern is not accepted
       {RuleType::kInclude, "site.test:443", "/",
        SessionError::kScopeRuleSiteScopedHostPatternMismatch}});

  EXPECT_EQ(params().specifications.size(), 1u);

  CheckEvaluateUrlTestCases(
      {// This is same-origin but gets caught in the "site.test" rule because
       // the rule didn't specify a port.
       {"https://site.test:8888", Result::kExclude},
       // This is same-site but gets caught in the "site.test" rule because
       // the rule didn't specify a port.
       {"https://site.test:1234", Result::kExclude},
       // Same-site is included by basic rules.
       {"https://other.site.test", Result::kInclude},
       // Also excluded explicitly by rule.
       {"https://site.test", Result::kExclude},
       {"https://site.test:443", Result::kExclude}});
}

TEST_F(SessionInclusionRulesTest, ToFromProto) {
  // Create a valid SessionInclusionRules object with default inclusion rule and
  // a couple of additional URL rules.
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));
  AssertDomainAndRegistry(root_site_origin, "site.test");

  SessionParams::Scope params;
  params.include_site = true;
  params.specifications = {{RuleType::kExclude, "excluded.site.test", "/"},
                           {RuleType::kInclude, "included.site.test", "/"}};
  auto inclusion_rules_or_error = SessionInclusionRules::Create(
      root_site_origin, std::move(params), GURL());
  ASSERT_TRUE(inclusion_rules_or_error.has_value());
  SessionInclusionRules& rules = *inclusion_rules_or_error;

  // Create a corresponding proto object and validate.
  proto::SessionInclusionRules proto = rules.ToProto();
  EXPECT_EQ(root_site_origin.Serialize(), proto.origin());
  EXPECT_TRUE(proto.do_include_site());
  ASSERT_EQ(proto.url_rules().size(), 2);
  {
    const auto& rule = proto.url_rules(0);
    EXPECT_EQ(rule.rule_type(), proto::RuleType::EXCLUDE);
    EXPECT_EQ(rule.host_pattern(), "excluded.site.test");
    EXPECT_EQ(rule.path_prefix(), "/");
  }
  {
    const auto& rule = proto.url_rules(1);
    EXPECT_EQ(rule.rule_type(), proto::RuleType::INCLUDE);
    EXPECT_EQ(rule.host_pattern(), "included.site.test");
    EXPECT_EQ(rule.path_prefix(), "/");
  }

  // Create a SessionInclusionRules object from the proto and verify
  // that it is the same as the original.
  std::optional<SessionInclusionRules> restored_inclusion_rules =
      SessionInclusionRules::CreateFromProto(proto);
  ASSERT_TRUE(restored_inclusion_rules.has_value());
  EXPECT_EQ(*restored_inclusion_rules, rules);
}

TEST_F(SessionInclusionRulesTest, FailCreateFromInvalidProto) {
  // Empty proto.
  {
    proto::SessionInclusionRules proto;
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(proto));
  }
  // Opaque origin.
  {
    proto::SessionInclusionRules proto;
    proto.set_origin("about:blank");
    proto.set_do_include_site(false);
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(proto));
  }

  // Create a fully populated proto.
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));
  SessionParams::Scope params;
  params.include_site = true;
  params.specifications = {{RuleType::kExclude, "excluded.site.test", "/"},
                           {RuleType::kInclude, "included.site.test", "/"}};
  auto inclusion_rules_or_error = SessionInclusionRules::Create(
      root_site_origin, std::move(params), GURL());
  ASSERT_TRUE(inclusion_rules_or_error.has_value());
  SessionInclusionRules& rules = *inclusion_rules_or_error;
  proto::SessionInclusionRules proto = rules.ToProto();

  // The proto must actually be valid, or none of the following tests will be
  // validating anything.
  ASSERT_TRUE(SessionInclusionRules::CreateFromProto(proto).has_value());

  // Test for missing proto fields by clearing the fields one at a time.
  {
    proto::SessionInclusionRules p(proto);
    p.clear_origin();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
  {
    proto::SessionInclusionRules p(proto);
    p.clear_do_include_site();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
  // URL rules with missing parameters.
  {
    proto::SessionInclusionRules p(proto);
    p.mutable_url_rules(0)->clear_rule_type();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
  {
    proto::SessionInclusionRules p(proto);
    p.mutable_url_rules(0)->clear_host_pattern();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
  {
    proto::SessionInclusionRules p(proto);
    p.mutable_url_rules(0)->clear_path_prefix();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
}

}  // namespace net::device_bound_sessions
