// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_mapping_rules.h"

#include <string.h>

#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
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

TEST(HostMappingRulesTest, RewritesUrl) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  GURL url("http://initial.test:111");
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("http://replacement.test:1000"));
}

TEST(HostMappingRulesTest, RewritesUrlToIpv6Literal) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test [2345:6789::0abc]:1112");

  GURL url("http://initial.test:111");
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("http://[2345:6789::0abc]:1112"));
}

TEST(HostMappingRulesTest, RewritesUrlPreservingScheme) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  GURL url("wss://initial.test:222");
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("wss://replacement.test:1000"));
}

TEST(HostMappingRulesTest, RewritesFileUrl) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  // Expect replacement port to be ignored because file URLs do not use port.
  GURL url("file://initial.test/file.txt");
  ASSERT_EQ(url.EffectiveIntPort(), url::PORT_UNSPECIFIED);
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("file://replacement.test/file.txt"));
  EXPECT_EQ(url.EffectiveIntPort(), url::PORT_UNSPECIFIED);
}

TEST(HostMappingRulesTest, RewritesAnyStandardUrlWithPort) {
  const char kScheme[] = "foo";
  url::ScopedSchemeRegistryForTests scoped_registry;
  AddStandardScheme(kScheme, url::SCHEME_WITH_HOST_AND_PORT);
  ASSERT_TRUE(url::IsStandard(kScheme, url::Component(0, strlen(kScheme))));

  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  GURL url("foo://initial.test:100");
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("foo://replacement.test:1000"));
}

TEST(HostMappingRulesTest, RewritesAnyStandardUrlWithoutPort) {
  const char kScheme[] = "foo";
  url::ScopedSchemeRegistryForTests scoped_registry;
  AddStandardScheme(kScheme, url::SCHEME_WITH_HOST);
  ASSERT_TRUE(url::IsStandard(kScheme, url::Component(0, strlen(kScheme))));

  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  // Expect replacement port to be ignored.
  GURL url("foo://initial.test");
  ASSERT_EQ(url.EffectiveIntPort(), url::PORT_UNSPECIFIED);
  EXPECT_EQ(rules.RewriteUrl(url), HostMappingRules::RewriteResult::kRewritten);
  EXPECT_EQ(url, GURL("foo://replacement.test"));
  EXPECT_EQ(url.EffectiveIntPort(), url::PORT_UNSPECIFIED);
}

TEST(HostMappingRulesTest, IgnoresUnmappedUrls) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test replacement.test:1000");

  GURL url("http://different.test:111");
  EXPECT_EQ(rules.RewriteUrl(url),
            HostMappingRules::RewriteResult::kNoMatchingRule);
  EXPECT_EQ(url, GURL("http://different.test:111"));
}

TEST(HostMappingRulesTest, IgnoresInvalidReplacementUrls) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test invalid/url");

  GURL url("http://initial.test");
  EXPECT_EQ(rules.RewriteUrl(url),
            HostMappingRules::RewriteResult::kInvalidRewrite);
  EXPECT_EQ(url, GURL("http://initial.test"));
}

// Remapping to "^NOTFOUND" is documented as a special case for
// MappedHostResolver usage. Ensure that it is handled as invalid as expected.
TEST(HostMappingRulesTest, NotFoundIgnoredAsInvalidUrl) {
  HostMappingRules rules;
  rules.AddRuleFromString("MAP initial.test ^NOTFOUND");

  GURL url("http://initial.test");
  EXPECT_EQ(rules.RewriteUrl(url),
            HostMappingRules::RewriteResult::kInvalidRewrite);
  EXPECT_EQ(url, GURL("http://initial.test"));
}

}  // namespace

}  // namespace net
