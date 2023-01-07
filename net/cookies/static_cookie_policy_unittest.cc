// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/static_cookie_policy.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test::IsOk;

namespace net {

class StaticCookiePolicyTest : public testing::Test {
 public:
  StaticCookiePolicyTest()
      : url_google_("http://www.google.izzle"),
        url_google_secure_("https://www.google.izzle"),
        url_google_mail_("http://mail.google.izzle"),
        url_google_analytics_("http://www.googleanalytics.izzle") {}
  void SetPolicyType(StaticCookiePolicy::Type type) { policy_.set_type(type); }
  int CanAccessCookies(const GURL& url, const GURL& first_party) {
    return policy_.CanAccessCookies(url,
                                    net::SiteForCookies::FromUrl(first_party));
  }

 protected:
  StaticCookiePolicy policy_;
  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_mail_;
  GURL url_google_analytics_;
};

TEST_F(StaticCookiePolicyTest, DefaultPolicyTest) {
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_secure_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_mail_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_analytics_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, GURL()), IsOk());
}

TEST_F(StaticCookiePolicyTest, AllowAllCookiesTest) {
  SetPolicyType(StaticCookiePolicy::ALLOW_ALL_COOKIES);

  EXPECT_THAT(CanAccessCookies(url_google_, url_google_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_secure_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_mail_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_analytics_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, GURL()), IsOk());
}

TEST_F(StaticCookiePolicyTest, BlockAllThirdPartyCookiesTest) {
  SetPolicyType(StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);

  EXPECT_THAT(CanAccessCookies(url_google_, url_google_), IsOk());
  EXPECT_THAT(CanAccessCookies(url_google_, url_google_mail_), IsOk());
  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_secure_));
  EXPECT_NE(OK, CanAccessCookies(url_google_secure_, url_google_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_analytics_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockAllCookiesTest) {
  SetPolicyType(StaticCookiePolicy::BLOCK_ALL_COOKIES);

  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_secure_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_mail_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, url_google_analytics_));
  EXPECT_NE(OK, CanAccessCookies(url_google_, GURL()));
}

}  // namespace net
