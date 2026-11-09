// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_mapping_rules.h"

#include <string>

#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace net {

namespace {

TEST(HostMappingRulesTest, SetRulesFromString) {
  HostMappingRules rules;
  rules.SetRulesFromString(
      "map *.com baz , map *.net bar:60, EXCLUDE *.foo.com");

  HostPortPair host_port("test", 1234);
  EXPECT_FALSE(rules.RewriteHost(&host_port));
  EXPECT_EQ("test", host_port.host());
  EXPECT_EQ(1234u, host_port.port());

  host_port = HostPortPair("chrome.net", 80);
  EXPECT_TRUE(rules.RewriteHost(&host_port));
  EXPECT_EQ("bar", host_port.host());
  EXPECT_EQ(60u, host_port.port());

  host_port = HostPortPair("crack.com", 80);
  EXPECT_TRUE(rules.RewriteHost(&host_port));
  EXPECT_EQ("baz", host_port.host());
  EXPECT_EQ(80u, host_port.port());

  host_port = HostPortPair("wtf.foo.com", 666);
  EXPECT_FALSE(rules.RewriteHost(&host_port));
  EXPECT_EQ("wtf.foo.com", host_port.host());
  EXPECT_EQ(666u, host_port.port());
}

TEST(HostMappingRulesTest, PortSpecificMatching) {
  HostMappingRules rules;
  rules.SetRulesFromString(
      "map *.com:80 baz:111 , map *.com:443 blat:333, EXCLUDE *.foo.com");

  // No match
  HostPortPair host_port("test.com", 1234);
  EXPECT_FALSE(rules.RewriteHost(&host_port));
  EXPECT_EQ("test.com", host_port.host());
  EXPECT_EQ(1234u, host_port.port());

  // Match port 80
  host_port = HostPortPair("crack.com", 80);
  EXPECT_TRUE(rules.RewriteHost(&host_port));
  EXPECT_EQ("baz", host_port.host());
  EXPECT_EQ(111u, host_port.port());

  // Match port 443
  host_port = HostPortPair("wtf.com", 443);
  EXPECT_TRUE(rules.RewriteHost(&host_port));
  EXPECT_EQ("blat", host_port.host());
  EXPECT_EQ(333u, host_port.port());

  // Match port 443, but excluded.
  host_port = HostPortPair("wtf.foo.com", 443);
  EXPECT_FALSE(rules.RewriteHost(&host_port));
  EXPECT_EQ("wtf.foo.com", host_port.host());
  EXPECT_EQ(443u, host_port.port());
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(HostMappingRulesTest, ParseInvalidRules) {
  HostMappingRules rules;

  EXPECT_FALSE(rules.AddRuleFromString("xyz"));
  EXPECT_FALSE(rules.AddRuleFromString(std::string()));
  EXPECT_FALSE(rules.AddRuleFromString(" "));
  EXPECT_FALSE(rules.AddRuleFromString("EXCLUDE"));
  EXPECT_FALSE(rules.AddRuleFromString("EXCLUDE foo bar"));
  EXPECT_FALSE(rules.AddRuleFromString("INCLUDE"));
  EXPECT_FALSE(rules.AddRuleFromString("INCLUDE x"));
  EXPECT_FALSE(rules.AddRuleFromString("INCLUDE x :10"));
}

TEST(HostMappingRulesTest, RewritesSchemeHostPortStandardAndPortlessSchemes) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test reMapped.TEST:1000");
  rules.AddRuleFromString("MAP ipv6.test [1234:5678::000A]");
  rules.AddRuleFromString("MAP ipv6port.test [2345:6789::0abc]:1112");
  rules.AddRuleFromString("MAP invalid.test invalid/url");
  rules.AddRuleFromString("MAP notfound.test ^NOTFOUND");

  url::SchemeHostPort https_shp(url::kHttpsScheme, "initial.test", 443);
  EXPECT_EQ(rules.RewriteSchemeHostPort(https_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(https_shp,
            url::SchemeHostPort(url::kHttpsScheme, "remapped.test", 1000));

  url::SchemeHostPort wss_shp(url::kWssScheme, "initial.test", 222);
  EXPECT_EQ(rules.RewriteSchemeHostPort(wss_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(wss_shp,
            url::SchemeHostPort(url::kWssScheme, "remapped.test", 1000));

  url::SchemeHostPort ipv6_shp(url::kHttpScheme, "ipv6.test", 80);
  EXPECT_EQ(rules.RewriteSchemeHostPort(ipv6_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(ipv6_shp,
            url::SchemeHostPort(url::kHttpScheme, "[1234:5678::a]", 80));

  url::SchemeHostPort ipv6_port_shp(url::kHttpScheme, "ipv6port.test", 111);
  EXPECT_EQ(rules.RewriteSchemeHostPort(ipv6_port_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(ipv6_port_shp,
            url::SchemeHostPort(url::kHttpScheme, "[2345:6789::abc]", 1112));

  // Portless standard schemes (e.g. file://) ignore replacement ports.
  url::SchemeHostPort file_shp(url::kFileScheme, "initial.test", 0);
  EXPECT_EQ(rules.RewriteSchemeHostPort(file_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(file_shp,
            url::SchemeHostPort(url::kFileScheme, "remapped.test", 0));

  url::SchemeHostPort unmapped_shp(url::kHttpsScheme, "other.test", 443);
  EXPECT_EQ(rules.RewriteSchemeHostPort(unmapped_shp),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_EQ(unmapped_shp,
            url::SchemeHostPort(url::kHttpsScheme, "other.test", 443));

  url::SchemeHostPort invalid_shp(url::kHttpsScheme, "invalid.test", 443);
  EXPECT_EQ(rules.RewriteSchemeHostPort(invalid_shp),
            HostMappingRules::RewriteResult::kInvalidRewrite);
  EXPECT_EQ(invalid_shp,
            url::SchemeHostPort(url::kHttpsScheme, "invalid.test", 443));

  // Remapping to "^NOTFOUND" is documented as a special case for
  // MappedHostResolver usage and must return `kInvalidRewrite` unmodified.
  url::SchemeHostPort notfound_shp(url::kHttpsScheme, "notfound.test", 443);
  EXPECT_EQ(rules.RewriteSchemeHostPort(notfound_shp),
            HostMappingRules::RewriteResult::kInvalidRewrite);
  EXPECT_EQ(notfound_shp,
            url::SchemeHostPort(url::kHttpsScheme, "notfound.test", 443));
}

TEST(HostMappingRulesTest, RewritesSchemeHostPortCustomStandardSchemes) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("customstd", url::SCHEME_WITH_HOST_AND_PORT);
  url::AddStandardScheme("portlessstd", url::SCHEME_WITH_HOST);

  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test");
  rules.AddRuleFromString("MAP withport.test replacement.test:1000");

  // Even though DefaultPortForScheme("customstd") is PORT_UNSPECIFIED, the
  // explicit port on `SchemeHostPort` must be preserved rather than corrupted.
  url::SchemeHostPort shp("customstd", "initial.test", 443);
  ASSERT_TRUE(shp.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(shp, url::SchemeHostPort("customstd", "replacement.test", 443));

  url::SchemeHostPort with_port_shp("customstd", "withport.test", 100);
  ASSERT_TRUE(with_port_shp.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(with_port_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(with_port_shp,
            url::SchemeHostPort("customstd", "replacement.test", 1000));

  // Custom standard schemes without ports ignore replacement ports.
  url::SchemeHostPort portless_shp("portlessstd", "withport.test", 0);
  ASSERT_TRUE(portless_shp.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(portless_shp),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(portless_shp,
            url::SchemeHostPort("portlessstd", "replacement.test", 0));
}

TEST(HostMappingRulesTest, RewritesLocalNonStandardSchemeHostPort) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddLocalScheme("localnonstd");

  HostMappingRules rules;
  rules.SetRulesFromString(
      "MAP initial.test replacement.test, "
      "MAP 1234:5678::a ipv6target.test:9090, "
      "MAP notfound.test ^NOTFOUND, "
      "MAP *.wild.test wildtarget.test, "
      "EXCLUDE skip.wild.test");

  // Unmapped non-standard SchemeHostPort must not hit DCHECK(url.IsStandard()).
  url::SchemeHostPort unmapped("localnonstd", "other.test", 80);
  ASSERT_TRUE(unmapped.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(unmapped),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_EQ(unmapped, url::SchemeHostPort("localnonstd", "other.test", 80));

  // Mapped non-standard SchemeHostPort rewrites host and preserves port.
  url::SchemeHostPort mapped("localnonstd", "initial.test", 8080);
  ASSERT_TRUE(mapped.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(mapped),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(mapped,
            url::SchemeHostPort("localnonstd", "replacement.test", 8080));

  // Opaque-host non-standard schemes preserve uppercase ASCII, which must be
  // canonicalized so lowercased MAP and EXCLUDE rules match accurately.
  url::SchemeHostPort uppercase_mapped("localnonstd", "Initial.TEST", 8080);
  ASSERT_TRUE(uppercase_mapped.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(uppercase_mapped),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(uppercase_mapped,
            url::SchemeHostPort("localnonstd", "replacement.test", 8080));

  url::SchemeHostPort uppercase_excluded("localnonstd", "SKIP.WILD.TEST", 8080);
  ASSERT_TRUE(uppercase_excluded.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(uppercase_excluded),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_EQ(uppercase_excluded,
            url::SchemeHostPort("localnonstd", "SKIP.WILD.TEST", 8080));

  // Uncontracted IPv6 literal on a local scheme canonicalizes before matching.
  url::SchemeHostPort ipv6_local("localnonstd", "[1234:5678::000A]", 80);
  ASSERT_TRUE(ipv6_local.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(ipv6_local),
            HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(ipv6_local,
            url::SchemeHostPort("localnonstd", "ipv6target.test", 9090));

  url::SchemeHostPort notfound("localnonstd", "notfound.test", 80);
  ASSERT_TRUE(notfound.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(notfound),
            HostMappingRules::RewriteResult::kInvalidRewrite);

  // Malformed Mojo-style non-standard hosts (unmatched '[' or embedded NUL)
  // must return `kNoMatchingRule` even under a wildcard rule without hitting
  // `HostPortPair::HostForURL()` assertions.
  HostMappingRules wildcard_rules;
  wildcard_rules.SetRulesFromString("MAP * replacement.test");
  for (const std::string& bad_host :
       {std::string("[foo:bar"), std::string("foo\0bar", 7)}) {
    url::SchemeHostPort broken("localnonstd", bad_host, 80);
    ASSERT_TRUE(broken.IsValid());
    EXPECT_EQ(wildcard_rules.RewriteSchemeHostPort(broken),
              HostMappingRules::RewriteResult::kNoMatchingRule);
    EXPECT_EQ(broken.host(), bad_host);
  }
}

TEST(HostMappingRulesTest,
     IgnoresInvalidAndHostlessSchemeHostPortWithWildcardRules) {
  HostMappingRules rules;
  rules.SetRulesFromString(
      "MAP * replacement.test:1000, MAP *:0 replacement.test:1000");

  url::SchemeHostPort invalid_shp;
  ASSERT_FALSE(invalid_shp.IsValid());
  EXPECT_EQ(rules.RewriteSchemeHostPort(invalid_shp),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_FALSE(invalid_shp.IsValid());

  // Hostless `file:///` must not gain a network host from a wildcard rule.
  url::SchemeHostPort empty_file_shp(url::kFileScheme, "", 0);
  ASSERT_TRUE(empty_file_shp.IsValid());
  ASSERT_TRUE(empty_file_shp.host().empty());
  EXPECT_EQ(rules.RewriteSchemeHostPort(empty_file_shp),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_EQ(empty_file_shp, url::SchemeHostPort(url::kFileScheme, "", 0));

  {
    url::ScopedSchemeRegistryForTests scoped_registry;
    url::AddLocalScheme("localnonstd");

    url::SchemeHostPort empty_local_shp("localnonstd", "", 0);
    ASSERT_TRUE(empty_local_shp.IsValid());
    ASSERT_TRUE(empty_local_shp.host().empty());
    EXPECT_EQ(rules.RewriteSchemeHostPort(empty_local_shp),
              HostMappingRules::RewriteResult::kNoMatchingRule);
    EXPECT_EQ(empty_local_shp, url::SchemeHostPort("localnonstd", "", 0));
  }

  {
    // With Android WebView non-standard schemes enabled, `SchemeHostPort`
    // discards host/port (`host() == ""`) while keeping `IsValid() == true`.
    // Wildcard rules must still return `kNoMatchingRule` rather than claiming
    // `kRewritten` on an unchanged tuple.
    url::ScopedSchemeRegistryForTests scoped_registry;
    url::EnableNonStandardSchemesForAndroidWebView();

    url::SchemeHostPort webview_shp("nonstd", "initial.test", 8080);
    ASSERT_TRUE(webview_shp.IsValid());
    ASSERT_TRUE(webview_shp.host().empty());
    EXPECT_EQ(rules.RewriteSchemeHostPort(webview_shp),
              HostMappingRules::RewriteResult::kNoMatchingRule);
    EXPECT_TRUE(webview_shp.host().empty());
  }
}

}  // namespace

}  // namespace net
