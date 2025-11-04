// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_host_matching_rules.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
// On Windows, "loopback" resolves to localhost and is implicitly bypassed to
// match WinInet.
#define BYPASS_LOOPBACK
#endif

namespace net {

namespace {

// Calls |rules.Matches()| for each name in |hosts| (for various URL schemes),
// and checks that the result is |matches|. If the host is in |inverted_hosts|
// then the expectation is reversed.
void ExpectRulesMatch(const ProxyHostMatchingRules& rules,
                      base::span<const std::string_view> hosts,
                      bool matches,
                      const std::set<std::string>& inverted_hosts) {
  // The scheme of the URL shouldn't matter.
  const char* kUrlSchemes[] = {"http://", "https://", "ftp://"};

  for (auto* scheme : kUrlSchemes) {
    for (std::string_view host : hosts) {
      bool expectation = matches;

      if (inverted_hosts.count(std::string(host)) != 0) {
        expectation = !expectation;
      }

      std::string url = std::string(scheme) + std::string(host);

      EXPECT_EQ(expectation, rules.Matches(GURL(url))) << url;
    }
  }
}

// Tests calling |rules.Matches()| for localhost URLs returns |matches|.
void ExpectMatchLocalhost(
    const ProxyHostMatchingRules& rules,
    bool matches,
    const std::set<std::string>& inverted_hosts = std::set<std::string>()) {
  std::string_view kHosts[] = {
      "localhost",
      "localhost.",
      "foo.localhost",
      "127.0.0.1",
      "127.100.0.2",
      "[::1]",
      "[::0:FFFF:127.0.0.1]",
      "[::fFfF:127.100.0.0]",
      "[0::ffff:7f00:1]",
#if defined(BYPASS_LOOPBACK)
      "loopback",
      "loopback.",
#endif
  };

  ExpectRulesMatch(rules, kHosts, matches, inverted_hosts);
}

// Tests calling |rules.Matches()| for link-local URLs returns |matches|.
void ExpectMatchesLinkLocal(const ProxyHostMatchingRules& rules, bool matches) {
  std::string_view kHosts[] = {
      "169.254.3.2", "169.254.100.1",        "[FE80::8]",
      "[fe91::1]",   "[::ffff:169.254.3.2]",
  };

  ExpectRulesMatch(rules, kHosts, matches, {});
}

// Tests calling |rules.Matches()| with miscelaneous URLs that are neither
// localhost or link local IPs, returns |matches|.
void ExpectMatchesMisc(
    const ProxyHostMatchingRules& rules,
    bool matches,
    const std::set<std::string>& inverted_hosts = std::set<std::string>()) {
  std::string_view kHosts[] = {
      "192.168.0.1",
      "170.254.0.0",
      "128.0.0.1",
      "[::2]",
      "[FD80::1]",
      "foo",
      "www.example3.com",
      "[::ffff:128.0.0.1]",
      "[::ffff:126.100.0.0]",
      "[::ffff::ffff:127.0.0.1]",
      "[::ffff:0:127.0.0.1]",
      "[::127.0.0.1]",
#if !defined(BYPASS_LOOPBACK)
      "loopback",
      "loopback.",
#endif
  };

  ExpectRulesMatch(rules, kHosts, matches, inverted_hosts);
}

TEST(ProxyHostMatchingRulesTest, ParseAndMatchBasicHost) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("wWw.gOogle.com");
  ASSERT_EQ(1u, rules.rules().size());
  // Hostname rules are normalized to lower-case.
  EXPECT_EQ("www.google.com", rules.rules()[0]->ToString());

  // All of these match; port, scheme, and non-hostname components don't
  // matter.
  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(rules.Matches(GURL("ftp://www.google.com:99")));
  EXPECT_TRUE(rules.Matches(GURL("https://www.google.com:81")));

