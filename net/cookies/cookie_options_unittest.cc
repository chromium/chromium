// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(CookieOptionsTest, SameSiteCookieContextType) {
  using ContextType = CookieOptions::SameSiteCookieContext::ContextType;
  EXPECT_EQ("0", ::testing::PrintToString(ContextType::CROSS_SITE));
  EXPECT_EQ("1",
            ::testing::PrintToString(ContextType::SAME_SITE_LAX_METHOD_UNSAFE));
  EXPECT_EQ("2", ::testing::PrintToString(ContextType::SAME_SITE_LAX));
  EXPECT_EQ("3", ::testing::PrintToString(ContextType::SAME_SITE_STRICT));
}

TEST(CookieOptionsTest, SameSiteCookieContext) {
  using SameSiteCookieContext = CookieOptions::SameSiteCookieContext;
  SameSiteCookieContext cross_cross(
      SameSiteCookieContext::ContextType::CROSS_SITE);
  SameSiteCookieContext lax_lax(
      SameSiteCookieContext::ContextType::SAME_SITE_LAX);
  SameSiteCookieContext strict_strict(
      SameSiteCookieContext::ContextType::SAME_SITE_STRICT);
  SameSiteCookieContext strict_cross(
      SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
      SameSiteCookieContext::ContextType::CROSS_SITE);
  SameSiteCookieContext strict_lax(
      SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
      SameSiteCookieContext::ContextType::SAME_SITE_LAX);
  SameSiteCookieContext lax_cross(
      SameSiteCookieContext::ContextType::SAME_SITE_LAX,
      SameSiteCookieContext::ContextType::CROSS_SITE);

  EXPECT_EQ("{ context: 0, schemeful_context: 0 }",
            ::testing::PrintToString(cross_cross));
  EXPECT_EQ("{ context: 2, schemeful_context: 2 }",
            ::testing::PrintToString(lax_lax));
  EXPECT_EQ("{ context: 3, schemeful_context: 3 }",
            ::testing::PrintToString(strict_strict));
  EXPECT_EQ("{ context: 3, schemeful_context: 0 }",
            ::testing::PrintToString(strict_cross));
  EXPECT_EQ("{ context: 3, schemeful_context: 2 }",
            ::testing::PrintToString(strict_lax));
  EXPECT_EQ("{ context: 2, schemeful_context: 0 }",
            ::testing::PrintToString(lax_cross));
}

}  // namespace
}  // namespace net
