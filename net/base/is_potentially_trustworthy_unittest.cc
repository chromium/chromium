// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/is_potentially_trustworthy.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "net/base/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

bool TestIsOriginAllowlisted(const url::Origin& origin) {
  return SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(origin);
}

bool TestIsOriginAllowlisted(const char* str) {
  return TestIsOriginAllowlisted(url::Origin::Create(GURL(str)));
}

bool TestIsUrlPotentiallyTrustworthy(const char* str) {
  return IsUrlPotentiallyTrustworthy(GURL(str));
}

bool TestIsOriginPotentiallyTrustworthy(const char* str) {
  return IsOriginPotentiallyTrustworthy(url::Origin::Create(GURL(str)));
}

class NetSecureOriginAllowlistTest : public testing::Test {
  void TearDown() override {
    SecureOriginAllowlist::GetInstance().ResetForTesting();
  }
};

TEST_F(NetSecureOriginAllowlistTest, BasicTrustworthiness) {
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("https://example.com/"));
  EXPECT_TRUE(TestIsOriginPotentiallyTrustworthy("https://example.com/"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("wss://example.com/"));

  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://localhost/"));
  EXPECT_TRUE(TestIsOriginPotentiallyTrustworthy("http://localhost/"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://localhost:8000/report"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://127.0.0.1/"));
  EXPECT_TRUE(TestIsOriginPotentiallyTrustworthy("http://127.0.0.1/"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://127.0.0.1:8000/report"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://[::1]/"));
  EXPECT_TRUE(TestIsOriginPotentiallyTrustworthy("http://[::1]/"));

  EXPECT_FALSE(TestIsUrlPotentiallyTrustworthy("http://example.com/"));
  EXPECT_FALSE(TestIsOriginPotentiallyTrustworthy("http://example.com/"));
  EXPECT_FALSE(TestIsUrlPotentiallyTrustworthy("http://insecure.test/"));
}

TEST_F(NetSecureOriginAllowlistTest, UnsafelyTreatInsecureOriginAsSecure) {
  EXPECT_FALSE(TestIsOriginAllowlisted("http://example.com/a.html"));
  EXPECT_FALSE(TestIsUrlPotentiallyTrustworthy("http://example.com/a.html"));

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://example.com,http://127.example.com");
  SecureOriginAllowlist::GetInstance().ResetForTesting();

  EXPECT_TRUE(TestIsOriginAllowlisted("http://example.com/a.html"));
  EXPECT_TRUE(TestIsOriginAllowlisted("http://127.example.com/a.html"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://example.com/a.html"));
  EXPECT_TRUE(TestIsUrlPotentiallyTrustworthy("http://127.example.com/a.html"));
}

}  // namespace
}  // namespace net