  // Must be a strict host match to work.
  EXPECT_FALSE(rules.Matches(GURL("http://foo.www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://xxx.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com.baz.org")));
}

TEST(ProxyHostMatchingRulesTest, ParseAndMatchBasicDomain) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString(".gOOgle.com");
  ASSERT_EQ(1u, rules.rules().size());
  // Hostname rules are normalized to lower-case.
  // Note that we inferred this was an "ends with" test.
  EXPECT_EQ("*.google.com", rules.rules()[0]->ToString());

  // All of these match; port, scheme, and non-hostname components don't
  // matter.
  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(rules.Matches(GURL("ftp://www.google.com:99")));
  EXPECT_TRUE(rules.Matches(GURL("https://a.google.com:81")));
  EXPECT_TRUE(rules.Matches(GURL("http://foo.google.com/x/y?q")));
  EXPECT_TRUE(rules.Matches(GURL("http://foo:bar@baz.google.com#x")));

  // Must be a strict "ends with" to work.
  EXPECT_FALSE(rules.Matches(GURL("http://google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://foo.google.com.baz.org")));
}

TEST(ProxyHostMatchingRulesTest, ParseAndMatchBasicDomainWithPort) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("*.GOOGLE.com:80");
  ASSERT_EQ(1u, rules.rules().size());
  // Hostname rules are normalized to lower-case.
  EXPECT_EQ("*.google.com:80", rules.rules()[0]->ToString());

  // All of these match; scheme, and non-hostname components don't matter.
  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(rules.Matches(GURL("ftp://www.google.com:80")));
  EXPECT_TRUE(rules.Matches(GURL("https://a.google.com:80?x")));

  // Must be a strict "ends with" to work.
  EXPECT_FALSE(rules.Matches(GURL("http://google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://foo.google.com.baz.org")));

  // The ports must match.
  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com:90")));
  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
}

TEST(ProxyHostMatchingRulesTest, MatchAll) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("*");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("*", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(rules.Matches(GURL("ftp://www.foobar.com:99")));
  EXPECT_TRUE(rules.Matches(GURL("https://a.google.com:80?x")));
}

TEST(ProxyHostMatchingRulesTest, WildcardAtStart) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("*.org:443");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("*.org:443", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://www.google.org:443")));
  EXPECT_TRUE(rules.Matches(GURL("https://www.google.org")));

  EXPECT_FALSE(rules.Matches(GURL("http://www.google.org")));
  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("https://www.google.org.com")));
}

// Tests a codepath that parses hostnamepattern:port, where "port" is invalid
// by containing a leading plus.
TEST(ProxyHostMatchingRulesTest, ParseInvalidPort) {
  ProxyHostMatchingRules rules;
  EXPECT_TRUE(rules.AddRuleFromString("*.org:443"));
  EXPECT_FALSE(rules.AddRuleFromString("*.com:+443"));
  EXPECT_FALSE(rules.AddRuleFromString("*.com:-443"));
}

TEST(ProxyHostMatchingRulesTest, IPV4Address) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("192.168.1.1");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("192.168.1.1", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://192.168.1.1")));
  EXPECT_TRUE(rules.Matches(GURL("https://192.168.1.1:90")));

  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://sup.192.168.1.1")));
}

TEST(ProxyHostMatchingRulesTest, IPV4AddressWithPort) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("192.168.1.1:33");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("192.168.1.1:33", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://192.168.1.1:33")));

  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.168.1.1")));
  EXPECT_FALSE(rules.Matches(GURL("http://sup.192.168.1.1:33")));
}

TEST(ProxyHostMatchingRulesTest, IPV6Address) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("[3ffe:2a00:100:7031:0:0::1]");
  ASSERT_EQ(1u, rules.rules().size());
  // Note that we canonicalized the IP address.
  EXPECT_EQ("[3ffe:2a00:100:7031::1]", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://[3ffe:2a00:100:7031::1]")));
  EXPECT_TRUE(rules.Matches(GURL("http://[3ffe:2a00:100:7031::1]:33")));

  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://sup.192.168.1.1:33")));
}

