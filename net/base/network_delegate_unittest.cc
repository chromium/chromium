// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace {

constexpr char kURL[] = "example.test";

CanonicalCookie MakeCookie(const std::string& name) {
  return *CanonicalCookie::CreateUnsafeCookieForTesting(
      name, "value", kURL, /*path=*/"/", /*creation=*/base::Time(),
      /*expiration=*/base::Time(), /*last_access=*/base::Time(),
      /*last_update=*/base::Time(),
      /*secure=*/true, /*httponly=*/false, CookieSameSite::UNSPECIFIED,
      CookiePriority::COOKIE_PRIORITY_DEFAULT);
}

CookieAccessResult Include() {
  return {};
}

CookieAccessResult Exclude(CookieInclusionStatus::ExclusionReason reason) {
  return CookieAccessResult(CookieInclusionStatus(reason));
}

}  // namespace

TEST(NetworkDelegateTest, ExcludeAllCookies) {
  CookieAccessResultList maybe_included_cookies = {
      {MakeCookie("1"), Include()}, {MakeCookie("2"), Include()}};

  CookieAccessResultList excluded_cookies = {
      {MakeCookie("3"),
       Exclude(CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY)}};

  NetworkDelegate::ExcludeAllCookies(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES,
      maybe_included_cookies, excluded_cookies);

  EXPECT_THAT(maybe_included_cookies, IsEmpty());
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              MatchesCookieWithName("1"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithName("2"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithName("3"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY,
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_USER_PREFERENCES}),
                  _, _, _))));
}

TEST(NetworkDelegateTest, MoveExcludedCookies) {
  CookieAccessResultList maybe_included_cookies = {
      {MakeCookie("1"), Include()},
      {MakeCookie("2"),
       Exclude(CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY)},
      {MakeCookie("3"), Include()}};

  CookieAccessResultList excluded_cookies = {{
      MakeCookie("4"),
      Exclude(CookieInclusionStatus::ExclusionReason::EXCLUDE_SECURE_ONLY),
  }};

  NetworkDelegate::MoveExcludedCookies(maybe_included_cookies,
                                       excluded_cookies);

  EXPECT_THAT(
      maybe_included_cookies,
      ElementsAre(MatchesCookieWithAccessResult(
                      MatchesCookieWithName("1"),
                      MatchesCookieAccessResult(IsInclude(), _, _, _)),
                  MatchesCookieWithAccessResult(
                      MatchesCookieWithName("3"),
                      MatchesCookieAccessResult(IsInclude(), _, _, _))));
  EXPECT_THAT(
      excluded_cookies,
      UnorderedElementsAre(
          MatchesCookieWithAccessResult(
              MatchesCookieWithName("2"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY}),
                  _, _, _)),
          MatchesCookieWithAccessResult(
              MatchesCookieWithName("4"),
              MatchesCookieAccessResult(
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<CookieInclusionStatus::ExclusionReason>{
                          CookieInclusionStatus::ExclusionReason::
                              EXCLUDE_SECURE_ONLY}),
                  _, _, _))));
}

}  // namespace net
