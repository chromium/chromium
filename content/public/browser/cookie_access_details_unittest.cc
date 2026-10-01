// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cookie_access_details.h"

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kUrl[] = "https://example.test/path";
constexpr char kFirstPartyUrl[] = "https://top.test/";

// std::vector only moves its elements when it grows if the move constructor
// is noexcept; otherwise it copies them.
static_assert(std::is_nothrow_move_constructible_v<CookieAccessDetails>);
static_assert(std::is_nothrow_move_assignable_v<CookieAccessDetails>);

CookieAccessDetails CreateDetails() {
  const GURL url(kUrl);
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateForTesting(
          url, "name=value", base::Time::Now(), net::CookieSourceType::kHTTP);
  CHECK(cookie);
  net::CookieAccessResultList cookies;
  cookies.push_back({*cookie, net::CookieAccessResult()});
  return CookieAccessDetails(CookieAccessDetails::Type::kRead, url,
                             GURL(kFirstPartyUrl), cookies,
                             /*blocked_by_policy=*/true, /*is_ad_tagged=*/true,
                             net::CookieSettingOverrides(),
                             net::SiteForCookies::FromUrl(GURL(kFirstPartyUrl)),
                             CookieAccessDetails::Source::kNavigation);
}

void ExpectDetailsFromCreateDetails(const CookieAccessDetails& details) {
  EXPECT_EQ(details.type, CookieAccessDetails::Type::kRead);
  EXPECT_EQ(details.url, GURL(kUrl));
  EXPECT_EQ(details.first_party_url, GURL(kFirstPartyUrl));
  ASSERT_EQ(details.cookie_access_result_list.size(), 1u);
  EXPECT_EQ(details.cookie_access_result_list[0].cookie.Name(), "name");
  EXPECT_EQ(details.cookie_access_result_list[0].cookie.Value(), "value");
  EXPECT_TRUE(details.blocked_by_policy);
  EXPECT_TRUE(details.is_ad_tagged);
  EXPECT_TRUE(details.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL(kFirstPartyUrl))));
  EXPECT_EQ(details.source, CookieAccessDetails::Source::kNavigation);
}

}  // namespace

TEST(CookieAccessDetailsTest, MoveConstructionKeepsTheCookieList) {
  CookieAccessDetails details = CreateDetails();
  const net::CookieWithAccessResult* cookies =
      details.cookie_access_result_list.data();

  CookieAccessDetails moved(std::move(details));

  // A copy would have allocated a new list.
  EXPECT_EQ(moved.cookie_access_result_list.data(), cookies);
  ExpectDetailsFromCreateDetails(moved);
}

TEST(CookieAccessDetailsTest, MoveAssignmentKeepsTheCookieList) {
  CookieAccessDetails details = CreateDetails();
  const net::CookieWithAccessResult* cookies =
      details.cookie_access_result_list.data();

  CookieAccessDetails moved;
  moved = std::move(details);

  EXPECT_EQ(moved.cookie_access_result_list.data(), cookies);
  ExpectDetailsFromCreateDetails(moved);
}

TEST(CookieAccessDetailsTest, VectorGrowthMovesElements) {
  std::vector<CookieAccessDetails> accesses;
  accesses.push_back(CreateDetails());
  const net::CookieWithAccessResult* cookies =
      accesses[0].cookie_access_result_list.data();

  // Force a reallocation.
  accesses.reserve(accesses.capacity() + 1);

  EXPECT_EQ(accesses[0].cookie_access_result_list.data(), cookies);
  ExpectDetailsFromCreateDetails(accesses[0]);
}

TEST(CookieAccessDetailsTest, CopyStillCopies) {
  const CookieAccessDetails details = CreateDetails();

  CookieAccessDetails copy(details);

  EXPECT_NE(copy.cookie_access_result_list.data(),
            details.cookie_access_result_list.data());
  ExpectDetailsFromCreateDetails(copy);
  ExpectDetailsFromCreateDetails(details);
}

}  // namespace content