TEST(ProxyHostMatchingRulesTest, IPV6AddressWithPort) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("[3ffe:2a00:100:7031::1]:33");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("[3ffe:2a00:100:7031::1]:33", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://[3ffe:2a00:100:7031::1]:33")));

  EXPECT_FALSE(rules.Matches(GURL("http://[3ffe:2a00:100:7031::1]")));
  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com")));
}

TEST(ProxyHostMatchingRulesTest, HTTPOnly) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("http://www.google.com");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("http://www.google.com", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com/foo")));
  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com:99")));

  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("ftp://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://foo.www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com.org")));
  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
}

TEST(ProxyHostMatchingRulesTest, HTTPOnlyWithWildcard) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("http://*www.google.com");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("http://*www.google.com", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com/foo")));
  EXPECT_TRUE(rules.Matches(GURL("http://www.google.com:99")));
  EXPECT_TRUE(rules.Matches(GURL("http://foo.www.google.com")));

  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("ftp://www.google.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://www.google.com.org")));
  EXPECT_FALSE(rules.Matches(GURL("https://www.google.com")));
}

TEST(ProxyHostMatchingRulesTest, DoesNotUseSuffixMatching) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString(
      "foo1.com, .foo2.com, 192.168.1.1, "
      "*foobar.com:80, *.foo, http://baz, <local>");
  ASSERT_EQ(7u, rules.rules().size());
  EXPECT_EQ("foo1.com", rules.rules()[0]->ToString());
  EXPECT_EQ("*.foo2.com", rules.rules()[1]->ToString());
  EXPECT_EQ("192.168.1.1", rules.rules()[2]->ToString());
  EXPECT_EQ("*foobar.com:80", rules.rules()[3]->ToString());
  EXPECT_EQ("*.foo", rules.rules()[4]->ToString());
  EXPECT_EQ("http://baz", rules.rules()[5]->ToString());
  EXPECT_EQ("<local>", rules.rules()[6]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://foo1.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://aaafoo1.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://aaafoo1.com.net")));
}

TEST(ProxyHostMatchingRulesTest, MultipleRules) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString(".google.com , .foobar.com:30");
  ASSERT_EQ(2u, rules.rules().size());

  EXPECT_TRUE(rules.Matches(GURL("http://baz.google.com:40")));
  EXPECT_FALSE(rules.Matches(GURL("http://google.com:40")));
  EXPECT_TRUE(rules.Matches(GURL("http://bar.foobar.com:30")));
  EXPECT_FALSE(rules.Matches(GURL("http://bar.foobar.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://bar.foobar.com:33")));
}

TEST(ProxyHostMatchingRulesTest, BadInputs) {
  ProxyHostMatchingRules rules;
  EXPECT_FALSE(rules.AddRuleFromString("://"));
  EXPECT_FALSE(rules.AddRuleFromString("  "));
  EXPECT_FALSE(rules.AddRuleFromString("http://"));
  EXPECT_FALSE(rules.AddRuleFromString("*.foo.com:-34"));
  EXPECT_EQ(0u, rules.rules().size());
}

TEST(ProxyHostMatchingRulesTest, Equals) {
  ProxyHostMatchingRules rules1;
  ProxyHostMatchingRules rules2;

  rules1.ParseFromString("foo1.com, .foo2.com");
  rules2.ParseFromString("foo1.com,.FOo2.com");

  EXPECT_EQ(rules1, rules2);
  EXPECT_EQ(rules2, rules1);

  rules1.ParseFromString(".foo2.com");
  rules2.ParseFromString("foo1.com,.FOo2.com");

  EXPECT_FALSE(rules1 == rules2);
  EXPECT_FALSE(rules2 == rules1);
}

