// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_inclusion_rules.h"

#include <initializer_list>

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/device_bound_sessions/proto/storage.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

using Result = SessionInclusionRules::InclusionResult;

// These tests depend on the registry_controlled_domains code, so assert ahead
// of time that the eTLD+1 is what we expect, for clarity and to avoid confusing
// test failures.
#define ASSERT_DOMAIN_AND_REGISTRY(origin, expected_domain_and_registry)      \
  {                                                                           \
    ASSERT_EQ(                                                                \
        registry_controlled_domains::GetDomainAndRegistry(                    \
            origin, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES), \
        expected_domain_and_registry)                                         \
        << "Unexpected domain and registry.";                                 \
  }

struct EvaluateUrlTestCase {
  const char* url;
  Result expected_result;
};

void CheckEvaluateUrlTestCases(
    const SessionInclusionRules& inclusion_rules,
    std::initializer_list<EvaluateUrlTestCase> test_cases) {
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);
    EXPECT_EQ(inclusion_rules.EvaluateRequestUrl(GURL(test_case.url)),
              test_case.expected_result);
  }
}

struct AddUrlRuleTestCase {
  Result rule_type;
  const char* host_pattern;
  const char* path_prefix;
  bool expected_is_added;
};

void CheckAddUrlRuleTestCases(
    SessionInclusionRules& inclusion_rules,
    std::initializer_list<AddUrlRuleTestCase> test_cases) {
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::JoinString(
        {test_case.host_pattern, test_case.path_prefix}, ", "));
    bool is_added = inclusion_rules.AddUrlRuleIfValid(
        test_case.rule_type, test_case.host_pattern, test_case.path_prefix);
    EXPECT_EQ(is_added, test_case.expected_is_added);
  }
}

TEST(SessionInclusionRulesTest, DefaultConstructorMatchesNothing) {
  SessionInclusionRules inclusion_rules;
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  EXPECT_EQ(Result::kExclude,
            inclusion_rules.EvaluateRequestUrl(GURL("https://origin.test")));
  EXPECT_EQ(Result::kExclude, inclusion_rules.EvaluateRequestUrl(GURL()));
}

