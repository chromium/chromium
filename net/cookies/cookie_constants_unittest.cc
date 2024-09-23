// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CookieConstantsTest, TestCookiePriority) {
  // Basic cases.
  EXPECT_EQ("low", CookiePriorityToString(COOKIE_PRIORITY_LOW));
  EXPECT_EQ("medium", CookiePriorityToString(COOKIE_PRIORITY_MEDIUM));
  EXPECT_EQ("high", CookiePriorityToString(COOKIE_PRIORITY_HIGH));

  EXPECT_EQ(COOKIE_PRIORITY_LOW, StringToCookiePriority("low"));
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, StringToCookiePriority("medium"));
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, StringToCookiePriority("high"));

  // Case Insensitivity of StringToCookiePriority().
  EXPECT_EQ(COOKIE_PRIORITY_LOW, StringToCookiePriority("LOW"));
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, StringToCookiePriority("Medium"));
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, StringToCookiePriority("hiGH"));

  // Value of default priority.
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, COOKIE_PRIORITY_MEDIUM);

  // Numeric values.
  EXPECT_LT(COOKIE_PRIORITY_LOW, COOKIE_PRIORITY_MEDIUM);
  EXPECT_LT(COOKIE_PRIORITY_MEDIUM, COOKIE_PRIORITY_HIGH);

  // Unrecognized tokens are interpreted as COOKIE_PRIORITY_DEFAULT.
  const char* const bad_tokens[] = {
    "", "lo", "lowerest", "high ", " high", "0"};
  for (const auto* bad_token : bad_tokens) {
    EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, StringToCookiePriority(bad_token));
  }
}

// TODO(crbug.com/40641705): Add tests for multiple possibly-invalid attributes.
TEST(CookieConstantsTest, TestCookieSameSite) {
  // Test case insensitivity
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, StringToCookieSameSite("None"));
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, StringToCookieSameSite("none"));
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, StringToCookieSameSite("NONE"));
  EXPECT_EQ(CookieSameSite::LAX_MODE, StringToCookieSameSite("Lax"));
  EXPECT_EQ(CookieSameSite::LAX_MODE, StringToCookieSameSite("LAX"));
  EXPECT_EQ(CookieSameSite::LAX_MODE, StringToCookieSameSite("lAx"));
  EXPECT_EQ(CookieSameSite::STRICT_MODE, StringToCookieSameSite("Strict"));
  EXPECT_EQ(CookieSameSite::STRICT_MODE, StringToCookieSameSite("STRICT"));
  EXPECT_EQ(CookieSameSite::STRICT_MODE, StringToCookieSameSite("sTrIcT"));
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, StringToCookieSameSite("extended"));
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, StringToCookieSameSite("EXTENDED"));
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, StringToCookieSameSite("ExtenDED"));

  // Unrecognized tokens are interpreted as UNSPECIFIED.
  const char* const bad_tokens[] = {"",          "foo",   "none ",
                                    "strictest", " none", "0"};
  for (const auto* bad_token : bad_tokens) {
    EXPECT_EQ(CookieSameSite::UNSPECIFIED, StringToCookieSameSite(bad_token));
  }
}

TEST(CookieConstantsTest, TestReducePortRangeForCookieHistogram) {
  struct TestData {
    int input_port;
    CookiePort expected_enum;
  };

  const TestData kTestValues[] = {
      {-1234 /* Invalid port. */, CookiePort::kOther},
      {0 /* Invalid port. */, CookiePort::kOther},
      {1 /* Valid but outside range. */, CookiePort::kOther},
      {79 /* Valid but outside range. */, CookiePort::kOther},
      {80, CookiePort::k80},
      {445, CookiePort::k445},
      {3001, CookiePort::k3001},
      {4200, CookiePort::k4200},
      {5002, CookiePort::k5002},
      {7003, CookiePort::k7003},
      {8001, CookiePort::k8001},
      {8080, CookiePort::k8080},
      {8086 /* Valid but outside range. */, CookiePort::kOther},
      {8095, CookiePort::k8095},
      {8100, CookiePort::k8100},
      {8201, CookiePort::k8201},
      {8445, CookiePort::k8445},
      {8888, CookiePort::k8888},
      {9004, CookiePort::k9004},
      {9091, CookiePort::k9091},
      {65535 /* Valid but outside range. */, CookiePort::kOther},
      {655356 /* Invalid port. */, CookiePort::kOther},
  };

  for (const auto& value : kTestValues) {
    EXPECT_EQ(value.expected_enum,
              ReducePortRangeForCookieHistogram(value.input_port));
  }
}

}  // namespace net
