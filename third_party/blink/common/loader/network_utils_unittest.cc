// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {

TEST(OriginUtilTest, IsOriginSecure) {
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("file:///test/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("file:///test/")));

  EXPECT_TRUE(
      network_utils::IsOriginSecure(GURL("https://example.com/fun.html")));
  EXPECT_FALSE(
      network_utils::IsOriginSecure(GURL("http://example.com/fun.html")));

  EXPECT_TRUE(
      network_utils::IsOriginSecure(GURL("wss://example.com/fun.html")));
  EXPECT_FALSE(
      network_utils::IsOriginSecure(GURL("ws://example.com/fun.html")));

  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("http://localhost/fun.html")));
  EXPECT_TRUE(
      network_utils::IsOriginSecure(GURL("http://pumpkin.localhost/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("http://crumpet.pumpkin.localhost/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("http://pumpkin.localhost:8080/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("http://crumpet.pumpkin.localhost:3000/fun.html")));
  EXPECT_FALSE(
      network_utils::IsOriginSecure(GURL("http://localhost.com/fun.html")));
  EXPECT_TRUE(
      network_utils::IsOriginSecure(GURL("https://localhost.com/fun.html")));

  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("http://127.0.0.1/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("ftp://127.0.0.1/fun.html")));
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("http://127.3.0.1/fun.html")));
  EXPECT_FALSE(
      network_utils::IsOriginSecure(GURL("http://127.example.com/fun.html")));
  EXPECT_TRUE(
      network_utils::IsOriginSecure(GURL("https://127.example.com/fun.html")));

  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("http://[::1]/fun.html")));
  EXPECT_FALSE(network_utils::IsOriginSecure(GURL("http://[::2]/fun.html")));
  EXPECT_FALSE(
      network_utils::IsOriginSecure(GURL("http://[::1].example.com/fun.html")));

  EXPECT_FALSE(network_utils::IsOriginSecure(
      GURL("filesystem:http://www.example.com/temporary/")));
  EXPECT_FALSE(network_utils::IsOriginSecure(
      GURL("filesystem:ftp://www.example.com/temporary/")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("filesystem:ftp://127.0.0.1/temporary/")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("filesystem:https://www.example.com/temporary/")));

  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("about:blank")));
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("about:blank#ref")));
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("about:srcdoc")));

  EXPECT_FALSE(network_utils::IsOriginSecure(GURL("javascript:alert('blah')")));

  // TODO(lukasza): data: URLs (and opaque origins associated with them) should
  // be considered insecure according to
  // https://www.w3.org/TR/powerful-features/#is-url-trustworthy.
  // Unfortunately, changing the behavior of network_utils::IsOriginSecure
  // breaks quite a few tests for now (e.g. considering data: insecure makes us
  // think that https + data = mixed content).
  EXPECT_TRUE(network_utils::IsOriginSecure(GURL("data:test/plain;blah")));

  EXPECT_FALSE(network_utils::IsOriginSecure(
      GURL("blob:http://www.example.com/guid-goes-here")));
  EXPECT_FALSE(network_utils::IsOriginSecure(
      GURL("blob:ftp://www.example.com/guid-goes-here")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("blob:ftp://127.0.0.1/guid-goes-here")));
  EXPECT_TRUE(network_utils::IsOriginSecure(
      GURL("blob:https://www.example.com/guid-goes-here")));
}

}  // namespace blink
