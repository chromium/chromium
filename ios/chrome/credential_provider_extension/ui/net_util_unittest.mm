// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/net_util.h"

#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace credential_provider_extension {

using NetUtilTest = PlatformTest;

TEST_F(NetUtilTest, GetDomainAndRegistry) {
  auto GetDomainAndRegistry = [](std::string_view host) {
    return net::registry_controlled_domains::GetDomainAndRegistry(
        host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  };

  // ICANN Registry TLD matches.
  EXPECT_EQ(GetDomainAndRegistry("example.com"), "example.com");
  EXPECT_EQ(GetDomainAndRegistry("login.example.com"), "example.com");
  EXPECT_EQ(GetDomainAndRegistry("auth.login.example.com"), "example.com");
  EXPECT_EQ(GetDomainAndRegistry("example.co.uk"), "example.co.uk");
  EXPECT_EQ(GetDomainAndRegistry("sub.example.co.uk"), "example.co.uk");

  // Private Registry Suffix matches.
  EXPECT_EQ(GetDomainAndRegistry("railway.app"), "railway.app");
  EXPECT_EQ(GetDomainAndRegistry("login.railway.app"), "railway.app");
  EXPECT_EQ(GetDomainAndRegistry("attacker.up.railway.app"),
            "attacker.up.railway.app");

  // Unknown Registry Suffix fallback rules.
  EXPECT_EQ(GetDomainAndRegistry("foo.bar.invalid"), "bar.invalid");
  EXPECT_EQ(GetDomainAndRegistry("invalid"), "");

  // Flat and Invalid Registry values.
  EXPECT_EQ(GetDomainAndRegistry("com"), "");
  EXPECT_EQ(GetDomainAndRegistry("co.uk"), "");
  EXPECT_EQ(GetDomainAndRegistry("up.railway.app"), "");
  EXPECT_EQ(GetDomainAndRegistry("192.168.1.1"), "");
  EXPECT_EQ(GetDomainAndRegistry("2001:db8::1"), "");
  EXPECT_EQ(GetDomainAndRegistry("[::1]"), "");
  EXPECT_EQ(GetDomainAndRegistry(""), "");
}

TEST_F(NetUtilTest, SecureHostsMatch) {
  // Exact match.
  EXPECT_TRUE(SecureHostsMatch(@"example.com", @"example.com"));
  EXPECT_TRUE(SecureHostsMatch(@"login.example.com", @"login.example.com"));

  // RP ID / credential host is a registrable suffix of the origin's domain.
  EXPECT_TRUE(SecureHostsMatch(@"login.example.com", @"example.com"));
  EXPECT_TRUE(SecureHostsMatch(@"auth.login.example.com", @"example.com"));
  EXPECT_TRUE(
      SecureHostsMatch(@"auth.login.example.com", @"login.example.com"));
  EXPECT_TRUE(SecureHostsMatch(@"login.railway.app", @"railway.app"));

  // Same eTLD+1 but not a suffix (e.g., origin a.foo.com claiming RP ID
  // b.foo.com).
  EXPECT_FALSE(SecureHostsMatch(@"a.foo.com", @"b.foo.com"));
  EXPECT_FALSE(SecureHostsMatch(@"login1.example.com", @"login2.example.com"));

  // Credential host is longer than requested host.
  EXPECT_FALSE(SecureHostsMatch(@"example.com", @"login.example.com"));
  EXPECT_FALSE(SecureHostsMatch(@"railway.app", @"login.railway.app"));

  // Suffix Match boundary hijacking checks (should REJECT!).
  EXPECT_FALSE(SecureHostsMatch(@"attacker.up.railway.app", @"railway.app"));
  EXPECT_FALSE(SecureHostsMatch(@"evil-railway.app", @"railway.app"));
  EXPECT_FALSE(SecureHostsMatch(@"railway.app.evil.com", @"railway.app"));

  // TLD / non-registrable suffix.
  EXPECT_FALSE(SecureHostsMatch(@"login.example.com", @"com"));
}

}  // namespace credential_provider_extension
