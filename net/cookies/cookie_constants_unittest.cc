// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
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

// TODO(crbug.com/996811): Add tests for multiple possibly-invalid attributes.
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

}  // namespace net
