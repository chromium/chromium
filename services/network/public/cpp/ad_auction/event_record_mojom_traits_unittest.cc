// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ad_auction/event_record_mojom_traits.h"

#include <optional>
#include <string_view>

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/http/structured_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

using ::testing::ElementsAre;

constexpr std::string_view kEligibleOrigin1Str = "https://example-dsp1.test";
constexpr std::string_view kEligibleOrigin2Str = "https://example-dsp2.test";

constexpr std::string_view kProviderOriginStr = "https://example-provider.test";

TEST(AdAuctionEventRecordMojomTraitsTest, SerializeAndDeserialize) {
  constexpr std::string_view kValidHeaderContentsView =
      "type=\"view\", eligible-origins=(\"https://example-dsp1.test\" "
      "\"https://example-dsp2.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kValidHeaderContentsView);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);

  AdAuctionEventRecord record_clone;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::AdAuctionEventRecord>(
          *record, record_clone));

  EXPECT_EQ(AdAuctionEventRecord::Type::kView, record_clone.type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record_clone.providing_origin);
  EXPECT_THAT(record_clone.eligible_origins,
              ElementsAre(url::Origin::Create(GURL(kEligibleOrigin1Str)),
                          url::Origin::Create(GURL(kEligibleOrigin2Str))));
  EXPECT_TRUE(record_clone.IsValid());
}

TEST(AdAuctionEventRecordMojomTraitsTest, SerializeAndDeserialize_Invalid) {
  AdAuctionEventRecord manual_record;
  manual_record.type = AdAuctionEventRecord::Type::kView;
  manual_record.providing_origin =
      url::Origin::Create(GURL(kProviderOriginStr));
  EXPECT_TRUE(manual_record.IsValid());
  manual_record.eligible_origins.emplace_back(
      url::Origin::Create(GURL("http://not-https.test")));
  EXPECT_FALSE(manual_record.IsValid());

  AdAuctionEventRecord record_clone;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<network::mojom::AdAuctionEventRecord>(
          manual_record, record_clone));
}

}  // namespace
}  // namespace network
