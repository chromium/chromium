// Copyright 2021 The Chromium Authors
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

  SameSiteCookieContext::ContextMetadata metadata1;
  metadata1.cross_site_redirect_downgrade = SameSiteCookieContext::
      ContextMetadata::ContextDowngradeType::kStrictToLax;
  metadata1.redirect_type_bug_1221316 = SameSiteCookieContext::ContextMetadata::
      ContextRedirectTypeBug1221316::kPartialSameSiteRedirect;
  metadata1.http_method_bug_1221316 =
      SameSiteCookieContext::ContextMetadata::HttpMethod::kGet;
  SameSiteCookieContext::ContextMetadata metadata2;
  metadata2.cross_site_redirect_downgrade = SameSiteCookieContext::
      ContextMetadata::ContextDowngradeType::kStrictToLax;
  metadata2.redirect_type_bug_1221316 = SameSiteCookieContext::ContextMetadata::
      ContextRedirectTypeBug1221316::kPartialSameSiteRedirect;
  metadata2.http_method_bug_1221316 =
      SameSiteCookieContext::ContextMetadata::HttpMethod::kPost;
  SameSiteCookieContext context_with_metadata(
      SameSiteCookieContext::ContextType::SAME_SITE_STRICT,
      SameSiteCookieContext::ContextType::SAME_SITE_STRICT, metadata1,
      metadata2);

  EXPECT_EQ(
      "{ context: 0, schemeful_context: 0, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(cross_cross));
  EXPECT_EQ(
      "{ context: 2, schemeful_context: 2, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(lax_lax));
  EXPECT_EQ(
      "{ context: 3, schemeful_context: 3, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(strict_strict));
  EXPECT_EQ(
      "{ context: 3, schemeful_context: 0, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(strict_cross));
  EXPECT_EQ(
      "{ context: 3, schemeful_context: 2, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(strict_lax));
  EXPECT_EQ(
      "{ context: 2, schemeful_context: 0, "
      "metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 0, "
      "redirect_type_bug_1221316: 0, "
      "http_method_bug_1221316: -1 } }",
      ::testing::PrintToString(lax_cross));
  EXPECT_EQ(
      "{ context: 3, schemeful_context: 3, "
      "metadata: { cross_site_redirect_downgrade: 1, "
      "redirect_type_bug_1221316: 3, "
      "http_method_bug_1221316: 1 }, "
      "schemeful_metadata: { cross_site_redirect_downgrade: 1, "
      "redirect_type_bug_1221316: 3, "
      "http_method_bug_1221316: 3 } }",
      ::testing::PrintToString(context_with_metadata));
}

}  // namespace
}  // namespace net