TEST(SessionInclusionRulesTest, DefaultIncludeOriginMayNotIncludeSite) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(subdomain_origin, "site.test");

  SessionInclusionRules inclusion_rules{subdomain_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  CheckEvaluateUrlTestCases(
      inclusion_rules, {// URL not valid.
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

TEST(SessionInclusionRulesTest, DefaultIncludeOriginThoughMayIncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  // All expectations are as above. Even though including the site is allowed,
  // because the origin's host is its root eTLD+1, it is still limited to a
  // default origin inclusion_rules because it did not set include_site.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {// URL not valid.
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

TEST(SessionInclusionRulesTest, IncludeSiteAttemptedButNotAllowed) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(subdomain_origin, "site.test");

  SessionInclusionRules inclusion_rules{subdomain_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  // Only the origin is included.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://some.site.test", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});

  // This shouldn't do anything.
  inclusion_rules.SetIncludeSite(true);
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  // Still only the origin is included.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://some.site.test", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});
}

TEST(SessionInclusionRulesTest, IncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  inclusion_rules.SetIncludeSite(true);

  CheckEvaluateUrlTestCases(
      inclusion_rules, {// URL not valid.
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

TEST(SessionInclusionRulesTest, AddUrlRuleToOriginOnly) {
  url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://some.site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(subdomain_origin, "site.test");

  SessionInclusionRules inclusion_rules{subdomain_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  // Only the origin is allowed, since the setting origin is not the root
  // eTLD+1. The only acceptable rules are limited to the origin/same host.
  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// Host pattern equals origin's host. Path is valid.
       {Result::kExclude, "some.site.test", "/static", true},
       // Add an opposite rule to check later.
       {Result::kInclude, "some.site.test", "/static/included", true},
       // Path not valid.
       {Result::kExclude, "some.site.test", "NotAPath", false},
       // Other host patterns not accepted.
       {Result::kExclude, "*.site.test", "/", false},
       {Result::kExclude, "unrelated.test", "/", false},
       {Result::kExclude, "site.test", "/", false},
       {Result::kExclude, "other.site.test", "/", false},
       {Result::kExclude, "https://some.site.test", "/", false},
       {Result::kExclude, "some.site.test:443", "/", false}});

  EXPECT_EQ(inclusion_rules.num_url_rules_for_testing(), 2u);

  CheckEvaluateUrlTestCases(
      inclusion_rules,
      {// Matches the rule.
       {"https://some.site.test/static", Result::kExclude},
       // A path under the rule's path prefix is subject to the rule.
       {"https://some.site.test/static/some/thing", Result::kExclude},
       // These do not match the rule, so are subject to the basic rules (the
       // origin).
       {"https://some.site.test/staticcccccccc", Result::kInclude},
       {"https://other.site.test/static", Result::kExclude},
       // The more recently added rule wins out.
       {"https://some.site.test/static/included", Result::kInclude}});

  // Note that what matters is when the rule was added, not how specific the URL
  // path prefix is. Let's add another rule now to show that.
  EXPECT_TRUE(inclusion_rules.AddUrlRuleIfValid(Result::kExclude,
                                                "some.site.test", "/"));
  EXPECT_EQ(Result::kExclude, inclusion_rules.EvaluateRequestUrl(GURL(
                                  "https://some.site.test/static/included")));
}

TEST(SessionInclusionRulesTest, AddUrlRuleToOriginThatMayIncludeSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  // Without any rules yet, the basic rules is just the origin, because
  // include_site was not set.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://site.test/static", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});

  // Since the origin's host is the root eTLD+1, it is allowed to set rules that
  // affect URLs other than the setting origin (but still within the site).
  CheckAddUrlRuleTestCases(inclusion_rules,
                           {{Result::kExclude, "excluded.site.test", "/", true},
                            {Result::kInclude, "included.site.test", "/", true},
                            {Result::kExclude, "site.test", "/static", true},
                            // Rules outside of the site are not allowed.
                            {Result::kExclude, "unrelated.test", "/", false}});

  EXPECT_EQ(inclusion_rules.num_url_rules_for_testing(), 3u);

  CheckEvaluateUrlTestCases(inclusion_rules,
                            {// Path is excluded by rule.
                             {"https://site.test/static", Result::kExclude},
                             // Rule excludes URL explicitly.
                             {"https://excluded.site.test", Result::kExclude},
                             // Rule includes URL explicitly.
                             {"https://included.site.test", Result::kInclude},
                             // Rule does not apply to wrong scheme.
                             {"http://included.site.test", Result::kExclude},
                             // No rules applies to these URLs, so the basic
                             // rules (origin) applies.
                             {"https://other.site.test", Result::kExclude},
                             {"https://site.test/stuff", Result::kInclude}});
}

TEST(SessionInclusionRulesTest, AddUrlRuleToRulesIncludingSite) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  inclusion_rules.SetIncludeSite(true);

  // Without any rules yet, the basic rules is the site.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://site.test/static", Result::kInclude},
                             {"https://other.site.test", Result::kInclude}});

  // Since the origin's host is the root eTLD+1, it is allowed to set rules that
  // affect URLs other than the setting origin (but still within the site).
  CheckAddUrlRuleTestCases(inclusion_rules,
                           {{Result::kExclude, "excluded.site.test", "/", true},
                            {Result::kInclude, "included.site.test", "/", true},
                            {Result::kExclude, "site.test", "/static", true},
                            // Rules outside of the site are not allowed.
                            {Result::kExclude, "unrelated.test", "/", false}});

  EXPECT_EQ(inclusion_rules.num_url_rules_for_testing(), 3u);

  CheckEvaluateUrlTestCases(
      inclusion_rules,
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
       {"https://site.test/stuff", Result::kInclude}});

  // Note that the rules are independent of "include_site", so even if that is
  // "revoked" the rules still work the same way.
  inclusion_rules.SetIncludeSite(false);
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {// Path is excluded by rule.
                             {"https://site.test/static", Result::kExclude},
                             // Rule excludes URL explicitly.
                             {"https://excluded.site.test", Result::kExclude},
                             // Rule includes URL explicitly.
                             {"https://included.site.test", Result::kInclude},
                             // No rules applies to these URLs, so the basic
                             // rules (which is now the origin) applies.
                             {"https://other.site.test", Result::kExclude},
                             {"https://site.test/stuff", Result::kInclude}});
}

TEST(SessionInclusionRulesTest, UrlRuleParsing) {
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));

  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  // Use the most permissive type of inclusion_rules, to hit the interesting
  // edge cases.
  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// Empty host pattern not permitted.
       {Result::kExclude, "", "/", false},
       // Host pattern that is only whitespace is not permitted.
       {Result::kExclude, " ", "/", false},
       // Forbidden characters in host_pattern.
       {Result::kExclude, "https://site.test", "/", false},
       {Result::kExclude, "site.test:8888", "/", false},
       {Result::kExclude, "site.test,other.test", "/", false},
       // Non-IPv6-allowable characters within the brackets.
       {Result::kExclude, "[*.:abcd::3:4:ff]", "/", false},
       {Result::kExclude, "[1:ab+cd::3:4:ff]", "/", false},
       {Result::kExclude, "[[1:abcd::3:4:ff]]", "/", false},
       // Internal wildcard characters are forbidden in the host pattern.
       {Result::kExclude, "sub.*.site.test", "/", false},
       // Multiple wildcard characters are forbidden in the host pattern.
       {Result::kExclude, "*.sub.*.site.test", "/", false},
       // Wildcard must be followed by a dot.
       {Result::kExclude, "*site.test", "/", false},
       // Wildcard must be followed by a non-eTLD.
       {Result::kExclude, "*.com", "/", false},
       // Other sites are not allowed.
       {Result::kExclude, "unrelated.site", "/", false},
       // Other hosts with no registrable domain are not allowed.
       {Result::kExclude, "4.31.198.44", "/", false},
       {Result::kExclude, "[1:abcd::3:4:ff]", "/", false},
       {Result::kExclude, "co.uk", "/", false},
       {Result::kExclude, "com", "/", false}});
}

