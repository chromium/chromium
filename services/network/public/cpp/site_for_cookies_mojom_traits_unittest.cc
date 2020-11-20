// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/site_for_cookies_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/site_for_cookies.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {

TEST(SiteForCookiesMojomTraitsTest, SerializeAndDeserialize) {
  std::vector<net::SiteForCookies> keys = {
      net::SiteForCookies(),
      net::SiteForCookies::FromUrl(GURL("file:///whatver")),
      net::SiteForCookies::FromUrl(GURL("ws://127.0.0.1/things")),
      net::SiteForCookies::FromUrl(GURL("https://example.com"))};

  for (auto original : keys) {
    net::SiteForCookies copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<network::mojom::SiteForCookies>(
            original, copied));
    EXPECT_TRUE(original.IsEquivalent(copied));
    EXPECT_EQ(original.schemefully_same(), copied.schemefully_same());
  }
}

}  // namespace mojo
