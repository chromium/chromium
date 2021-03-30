// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/is_potentially_trustworthy_unittest.h"

#include "base/test/scoped_command_line.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace network {
namespace test {

bool IsOriginAllowlisted(const url::Origin& origin) {
  return SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(origin);
}

bool IsOriginAllowlisted(const char* str) {
  return IsOriginAllowlisted(url::Origin::Create(GURL(str)));
}

bool IsUrlPotentiallyTrustworthy(const char* str) {
  return network::IsUrlPotentiallyTrustworthy(GURL(str));
}

std::vector<std::string> CanonicalizeAllowlist(
    const std::vector<std::string>& allowlist,
    std::vector<std::string>* rejected_patterns) {
  return SecureOriginAllowlist::CanonicalizeAllowlistForTesting(
      allowlist, rejected_patterns);
}

// TODO(crbug.com/1153336 and crbug.com/1164416): Fix product behavior, so that
// blink::SecurityOrigin::IsSecure(const KURL&) is compatible with
// network::IsUrlPotentiallyTrustworthy(const GURL&) and then move the tests
// below to the AbstractTrustworthinessTest.UrlFromString test case in
// //services/network/public/cpp/is_potentially_trustworthy_unittest.h
// See also SecurityOriginTest.IsSecure test.
TEST(IsPotentiallyTrustworthy, Url) {
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file://localhost/test/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file://otherhost/test/"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://localhost/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://localhost./fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://crumpet.pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://pumpkin.localhost:8080/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost:3000/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("ftp://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.3.0.1/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://[::1]/fun.html"));

  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("filesystem:ftp://127.0.0.1/temporary/"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("blob:ftp://127.0.0.1/guid-goes-here"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("blob:data:text/html,Hello"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("blob:about:blank"));
}

class SecureOriginAllowlistTest : public testing::Test {
  void TearDown() override {
    // Ensure that we reset the allowlisted origins without any flags applied.
    SecureOriginAllowlist::GetInstance().ResetForTesting();
  }
};

TEST_F(SecureOriginAllowlistTest, UnsafelyTreatInsecureOriginAsSecure) {
  EXPECT_FALSE(IsOriginAllowlisted("http://example.com/a.html"));
  EXPECT_FALSE(IsOriginAllowlisted("http://127.example.com/a.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://example.com/a.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://127.example.com/a.html"));

  // Add http://example.com and http://127.example.com to allowlist by
  // command-line and see if they are now considered secure origins.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://example.com,http://127.example.com");
  SecureOriginAllowlist::GetInstance().ResetForTesting();

  // They should be now allow-listed.
  EXPECT_TRUE(IsOriginAllowlisted("http://example.com/a.html"));
  EXPECT_TRUE(IsOriginAllowlisted("http://127.example.com/a.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://example.com/a.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.example.com/a.html"));

  // Check that similarly named sites are not considered secure.
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://128.example.com/a.html"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("http://foobar.127.example.com/a.html"));

  // When port is not specified, default port is assumed.
  EXPECT_TRUE(IsOriginAllowlisted("http://example.com:80/a.html"));
  EXPECT_FALSE(IsOriginAllowlisted("http://example.com:8080/a.html"));
}

TEST_F(SecureOriginAllowlistTest, HostnamePatterns) {
  const struct HostnamePatternCase {
    const char* pattern;
    const char* test_input;
    bool expected_secure;
  } kTestCases[] = {
      {"*.foo.com", "http://bar.foo.com", true},
      {"*.foo.*.bar.com", "http://a.foo.b.bar.com:8000", true},
      // For parsing/canonicalization simplicity, wildcard patterns can be
      // hostnames only, not full origins.
      {"http://*.foo.com", "http://bar.foo.com", false},
      {"*://foo.com", "http://foo.com", false},
      // Wildcards must be beyond eTLD+1.
      {"*.co.uk", "http://foo.co.uk", false},
      {"*.co.uk", "http://co.uk", false},
      {"*.baz", "http://foo.baz", false},
      {"foo.*.com", "http://foo.bar.com", false},
      {"*.foo.baz", "http://a.foo.baz", true},
      // Hostname patterns should be canonicalized.
      {"*.FoO.com", "http://a.foo.com", true},
      {"%2A.foo.com", "http://a.foo.com", false},
      // Hostname patterns must contain a wildcard and a wildcard can only
      // replace a component, not a part of a component.
      {"foo.com", "http://foo.com", false},
      {"test*.foo.com", "http://testblah.foo.com", false},
      {"*foo.com", "http://testfoo.com", false},
      {"foo*.com", "http://footest.com", false},
      // With Hostname pattern, all ports are allowed.
      {"*.foo.com", "http://bar.foo.com:80", true},
      {"*.foo.com", "http://bar.foo.com:1234", true},
      // With Hostname pattern, all schemes are allowed.
      {"*.foo.com", "ws://bar.foo.com", true},
      {"*.foo.com", "blob:http://bar.foo.com/guid-goes-here", true},
      // Hostname pattern works on IP addresses, but wildcards must be beyond
      // eTLD+1.
      {"*.20.30.40", "http://10.20.30.40", true},
      {"*.30.40", "http://10.20.30.40", true},
      {"*.40", "http://10.20.30.40", false},
  };

  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    command_line->AppendSwitchASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure, test.pattern);
    SecureOriginAllowlist::GetInstance().ResetForTesting();
    GURL input_url(test.test_input);
    url::Origin input_origin = url::Origin::Create(input_url);
    EXPECT_EQ(test.expected_secure, IsOriginAllowlisted(input_origin));
    EXPECT_EQ(test.expected_secure,
              IsUrlPotentiallyTrustworthy(test.test_input));
  }
}

TEST_F(SecureOriginAllowlistTest, MixOfOriginAndHostnamePatterns) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://example.com,*.foo.com,http://10.20.30.40");
  SecureOriginAllowlist::GetInstance().ResetForTesting();

