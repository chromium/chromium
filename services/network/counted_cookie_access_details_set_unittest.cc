// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/counted_cookie_access_details_set.h"

#include <vector>

#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

static const GURL kDefaultUrl = GURL("https://example.com");
static const GURL kDefaultTopFrameUrl =
    GURL("https://test-top-level-origin.com");
static const GURL kAlternateUrl = GURL("https://another-test-site.html");
static const base::Time kTime = base::Time::Now();

net::CanonicalCookie GetDefaultCookie() {
  auto cookie =
      net::CanonicalCookie::CreateForTesting(kDefaultUrl, "test", kTime);
  return *cookie;
}

mojom::CookieOrLineWithAccessResultPtr
GetDefaultCookieOrLineWithAccessResult() {
  return mojom::CookieOrLineWithAccessResult::New(
      mojom::CookieOrLine::NewCookie(GetDefaultCookie()),
      net::CookieAccessResult());
}

mojom::CookieAccessDetailsPtr GetDefaultCookieAccessDetails() {
  const net::SiteForCookies site_for_cookies =
      net::SiteForCookies(net::SchemefulSite(kDefaultTopFrameUrl));
  const url::Origin top_frame_origin = url::Origin::Create(kDefaultTopFrameUrl);
  const mojom::CookieAccessDetails::Type type =
      mojom::CookieAccessDetails::Type::kRead;
  const bool is_ad_tagged = false;
  const size_t count = 1;
  std::vector<mojom::CookieOrLineWithAccessResultPtr> cookie_list;
  cookie_list.push_back(GetDefaultCookieOrLineWithAccessResult());

  return mojom::CookieAccessDetails::New(
      type, kDefaultTopFrameUrl, top_frame_origin, site_for_cookies,
      std::move(cookie_list), std::nullopt, count, is_ad_tagged,
      net::CookieSettingOverrides());
}

std::pair<mojom::CookieAccessDetailsPtr, std::unique_ptr<size_t>>
GetDefaultCountedCookieAccessDetails() {
  return std::make_pair(GetDefaultCookieAccessDetails(),
                        std::make_unique<size_t>(0));
}

std::pair<mojom::CookieAccessDetailsPtr, std::unique_ptr<size_t>>
CloneCountedCookieAccessDetails(
    const std::pair<mojom::CookieAccessDetailsPtr, std::unique_ptr<size_t>>&
        details) {
  return make_pair(details.first.Clone(),
                   std::make_unique<size_t>(*details.second));
}

TEST(CountedCookieAccessDetailsSet, DoNotAddSame) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_FALSE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryType) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->type = mojom::CookieAccessDetails::Type::kChange;
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryUrl) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->url = kAlternateUrl;
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryTopFrameOrigin) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->top_frame_origin = url::Origin::Create(kAlternateUrl);
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VarySiteForCookies) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->site_for_cookies =
      net::SiteForCookies(net::SchemefulSite(kAlternateUrl));
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VarySiteForCookiesSchemefully) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->site_for_cookies.SetSchemefullySameForTesting(
      !clone.first->site_for_cookies.schemefully_same());
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryDevtoolsRequestId) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->devtools_request_id = "foo";
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryCount) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->count = 100;
  result = details.insert(std::move(clone));
  // We do not expect count to impact equality, so we should not have added.
  EXPECT_FALSE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryIsAdTagged) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->is_ad_tagged = true;
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryCookieSettingOverrides) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->cookie_setting_overrides = net::CookieSettingOverrides::All();
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, RemoveCookieFromList) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->cookie_list.clear();
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, AddCookieToList) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->cookie_list.push_back(GetDefaultCookieOrLineWithAccessResult());
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

TEST(CountedCookieAccessDetailsSet, VaryCookie) {
  CookieAccessDetails details;
  auto detail = GetDefaultCountedCookieAccessDetails();
  auto result = details.insert(CloneCountedCookieAccessDetails(detail));
  EXPECT_TRUE(result.second);
  auto clone = CloneCountedCookieAccessDetails(detail);
  clone.first->cookie_list.clear();
  clone.first->cookie_list.push_back(GetDefaultCookieOrLineWithAccessResult());
  result = details.insert(std::move(clone));
  EXPECT_TRUE(result.second);
}

}  // namespace
}  // namespace network