TEST(ProxyHostMatchingRulesTest, MatchSimpleHostnames) {
  // Test the simple hostnames rule in isolation, by first removing the
  // implicit rules.
  ProxyHostMatchingRules rules;
  rules.ParseFromString("<-loopback>; <local>");

  ASSERT_EQ(2u, rules.rules().size());
  EXPECT_EQ("<-loopback>", rules.rules()[0]->ToString());
  EXPECT_EQ("<local>", rules.rules()[1]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://example/")));

  EXPECT_FALSE(rules.Matches(GURL("http://example./")));
  EXPECT_FALSE(rules.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(rules.Matches(GURL("http://[dead::beef]/")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.168.1.1/")));

  // Confusingly, <local> rule is NOT about localhost names.
  ExpectMatchLocalhost(rules, false, {"localhost", "loopback"});

  // Should NOT match link-local addresses.
  ExpectMatchesLinkLocal(rules, false);

  // Should not match other names either (except for the ones with no dot).
  ExpectMatchesMisc(rules, false, {"foo", "loopback"});
}

TEST(ProxyHostMatchingRulesTest, ParseAndMatchCIDR_IPv4) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("192.168.1.1/16");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("192.168.1.1/16", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://192.168.1.1")));
  EXPECT_TRUE(rules.Matches(GURL("ftp://192.168.4.4")));
  EXPECT_TRUE(rules.Matches(GURL("https://192.168.0.0:81")));
  // Test that an IPv4 mapped IPv6 literal matches an IPv4 CIDR rule.
  EXPECT_TRUE(rules.Matches(GURL("http://[::ffff:192.168.11.11]")));

  EXPECT_FALSE(rules.Matches(GURL("http://foobar.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.169.1.1")));
  EXPECT_FALSE(rules.Matches(GURL("http://xxx.192.168.1.1")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.168.1.1.xx")));
}

TEST(ProxyHostMatchingRulesTest, ParseAndMatchCIDR_IPv6) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("a:b:c:d::/48");
  ASSERT_EQ(1u, rules.rules().size());
  EXPECT_EQ("a:b:c:d::/48", rules.rules()[0]->ToString());

  EXPECT_TRUE(rules.Matches(GURL("http://[A:b:C:9::]")));
  EXPECT_FALSE(rules.Matches(GURL("http://foobar.com")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.169.1.1")));

  // Test that an IPv4 literal matches an IPv4 mapped IPv6 CIDR rule.
  // This is the IPv4 mapped equivalent to 192.168.1.1/16.
  rules.ParseFromString("::ffff:192.168.1.1/112");
  EXPECT_TRUE(rules.Matches(GURL("http://[::ffff:192.168.1.3]")));
  EXPECT_TRUE(rules.Matches(GURL("http://192.168.11.11")));
  EXPECT_FALSE(rules.Matches(GURL("http://10.10.1.1")));

  // Test using an IP range that is close to IPv4 mapped, but not
  // quite. Should not result in matches.
  rules.ParseFromString("::fffe:192.168.1.1/112");
  EXPECT_TRUE(rules.Matches(GURL("http://[::fffe:192.168.1.3]")));
  EXPECT_FALSE(rules.Matches(GURL("http://[::ffff:192.168.1.3]")));
  EXPECT_FALSE(rules.Matches(GURL("http://192.168.11.11")));
  EXPECT_FALSE(rules.Matches(GURL("http://10.10.1.1")));
}

// Test that parsing an IPv6 range given a bracketed literal is not supported.
// Whether IPv6 literals need to be bracketed or not is pretty much a coin toss
// depending on the context, and here it is expected to be unbracketed to match
// macOS. It would be fine to support bracketed too, however none of the
// grammars we parse need that.
TEST(ProxyHostMatchingRulesTest, ParseBracketedIPv6Range) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("[a:b:c:d::]/48");
  ASSERT_EQ(0u, rules.rules().size());
}

// Check which URLs an empty ProxyHostMatchingRules matches.
TEST(ProxyHostMatchingRulesTest, DefaultImplicitRules) {
  ProxyHostMatchingRules rules;

  EXPECT_EQ("", rules.ToString());

  // Should match all localhost and loopback names.
  ExpectMatchLocalhost(rules, true);

  // Should match all link-local addresses.
  ExpectMatchesLinkLocal(rules, true);

  // Should not match other names.
  ExpectMatchesMisc(rules, false);
}

// Test use of the <-loopback> match rule.
TEST(ProxyHostMatchingRulesTest, NegativeWinLoopback) {
  ProxyHostMatchingRules rules;

  rules.ParseFromString("www.example.com;<-loopback>");
  ASSERT_EQ(2u, rules.rules().size());
  EXPECT_EQ("www.example.com", rules.rules()[0]->ToString());
  EXPECT_EQ("<-loopback>", rules.rules()[1]->ToString());

  // Should NOT match localhost and loopback names.
  ExpectMatchLocalhost(rules, false);

  // Should NOT match link-local addresses.
  ExpectMatchesLinkLocal(rules, false);

  // Should not match other names either.
  ExpectMatchesMisc(rules, false);

  // Only www.example.com should be matched.
  EXPECT_TRUE(rules.Matches(GURL("http://www.example.com/")));
}

// Verifies the evaluation order of mixing negative and positive rules. This
// expectation comes from WinInet (which is where <-loopback> comes from).
TEST(ProxyHostMatchingRulesTest, RemoveImplicitAndAddLocalhost) {
  ProxyHostMatchingRules rules;

  rules.ParseFromString("<-loopback>; localhost");
  ASSERT_EQ(2u, rules.rules().size());
  EXPECT_EQ("<-loopback>", rules.rules()[0]->ToString());
  EXPECT_EQ("localhost", rules.rules()[1]->ToString());

  // Should not match localhost names because of <-loopback>. Except for
  // "localhost" which was added at the end.
  ExpectMatchLocalhost(rules, false, {"localhost"});

  // Should NOT match link-local addresses.
  ExpectMatchesLinkLocal(rules, false);

  // Should not match other names either.
  ExpectMatchesMisc(rules, false);
}

// Verifies the evaluation order of mixing negative and positive rules. This
// expectation comes from WinInet (which is where <-loopback> comes from).
TEST(ProxyHostMatchingRulesTest, AddLocalhostThenRemoveImplicit) {
  ProxyHostMatchingRules rules;

  rules.ParseFromString("localhost; <-loopback>");
  ASSERT_EQ(2u, rules.rules().size());
  EXPECT_EQ("localhost", rules.rules()[0]->ToString());
  EXPECT_EQ("<-loopback>", rules.rules()[1]->ToString());

  // Because of the ordering, localhost is not matched, because <-loopback>
  // "unmatches" it.
  ExpectMatchLocalhost(rules, false);

  // Should NOT match link-local addresses.
  ExpectMatchesLinkLocal(rules, false);

  // Should not match other names either.
  ExpectMatchesMisc(rules, false);
}

TEST(ProxyHostMatchingRulesTest, AddRulesToSubtractImplicit) {
  ProxyHostMatchingRules rules;
  rules.ParseFromString("foo");

  rules.AddRulesToSubtractImplicit();

  ASSERT_EQ(2u, rules.rules().size());
  EXPECT_EQ("foo", rules.rules()[0]->ToString());
  EXPECT_EQ("<-loopback>", rules.rules()[1]->ToString());
}

TEST(ProxyHostMatchingRulesTest, GetRulesToSubtractImplicit) {
  EXPECT_EQ("<-loopback>;",
            ProxyHostMatchingRules::GetRulesToSubtractImplicit());
}

// Verifies that the <local> and <-loopback> rules can be specified in any
// case. This matches how WinInet's parses them.
TEST(ProxyHostMatchingRulesTest, LoopbackAndLocalCaseInsensitive) {
  ProxyHostMatchingRules rules;

  rules.ParseFromString("<Local>; <-LoopBacK>; <LoCaL>; <-LoOpBack>");
  ASSERT_EQ(4u, rules.rules().size());
  EXPECT_EQ("<local>", rules.rules()[0]->ToString());
  EXPECT_EQ("<-loopback>", rules.rules()[1]->ToString());
  EXPECT_EQ("<local>", rules.rules()[2]->ToString());
  EXPECT_EQ("<-loopback>", rules.rules()[3]->ToString());
}

}  // namespace

}  // namespace net
