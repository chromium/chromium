// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/cookie/canonical_cookie.h"

#include <initializer_list>

#include "base/optional.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(CanonicalCookieTest, Defaults) {
  CanonicalCookie cookie;
  EXPECT_EQ(WebString(), cookie.Name());
  EXPECT_EQ(WebString(), cookie.Value());
  EXPECT_EQ(WebString(), cookie.Domain());
  EXPECT_EQ(WebString(), cookie.Path());
  EXPECT_EQ(base::Time(), cookie.CreationDate());
  EXPECT_EQ(base::Time(), cookie.ExpiryDate());
  EXPECT_EQ(base::Time(), cookie.LastAccessDate());
  EXPECT_FALSE(cookie.IsSecure());
  EXPECT_FALSE(cookie.IsHttpOnly());
  EXPECT_EQ(network::mojom::CookieSameSite::NO_RESTRICTION, cookie.SameSite());
  EXPECT_EQ(CanonicalCookie::kDefaultPriority, cookie.Priority());
}

TEST(CanonicalCookieTest, CreationFailure) {
  const WebURL url(KURL("http://example.com"));

  // Invalid cookie lines cause nullopt to be returned.
  EXPECT_FALSE(
      CanonicalCookie::Create(url, "\x01", base::Time::Now()).has_value());

  // Invalid names cause nullopt to be returned.
  EXPECT_FALSE(CanonicalCookie::Create(
                   "\x01", "value", "domain", "/path", base::Time::Now(),
                   base::Time::Now(), base::Time::Now(), false, false,
                   network::mojom::CookieSameSite::NO_RESTRICTION,
                   CanonicalCookie::kDefaultPriority)
                   .has_value());
}

TEST(CanonicalCookieTest, Properties) {
  const base::Time t1 = base::Time::FromDoubleT(1);
  const base::Time t2 = base::Time::FromDoubleT(2);
  const base::Time t3 = base::Time::FromDoubleT(3);
  ASSERT_NE(t1, t2);
  ASSERT_NE(t1, t3);
  ASSERT_NE(t2, t3);

  base::Optional<CanonicalCookie> cookie_opt = CanonicalCookie::Create(
      "name", "value", "domain", "/path", t1, t2, t3, true, true,
      network::mojom::CookieSameSite::STRICT_MODE,
      network::mojom::CookiePriority::HIGH);
  ASSERT_TRUE(cookie_opt);
  CanonicalCookie& cookie = cookie_opt.value();

  EXPECT_EQ("name", cookie.Name());
  EXPECT_EQ("value", cookie.Value());
  EXPECT_EQ("domain", cookie.Domain());
  EXPECT_EQ("/path", cookie.Path());
  EXPECT_EQ(t1, cookie.CreationDate());
  EXPECT_EQ(t2, cookie.ExpiryDate());
  EXPECT_EQ(t3, cookie.LastAccessDate());
  EXPECT_TRUE(cookie.IsSecure());
  EXPECT_TRUE(cookie.IsHttpOnly());
  EXPECT_EQ(network::mojom::CookieSameSite::STRICT_MODE, cookie.SameSite());
  EXPECT_EQ(network::mojom::CookiePriority::HIGH, cookie.Priority());

  // Exercise WebCookieSameSite values.
  for (auto same_site : {network::mojom::CookieSameSite::NO_RESTRICTION,
                         network::mojom::CookieSameSite::LAX_MODE,
                         network::mojom::CookieSameSite::STRICT_MODE}) {
    EXPECT_EQ(same_site,
              CanonicalCookie::Create("name", "value", "domain", "/path", t1,
                                      t2, t3, false, false, same_site,
                                      CanonicalCookie::kDefaultPriority)
                  ->SameSite());
  }

  // Exercise WebCookiePriority values.
  for (auto priority : {network::mojom::CookiePriority::LOW,
                        network::mojom::CookiePriority::MEDIUM,
                        network::mojom::CookiePriority::HIGH,
                        CanonicalCookie::kDefaultPriority}) {
    EXPECT_EQ(priority,
              CanonicalCookie::Create(
                  "name", "value", "domain", "/path", t1, t2, t3, false, false,
                  network::mojom::CookieSameSite::NO_RESTRICTION, priority)
                  ->Priority());
  }
}

}  // namespace blink
