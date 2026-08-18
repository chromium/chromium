// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/public_suffix_util.h"

#include <optional>
#include <string>

#include "net/base/registry_controlled_domains/effective_tld_names_unittest1-reversed-inc.cc"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::api::public_suffix {

namespace {

// Note: See the Public Suffix List effective_tld_names_unittest1.gperf.

ParsedHostname ParseValidHostname(const std::string& hostname) {
  std::optional<ParsedHostname> parsed_hostname = ParseHostname(hostname);
  if (!parsed_hostname.has_value()) {
    ADD_FAILURE() << "Failed to parse hostname: " << hostname;
    return ParsedHostname("ignored");
  }
  return parsed_hostname.value();
}

std::optional<std::string> GetKnownSuffix(const std::string& hostname) {
  return GetKnownSuffix(ParseValidHostname(hostname));
}

bool IsKnownSuffix(const std::string& hostname) {
  return IsKnownSuffix(ParseValidHostname(hostname));
}

std::optional<std::string> GetDomain(const std::string& hostname,
                                     const DomainOptions& options) {
  return GetDomain(ParseValidHostname(hostname), options);
}

bool IsInvalidHostname(const std::string& hostname) {
  return !ParseHostname(hostname).has_value();
}

}  // namespace

class PublicSuffixUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    net::registry_controlled_domains::SetFindDomainGraphForTesting(
        net::test1::kDafsa);
  }

  void TearDown() override {
    net::registry_controlled_domains::ResetFindDomainGraphForTesting();
  }
};

TEST_F(PublicSuffixUtilTest, KnownSuffix) {
  EXPECT_EQ(GetKnownSuffix("jp"), "jp");
  EXPECT_EQ(GetKnownSuffix("foo.jp"), "jp");
  EXPECT_EQ(GetKnownSuffix("foo.priv.no"), "priv.no");
  EXPECT_EQ(GetKnownSuffix("foo.bar.baz.com"), "bar.baz.com");
  EXPECT_EQ(GetKnownSuffix("foo.blah.bar.jp"), "blah.bar.jp");
  EXPECT_EQ(GetKnownSuffix("192.168.2.1"), std::nullopt);
  EXPECT_EQ(GetKnownSuffix("green.banana"), std::nullopt);

  EXPECT_EQ(GetKnownSuffix(".jp"), "jp");
  EXPECT_EQ(GetKnownSuffix(".bar.baz.com"), "bar.baz.com");

  EXPECT_EQ(IsKnownSuffix("jp"), true);
  EXPECT_EQ(IsKnownSuffix("foo.jp"), false);
  EXPECT_EQ(IsKnownSuffix("priv.no"), true);
  EXPECT_EQ(IsKnownSuffix("foo.priv.no"), false);
  EXPECT_EQ(IsKnownSuffix("blah.bar.jp"), true);
  EXPECT_EQ(IsKnownSuffix("foo.blah.bar.jp"), false);
  EXPECT_EQ(IsKnownSuffix("192.168.2.1"), false);
  EXPECT_EQ(IsKnownSuffix("green.banana"), false);

  EXPECT_EQ(GetKnownSuffix("foo.中国"), "xn--fiqs8s");
  EXPECT_EQ(GetKnownSuffix("foo.xn--fiqs8s"), "xn--fiqs8s");

  EXPECT_EQ(GetKnownSuffix("foo.private"), "private");
  EXPECT_EQ(IsKnownSuffix("private"), true);

  // Wildcard rules - The "*.c" rule means that "foo.c" is a known suffix.
  EXPECT_EQ(IsKnownSuffix("c"), true);
  EXPECT_EQ(IsKnownSuffix("foo.c"), true);
  EXPECT_EQ(GetKnownSuffix("sub.foo.c"), "foo.c");

  // Wildcard exception rules - there's an exception for "the b.c".
  EXPECT_EQ(IsKnownSuffix("b.c"), false);
  EXPECT_EQ(GetKnownSuffix("b.c"), "c");

  // Unlike a leading dot, a trailing dot should not be trimmed.
  EXPECT_EQ(GetKnownSuffix("jp."), "jp.");
  EXPECT_EQ(GetKnownSuffix("example.jp."), "jp.");
  EXPECT_EQ(IsKnownSuffix("jp."), true);
  EXPECT_EQ(IsKnownSuffix("example.jp."), false);
}