  EXPECT_TRUE(IsOriginAllowlisted("http://example.com/a.html"));
  EXPECT_TRUE(IsOriginAllowlisted("http://bar.foo.com/b.html"));
  EXPECT_TRUE(IsOriginAllowlisted("http://10.20.30.40/c.html"));
}

TEST_F(SecureOriginAllowlistTest, Canonicalization) {
  std::vector<std::string> canonicalized;
  std::vector<std::string> rejected;

  // Basic test.
  rejected.clear();
  canonicalized = CanonicalizeAllowlist({"*.foo.com"}, &rejected);
  EXPECT_THAT(rejected, ::testing::IsEmpty());
  EXPECT_THAT(canonicalized, ::testing::ElementsAre("*.foo.com"));

  // Okay to pass |nullptr| as a 2nd arg.
  rejected.clear();
  canonicalized = CanonicalizeAllowlist({"null", "*.com"}, nullptr);
  EXPECT_THAT(canonicalized, ::testing::IsEmpty());

  // Opaque origins or invalid urls should be rejected.
  rejected.clear();
  canonicalized = CanonicalizeAllowlist({"null", "invalid"}, &rejected);
  EXPECT_THAT(rejected, ::testing::ElementsAre("null", "invalid"));
  EXPECT_THAT(canonicalized, ::testing::IsEmpty());

  // Wildcard shouldn't appear in eTLD+1.
  rejected.clear();
  canonicalized = CanonicalizeAllowlist({"*.com"}, &rejected);
  EXPECT_THAT(rejected, ::testing::ElementsAre("*.com"));
  EXPECT_THAT(canonicalized, ::testing::IsEmpty());

  // Replacing '*' with a hostname component should form a valid hostname (so,
  // schemes or ports or paths should not be part of a wildcards;  only valid
  // hostname characters are allowed).
  rejected.clear();
  canonicalized = CanonicalizeAllowlist(
      {"*.example.com", "*.example.com:1234", "!@#$%^&---.*.com"}, &rejected);
  EXPECT_THAT(rejected,
              ::testing::ElementsAre("*.example.com:1234", "!@#$%^&---.*.com"));
  EXPECT_THAT(canonicalized, ::testing::ElementsAre("*.example.com"));
}

class TrustworthinessTestTraits : public url::UrlOriginTestTraits {
 public:
  using OriginType = url::Origin;

  static bool IsOriginPotentiallyTrustworthy(const OriginType& origin) {
    return network::IsOriginPotentiallyTrustworthy(origin);
  }
  static bool IsUrlPotentiallyTrustworthy(base::StringPiece str) {
    return network::IsUrlPotentiallyTrustworthy(GURL(str));
  }
  static bool IsOriginOfLocalhost(const OriginType& origin) {
    return net::IsLocalhost(origin.GetURL());
  }

  // Only static members = no constructors are needed.
  TrustworthinessTestTraits() = delete;
};

INSTANTIATE_TYPED_TEST_SUITE_P(UrlOrigin,
                               AbstractTrustworthinessTest,
                               TrustworthinessTestTraits);

}  // namespace test
}  // namespace network
