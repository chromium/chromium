// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/suitable_trust_token_origin.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

TEST(SuitableTrustTokenOrigin, Suitable) {
  // Suitable: HTTP/HTTPS and potentially trustworthy
  auto suitable_url = GURL("https://suitable-origin.example");
  auto suitable_origin = url::Origin::Create(suitable_url);

  std::optional<SuitableTrustTokenOrigin> created_from_url =
      SuitableTrustTokenOrigin::Create(suitable_url);

  ASSERT_TRUE(created_from_url);
  EXPECT_EQ(created_from_url->origin(), suitable_origin);

  std::optional<SuitableTrustTokenOrigin> created_from_origin =
      SuitableTrustTokenOrigin::Create(suitable_origin);
  ASSERT_TRUE(created_from_origin);
  EXPECT_EQ(created_from_origin->origin(), suitable_origin);

  EXPECT_EQ(created_from_origin->Serialize(), suitable_origin.Serialize());
}

TEST(SuitableTrustTokenOrigin, Insecure) {
  // Unsuitable: not potentially trustworthy
  auto unsuitable_url = GURL("http://suitable-origin.example");
  auto unsuitable_origin = url::Origin::Create(unsuitable_url);

  EXPECT_FALSE(SuitableTrustTokenOrigin::Create(unsuitable_url));
  EXPECT_FALSE(SuitableTrustTokenOrigin::Create(unsuitable_origin));
}

TEST(SuitableTrustTokenOrigin, SecureButNeitherHttpNorHttps) {
  // Unsuitable: potentially trustworthy, but neither HTTP nor HTTPS
  auto unsuitable_url = GURL("file:///");
  auto unsuitable_origin = url::Origin::Create(unsuitable_url);

  EXPECT_FALSE(SuitableTrustTokenOrigin::Create(unsuitable_url));
  EXPECT_FALSE(SuitableTrustTokenOrigin::Create(unsuitable_origin));
}

}  // namespace network
