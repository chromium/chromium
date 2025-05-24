// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/data_remover_util.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

TEST(ConvertClearDataFilterTypeTest, ConvertsCorrectly) {
  EXPECT_EQ(
      ConvertClearDataFilterType(mojom::ClearDataFilter_Type::DELETE_MATCHES),
      net::UrlFilterType::kTrueIfMatches);
  EXPECT_EQ(
      ConvertClearDataFilterType(mojom::ClearDataFilter_Type::KEEP_MATCHES),
      net::UrlFilterType::kFalseIfMatches);
}

// The actual functionality of net::DoesUrlMatchFilter is tested in its own
// unit tests. Here we only verify that arguments are passed through
// correctly.
TEST(BindDoesUrlMatchFilterTest, Origin) {
  auto callback = BindDoesUrlMatchFilter(
      mojom::ClearDataFilter_Type::DELETE_MATCHES,
      {url::Origin::Create(GURL("https://example.com/"))}, {});
  EXPECT_TRUE(callback.Run(GURL("https://example.com/fish.html")));
  EXPECT_FALSE(callback.Run(GURL("https://www.example/fish.html")));
}

TEST(BindDoesUrlMatchFilterTest, Domain) {
  auto callback = BindDoesUrlMatchFilter(
      mojom::ClearDataFilter_Type::DELETE_MATCHES, {}, {"example.com"});
  EXPECT_TRUE(callback.Run(GURL("https://example.com/frog.html")));
  EXPECT_TRUE(callback.Run(GURL("http://www.example.com/")));
  EXPECT_FALSE(callback.Run(GURL("https://www.example/frog.html")));
}

TEST(BindDoesUrlMatchFilterTest, FilterType) {
  auto callback = BindDoesUrlMatchFilter(
      mojom::ClearDataFilter_Type::KEEP_MATCHES,
      {url::Origin::Create(GURL("https://example.com/"))}, {});
  // These expectations are the inverse of the "Origin" test above.
  EXPECT_FALSE(callback.Run(GURL("https://example.com/fish.html")));
  EXPECT_TRUE(callback.Run(GURL("https://www.example/fish.html")));
}

}  // namespace

}  // namespace network