TEST(SessionInclusionRulesTest, UrlRuleParsingTopLevelDomain) {
  url::Origin tld_origin = url::Origin::Create(GURL("https://com"));

  ASSERT_DOMAIN_AND_REGISTRY(tld_origin, "");

  SessionInclusionRules inclusion_rules{tld_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// Exact host is allowed.
       {Result::kExclude, "com", "/", true},
       // Wildcards are not permitted.
       {Result::kExclude, "*.com", "/", false},
       // Other hosts with no registrable domain are not allowed.
       {Result::kExclude, "4.31.198.44", "/", false},
       {Result::kExclude, "[1:abcd::3:4:ff]", "/", false},
       {Result::kExclude, "co.uk", "/", false}});
}

TEST(SessionInclusionRulesTest, UrlRuleParsingIPv4Address) {
  url::Origin ip_origin = url::Origin::Create(GURL("https://4.31.198.44"));

  ASSERT_DOMAIN_AND_REGISTRY(ip_origin, "");

  SessionInclusionRules inclusion_rules{ip_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// Exact host is allowed.
       {Result::kExclude, "4.31.198.44", "/", true},
       // Wildcards are not permitted.
       {Result::kExclude, "*.31.198.44", "/", false},
       {Result::kExclude, "*.4.31.198.44", "/", false},
       // Other hosts with no registrable domain are not allowed.
       {Result::kExclude, "[1:abcd::3:4:ff]", "/", false},
       {Result::kExclude, "co.uk", "/", false},
       {Result::kExclude, "com", "/", false}});
}

TEST(SessionInclusionRulesTest, UrlRuleParsingIPv6Address) {
  url::Origin ipv6_origin =
      url::Origin::Create(GURL("https://[1:abcd::3:4:ff]"));

  ASSERT_DOMAIN_AND_REGISTRY(ipv6_origin, "");

  SessionInclusionRules inclusion_rules{ipv6_origin};
  EXPECT_FALSE(inclusion_rules.may_include_site_for_testing());

  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// Exact host is allowed.
       {Result::kExclude, "[1:abcd::3:4:ff]", "/", true},
       // Wildcards are not permitted.
       {Result::kExclude, "*.[1:abcd::3:4:ff]", "/", false},
       // Brackets mismatched.
       {Result::kExclude, "[1:abcd::3:4:ff", "/", false},
       {Result::kExclude, "1:abcd::3:4:ff]", "/", false},
       // Non-IPv6-allowable characters within the brackets.
       {Result::kExclude, "[*.:abcd::3:4:ff]", "/", false},
       {Result::kExclude, "[1:ab+cd::3:4:ff]", "/", false},
       {Result::kExclude, "[[1:abcd::3:4:ff]]", "/", false},
       // Other hosts with no registrable domain are not allowed.
       {Result::kExclude, "4.31.198.44", "/", false},
       {Result::kExclude, "co.uk", "/", false},
       {Result::kExclude, "com", "/", false}});
}