TEST_F(PublicSuffixUtilTest, GetDomain) {
  EXPECT_EQ(GetDomain("sub.domain.jp", DomainOptions()), "domain.jp");
  EXPECT_EQ(GetDomain("a.b.bar.baz.com", DomainOptions()), "b.bar.baz.com");
  EXPECT_EQ(GetDomain("EXAMPLE.JP", DomainOptions()), "example.jp");
  EXPECT_EQ(GetDomain(".example.jp", DomainOptions()), "example.jp");
  EXPECT_EQ(GetDomain("example.jp.", DomainOptions()), "example.jp.");

  // Wildcard rules.
  EXPECT_EQ(GetDomain("blah.bar.jp", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("sub.blah.bar.jp", DomainOptions()), "sub.blah.bar.jp");
  EXPECT_EQ(GetDomain("sub.sub.blah.bar.jp", DomainOptions()),
            "sub.blah.bar.jp");

  // Wildcard exception rules.
  EXPECT_EQ(GetDomain("pref.bar.jp", DomainOptions()), "pref.bar.jp");
  EXPECT_EQ(GetDomain("sub.pref.bar.jp", DomainOptions()), "pref.bar.jp");
  EXPECT_EQ(GetDomain("sub.sub.pref.bar.jp", DomainOptions()), "pref.bar.jp");

  // Private suffix rules.
  EXPECT_EQ(GetDomain("foo.priv.no", DomainOptions()), "foo.priv.no");
  EXPECT_EQ(GetDomain("sub.foo.priv.no", DomainOptions()), "foo.priv.no");
}

TEST_F(PublicSuffixUtilTest, GetDomainAllowUnknownSuffix) {
  EXPECT_EQ(GetDomain("green.banana", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("very.green.banana", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("printer.homenet", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("localhost", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("apple.pear.banana.", DomainOptions()), std::nullopt);

  DomainOptions allow_unknown;
  allow_unknown.allow_unknown_suffix = true;
  EXPECT_EQ(GetDomain("green.banana", allow_unknown), "green.banana");
  EXPECT_EQ(GetDomain("very.green.banana", allow_unknown), "green.banana");
  EXPECT_EQ(GetDomain("printer.homenet", allow_unknown), "printer.homenet");
  EXPECT_EQ(GetDomain("localhost", allow_unknown), "localhost");
  EXPECT_EQ(GetDomain("apple.pear.banana.", allow_unknown), "pear.banana.");
}

TEST_F(PublicSuffixUtilTest, GetDomainAllowPlainSuffix) {
  EXPECT_EQ(GetDomain("jp", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain(".jp", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("priv.no", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("bar.baz.com", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("blah.bar.jp", DomainOptions()), std::nullopt);

  DomainOptions allow_plain;
  allow_plain.allow_plain_suffix = true;
  EXPECT_EQ(GetDomain("jp", allow_plain), "jp");
  EXPECT_EQ(GetDomain("priv.no", allow_plain), "priv.no");
  EXPECT_EQ(GetDomain("bar.baz.com", allow_plain), "bar.baz.com");
  EXPECT_EQ(GetDomain("blah.bar.jp", allow_plain), "blah.bar.jp");
  EXPECT_EQ(GetDomain(".jp", allow_plain), "jp");
}

TEST_F(PublicSuffixUtilTest, GetDomainAllowIPAddress) {
  EXPECT_EQ(GetDomain("192.168.2.1", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("[::1]", DomainOptions()), std::nullopt);

  DomainOptions allow_ip;
  allow_ip.allow_ip_address = true;
  EXPECT_EQ(GetDomain("192.168.2.1", allow_ip), "192.168.2.1");
  EXPECT_EQ(GetDomain("[::1]", allow_ip), "[::1]");
}

TEST_F(PublicSuffixUtilTest, GetDomainDisplayEncoding) {
  EXPECT_EQ(GetDomain("中国", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("xn--fiqs8s", DomainOptions()), std::nullopt);
  EXPECT_EQ(GetDomain("foo.中国", DomainOptions()), "foo.xn--fiqs8s");
  EXPECT_EQ(GetDomain("foo.xn--fiqs8s", DomainOptions()), "foo.xn--fiqs8s");
  EXPECT_EQ(GetDomain("xn--bs-red.bar.baz.com", DomainOptions()),
            "xn--bs-red.bar.baz.com");

  DomainOptions display;
  display.encoding = DomainEncoding::kDisplay;
  EXPECT_EQ(GetDomain("foo.中国", display), "foo.中国");
  EXPECT_EQ(GetDomain("foo.xn--fiqs8s", display), "foo.中国");
  // Unicode confusable, display as punycode.
  EXPECT_EQ(GetDomain("xn--bs-red.bar.baz.com", display),
            "xn--bs-red.bar.baz.com");
}

TEST_F(PublicSuffixUtilTest, InvalidHostnames) {
  EXPECT_TRUE(IsInvalidHostname(""));
  EXPECT_TRUE(IsInvalidHostname("."));
  EXPECT_TRUE(IsInvalidHostname("website.example/path"));
  EXPECT_TRUE(IsInvalidHostname("website.example:443"));
  EXPECT_TRUE(IsInvalidHostname("user@website.example"));
  EXPECT_TRUE(IsInvalidHostname("https://website.example"));
  EXPECT_TRUE(IsInvalidHostname("website..example"));
  // "。" normalizes to ".", so this must still reject the empty label.
  EXPECT_TRUE(IsInvalidHostname(".。website.example"));
  EXPECT_TRUE(IsInvalidHostname("website.example.."));
  EXPECT_TRUE(IsInvalidHostname("*.com"));
  EXPECT_TRUE(IsInvalidHostname("::1"));
  // A single leading dot is accepted for hostnames, but not for IP addresses.
  EXPECT_TRUE(IsInvalidHostname(".192.168.2.1"));
  EXPECT_TRUE(IsInvalidHostname(".[::1]"));
}

TEST_F(PublicSuffixUtilTest, EdgeCases) {
  EXPECT_TRUE(IsInvalidHostname("sub%2edomain.jp"));
  EXPECT_FALSE(IsInvalidHostname("foo_bar.jp"));
  EXPECT_FALSE(IsInvalidHostname("example。jp"));
}

}  // namespace extensions::api::public_suffix
