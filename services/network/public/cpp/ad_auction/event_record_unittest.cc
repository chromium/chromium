// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ad_auction/event_record.h"

#include <optional>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/http/structured_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr std::string_view kEligibleOrigin1Str = "https://example-dsp1.test";
constexpr std::string_view kEligibleOrigin2Str = "https://example-dsp2.test";

constexpr std::string_view kValidHeaderContentsView =
    "type=\"view\", eligible-origins=(\"https://example-dsp1.test\" "
    "\"https://example-dsp2.test\")";

constexpr std::string_view kValidHeaderContentsClick =
    "type=\"click\", eligible-origins=(\"https://example-dsp1.test\" "
    "\"https://example-dsp2.test\")";

constexpr std::string_view kProviderOriginStr = "https://example-provider.test";

TEST(AdAuctionEventRecordTest, GetHeader) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(2u, 0u), "200 OK")
          .AddHeader("Ad-Auction-Record-Event", kValidHeaderContentsView)
          .Build();

  EXPECT_EQ(kValidHeaderContentsView,
            AdAuctionEventRecord::GetAdAuctionRecordEventHeader(headers.get()));
}

TEST(AdAuctionEventRecordTest, GetHeader_NotFound) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(2u, 0u), "200 OK")
          .AddHeader("Wrong-Header-Name", kValidHeaderContentsView)
          .Build();

  EXPECT_EQ(std::nullopt,
            AdAuctionEventRecord::GetAdAuctionRecordEventHeader(headers.get()));
}

TEST(AdAuctionEventRecordTest, GetHeader_NoHeaders) {
  EXPECT_EQ(std::nullopt,
            AdAuctionEventRecord::GetAdAuctionRecordEventHeader(nullptr));
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_View) {
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kValidHeaderContentsView);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);
  EXPECT_EQ(AdAuctionEventRecord::Type::kView, record->type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record->providing_origin);
  EXPECT_THAT(record->eligible_origins,
              ElementsAre(url::Origin::Create(GURL(kEligibleOrigin1Str)),
                          url::Origin::Create(GURL(kEligibleOrigin2Str))));
  EXPECT_TRUE(record->IsValid());
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_Click) {
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kValidHeaderContentsClick);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kClick,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);
  EXPECT_EQ(AdAuctionEventRecord::Type::kClick, record->type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record->providing_origin);
  EXPECT_THAT(record->eligible_origins,
              ElementsAre(url::Origin::Create(GURL(kEligibleOrigin1Str)),
                          url::Origin::Create(GURL(kEligibleOrigin2Str))));
  EXPECT_TRUE(record->IsValid());
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_ParametersIgnored) {
  constexpr std::string_view kValidHeaderContentsViewWithParameters =
      "type=\"view\";foo=1, "
      "eligible-origins=(\"https://example-dsp1.test\";bar=2 "
      "\"https://example-dsp2.test\"; baz=3);qux=4";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(
          kValidHeaderContentsViewWithParameters);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);
  EXPECT_EQ(AdAuctionEventRecord::Type::kView, record->type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record->providing_origin);
  EXPECT_THAT(record->eligible_origins,
              ElementsAre(url::Origin::Create(GURL(kEligibleOrigin1Str)),
                          url::Origin::Create(GURL(kEligibleOrigin2Str))));
  EXPECT_TRUE(record->IsValid());
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_TypeMismatch) {
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kValidHeaderContentsView);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kClick,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_InvalidType) {
  constexpr std::string_view kNoTypeDict =
      "type=\"not-a-type\", eligible-origins=(\"https://example-dsp1.test\" "
      "\"https://example-dsp2.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kNoTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_NoType) {
  constexpr std::string_view kNoTypeDict =
      "eligible-origins=(\"https://example-dsp1.test\" "
      "\"https://example-dsp2.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kNoTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_InvalidTypeInnerList) {
  constexpr std::string_view kInnerListTypeDict =
      "type=(\"view\"), eligible-origins=(\"https://example-dsp1.test\" "
      "\"https://example-dsp2.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kInnerListTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_InvalidTypeNotString) {
  constexpr std::string_view kIntegerTypeDict =
      "type=42, eligible-origins=(\"https://example-dsp1.test\" "
      "\"https://example-dsp2.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kIntegerTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_EmptyEligibleOrigins) {
  constexpr std::string_view kIntegerTypeDict =
      "type=\"view\", eligible-origins=()";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kIntegerTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);
  EXPECT_EQ(AdAuctionEventRecord::Type::kView, record->type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record->providing_origin);
  EXPECT_THAT(record->eligible_origins, IsEmpty());
  EXPECT_TRUE(record->IsValid());
}

TEST(AdAuctionEventRecordTest, CreateFromStructuredDict_NoEligibleOrigins) {
  constexpr std::string_view kIntegerTypeDict = "type=\"view\"";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kIntegerTypeDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  ASSERT_TRUE(record);
  EXPECT_EQ(AdAuctionEventRecord::Type::kView, record->type);
  EXPECT_EQ(url::Origin::Create(GURL(kProviderOriginStr)),
            record->providing_origin);
  EXPECT_THAT(record->eligible_origins, IsEmpty());
  EXPECT_TRUE(record->IsValid());
}

TEST(AdAuctionEventRecordTest,
     CreateFromStructuredDict_EligibleOriginsNotInnerList) {
  constexpr std::string_view kNotInnerListDict =
      "type=\"view\", eligible-origins=\"https://example-dsp1.test\"";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kNotInnerListDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest,
     CreateFromStructuredDict_EligibleOriginsNotListOfStrings) {
  constexpr std::string_view kNotListOfStringsDict =
      "type=\"view\", eligible-origins=(42)";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kNotListOfStringsDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);
}

TEST(AdAuctionEventRecordTest,
     CreateFromStructuredDict_NotHttpsProvidingOrigin) {
  const url::Origin kNotHttps =
      url::Origin::Create(GURL("http://not-http.test"));
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kValidHeaderContentsView);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/kNotHttps);
  EXPECT_FALSE(record);

  AdAuctionEventRecord manual_record;
  manual_record.type = AdAuctionEventRecord::Type::kView;
  manual_record.providing_origin =
      url::Origin::Create(GURL(kProviderOriginStr));
  EXPECT_TRUE(manual_record.IsValid());
  manual_record.providing_origin = kNotHttps;
  EXPECT_FALSE(manual_record.IsValid());
}

TEST(AdAuctionEventRecordTest,
     CreateFromStructuredDict_NotHttpsEligibleOrigin) {
  constexpr std::string_view kNotHttpsDict =
      "type=\"view\", eligible-origins=(\"http://not-http.test\")";
  std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(kNotHttpsDict);
  ASSERT_TRUE(dict);

  std::optional<AdAuctionEventRecord> record =
      AdAuctionEventRecord::MaybeCreateFromStructuredDict(
          /*dict=*/*dict,
          /*expected_type=*/AdAuctionEventRecord::Type::kView,
          /*providing_origin=*/url::Origin::Create(GURL(kProviderOriginStr)));
  EXPECT_FALSE(record);

  AdAuctionEventRecord manual_record;
  manual_record.type = AdAuctionEventRecord::Type::kView;
  manual_record.providing_origin =
      url::Origin::Create(GURL(kProviderOriginStr));
  EXPECT_TRUE(manual_record.IsValid());
  manual_record.eligible_origins.emplace_back(
      url::Origin::Create(GURL("http://not-https.test")));
  EXPECT_FALSE(manual_record.IsValid());
}

}  // namespace
}  // namespace network