// This test is more to document the current behavior than anything else. We may
// discover a need for more comprehensive support for port numbers in the
// future, in which case:
// TODO(chlily): Support port numbers in URL rules.
TEST(SessionInclusionRulesTest, NonstandardPort) {
  url::Origin nonstandard_port_origin =
      url::Origin::Create(GURL("https://site.test:8888"));

  ASSERT_DOMAIN_AND_REGISTRY(nonstandard_port_origin, "site.test");

  SessionInclusionRules inclusion_rules{nonstandard_port_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());

  // Without any URL rules, the default origin rule allows only the same origin.
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://site.test", Result::kExclude},
                             {"https://site.test:8888", Result::kInclude},
                             {"https://other.site.test", Result::kExclude}});

  // If we include_site, then same-site URLs regardless of port number are
  // included.
  inclusion_rules.SetIncludeSite(true);
  CheckEvaluateUrlTestCases(inclusion_rules,
                            {{"https://site.test", Result::kInclude},
                             {"https://site.test:8888", Result::kInclude},
                             {"https://site.test:1234", Result::kInclude},
                             {"https://other.site.test", Result::kInclude}});

  // However, adding URL rules to an inclusion_rules based on such an origin may
  // lead to unintuitive outcomes. It is not possible to specify a rule that
  // applies to the same origin as the setting origin if the setting origin has
  // a nonstandard port.
  CheckAddUrlRuleTestCases(
      inclusion_rules,
      {// The pattern is rejected due to the colon, despite being the
       // same origin.
       {Result::kExclude, "site.test:8888", "/", false},
       // A rule with the same host without port specified is accepted.
       // This rule applies to any URL with the specified host.
       {Result::kExclude, "site.test", "/", true},
       // Any explicitly specified port is rejected (due to the colon),
       // even if it's the standard one.
       {Result::kExclude, "site.test:443", "/", false}});

  EXPECT_EQ(inclusion_rules.num_url_rules_for_testing(), 1u);

  CheckEvaluateUrlTestCases(
      inclusion_rules,
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

TEST(SessionInclusionRulesTest, ToFromProto) {
  // Create a valid SessionInclusionRules object with default inclusion rule and
  // a couple of additional URL rules.
  url::Origin root_site_origin = url::Origin::Create(GURL("https://site.test"));
  ASSERT_DOMAIN_AND_REGISTRY(root_site_origin, "site.test");

  SessionInclusionRules inclusion_rules{root_site_origin};
  EXPECT_TRUE(inclusion_rules.may_include_site_for_testing());
  inclusion_rules.SetIncludeSite(true);
  EXPECT_TRUE(inclusion_rules.AddUrlRuleIfValid(Result::kExclude,
                                                "excluded.site.test", "/"));
  EXPECT_TRUE(inclusion_rules.AddUrlRuleIfValid(Result::kInclude,
                                                "included.site.test", "/"));

  // Create a corresponding proto object and validate.
  proto::SessionInclusionRules proto = inclusion_rules.ToProto();
  EXPECT_EQ(root_site_origin.Serialize(), proto.origin());
  EXPECT_TRUE(proto.do_include_site());
  ASSERT_EQ(proto.url_rules().size(), 2);
  {
    const auto& rule = proto.url_rules(0);
    EXPECT_EQ(rule.rule_type(), proto::RuleType::EXCLUDE);
    EXPECT_EQ(rule.host_matcher_rule(), "excluded.site.test");
    EXPECT_EQ(rule.path_prefix(), "/");
  }
  {
    const auto& rule = proto.url_rules(1);
    EXPECT_EQ(rule.rule_type(), proto::RuleType::INCLUDE);
    EXPECT_EQ(rule.host_matcher_rule(), "included.site.test");
    EXPECT_EQ(rule.path_prefix(), "/");
  }

  // Create a SessionInclusionRules object from the proto and verify
  // that it is the same as the original.
  std::unique_ptr<SessionInclusionRules> restored_inclusion_rules =
      SessionInclusionRules::CreateFromProto(proto);
  ASSERT_TRUE(restored_inclusion_rules != nullptr);
  EXPECT_EQ(*restored_inclusion_rules, inclusion_rules);
}

TEST(SessionInclusionRulesTest, FailCreateFromInvalidProto) {
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
  SessionInclusionRules inclusion_rules{root_site_origin};
  inclusion_rules.SetIncludeSite(true);
  inclusion_rules.AddUrlRuleIfValid(Result::kExclude, "excluded.site.test",
                                    "/");
  inclusion_rules.AddUrlRuleIfValid(Result::kInclude, "included.site.test",
                                    "/");
  proto::SessionInclusionRules proto = inclusion_rules.ToProto();

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
    p.mutable_url_rules(0)->clear_host_matcher_rule();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
  {
    proto::SessionInclusionRules p(proto);
    p.mutable_url_rules(0)->clear_path_prefix();
    EXPECT_FALSE(SessionInclusionRules::CreateFromProto(p));
  }
}

}  // namespace

}  // namespace net::device_bound_sessions
