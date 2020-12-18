// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/is_potentially_trustworthy.h"

#include "base/test/scoped_command_line.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace network {

bool IsOriginAllowlisted(const url::Origin& origin) {
  return SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(origin);
}

bool IsOriginAllowlisted(const char* str) {
  return IsOriginAllowlisted(url::Origin::Create(GURL(str)));
}

bool IsOriginPotentiallyTrustworthy(const char* str) {
  return IsOriginPotentiallyTrustworthy(url::Origin::Create(GURL(str)));
}

bool IsUrlPotentiallyTrustworthy(const char* str) {
  return IsUrlPotentiallyTrustworthy(GURL(str));
}

std::vector<std::string> CanonicalizeAllowlist(
    const std::vector<std::string>& allowlist,
    std::vector<std::string>* rejected_patterns) {
  return SecureOriginAllowlist::CanonicalizeAllowlistForTesting(
      allowlist, rejected_patterns);
}

TEST(IsPotentiallyTrustworthy, Origin) {
  const url::Origin unique_origin;
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy(unique_origin));
  const url::Origin opaque_origin =
      url::Origin::Create(GURL("https://www.example.com"))
          .DeriveNewOpaqueOrigin();
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy(opaque_origin));

  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("about:blank"));
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("about:srcdoc"));
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("javascript:alert('blah')"));
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("data:test/plain;blah"));

  EXPECT_FALSE(IsOriginPotentiallyTrustworthy("custom-scheme://example.com"));
  EXPECT_TRUE(
      IsOriginPotentiallyTrustworthy("quic-transport://example.com/counter"));
}

TEST(IsPotentiallyTrustworthy, Url) {
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank?x=2"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank?x=2#ref"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:srcdoc"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:srcdoc?x=2"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:srcdoc#ref"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:srcdoc?x=2#ref"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("about:about"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("data:test/plain;blah"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("javascript:alert('blah')"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file://localhost/test/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file://otherhost/test/"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://example.com/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("wss://example.com/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("ws://example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://localhost/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://localhost./fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://crumpet.pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://pumpkin.localhost:8080/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost:3000/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://localhost.com/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://localhost.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("ftp://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.3.0.1/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://127.example.com/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://127.example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://[::1]/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://[::2]/fun.html"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("http://[::1].example.com/fun.html"));

  // IPv4 mapped IPv6 literals for loopback.
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://[::ffff:127.0.0.1]/"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://[::ffff:7f00:1]"));

  // IPv4 compatible IPv6 literal for loopback.
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://[::127.0.0.1]"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://loopback"));

  // Legacy localhost names.
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://localhost.localdomain"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://localhost6"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("ftp://localhost6.localdomain6"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "filesystem:http://www.example.com/temporary/"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "filesystem:ftp://www.example.com/temporary/"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("filesystem:ftp://127.0.0.1/temporary/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "filesystem:https://www.example.com/temporary/"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "blob:http://www.example.com/guid-goes-here"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("blob:ftp://www.example.com/guid-goes-here"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("blob:ftp://127.0.0.1/guid-goes-here"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "blob:https://www.example.com/guid-goes-here"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("blob:data:text/html,Hello"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("blob:about:blank"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("filesystem:data:text/html,Hello"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("filesystem:about:blank"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "blob:blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("filesystem:blob:https://example.com/"
                                  "578223a1-8c13-17b3-84d5-eca045ae384a"));

  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("quic-transport://example.com/counter"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("custom-scheme://example.com"));
}

TEST(IsPotentiallyTrustworthy, CustomScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddSecureScheme("custom-scheme");

  // TODO(crbug.com/1159371): These tests should return true.
  EXPECT_FALSE(IsOriginPotentiallyTrustworthy(
      "custom-scheme://578223a1-8c13-17b3-84d5-eca045ae384a/fun.js"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "custom-scheme://578223a1-8c13-17b3-84d5-eca045ae384a/fun.js"));
}

// Tests that were for the removed blink::network_utils::IsOriginSecure.
// TODO(https://crbug.com/1153336): Merge with IsPotentiallyTrustworthy.Url?
TEST(IsPotentiallyTrustworthy, LegacyOriginUtilTests) {
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("file:///test/"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://example.com/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("wss://example.com/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("ws://example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://localhost/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://crumpet.pumpkin.localhost/fun.html"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("http://pumpkin.localhost:8080/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "http://crumpet.pumpkin.localhost:3000/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://localhost.com/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://localhost.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("ftp://127.0.0.1/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://127.3.0.1/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://127.example.com/fun.html"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("https://127.example.com/fun.html"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("http://[::1]/fun.html"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("http://[::2]/fun.html"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("http://[::1].example.com/fun.html"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "filesystem:http://www.example.com/temporary/"));
  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "filesystem:ftp://www.example.com/temporary/"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("filesystem:ftp://127.0.0.1/temporary/"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "filesystem:https://www.example.com/temporary/"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:blank#ref"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("about:srcdoc"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy("javascript:alert('blah')"));

  EXPECT_TRUE(IsUrlPotentiallyTrustworthy("data:test/plain;blah"));

  EXPECT_FALSE(IsUrlPotentiallyTrustworthy(
      "blob:http://www.example.com/guid-goes-here"));
  EXPECT_FALSE(
      IsUrlPotentiallyTrustworthy("blob:ftp://www.example.com/guid-goes-here"));
  EXPECT_TRUE(
      IsUrlPotentiallyTrustworthy("blob:ftp://127.0.0.1/guid-goes-here"));
  EXPECT_TRUE(IsUrlPotentiallyTrustworthy(
      "blob:https://www.example.com/guid-goes-here"));
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

}  // namespace network
