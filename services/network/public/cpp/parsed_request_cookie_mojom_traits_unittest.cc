// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_request_cookie_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/parsed_request_cookie.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(ParsedRequestCookieTraitsTest, ValidCookie) {
  net::cookie_util::ParsedRequestCookie original{"chocolate", "milk"};
  net::cookie_util::ParsedRequestCookie copied;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::ParsedRequestCookie>(
          original, copied));
  EXPECT_EQ(original, copied);
}

}  // namespace network
