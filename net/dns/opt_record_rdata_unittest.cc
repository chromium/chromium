// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

template <size_t N>
constexpr std::array<uint8_t, N> BytesArray(const uint8_t (&array)[N]) {
  std::array<uint8_t, N> result = {};
  std::copy(std::begin(array), std::end(array), result.begin());
  return result;
}

TEST(OptRecordRdataTest, ParseOptRecord) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      // First OPT
      0x00, 0x01,  // OPT code
      0x00, 0x02,  // OPT data size
      0xDE, 0xAD,  // OPT data
      // Second OPT
      0x00, 0xFF,             // OPT code
      0x00, 0x04,             // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);

  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_EQ(rdata_obj->OptCount(), 2u);

  // Check contains
  ASSERT_TRUE(rdata_obj->ContainsOptCode(1));
  ASSERT_FALSE(rdata_obj->ContainsOptCode(30));

  // Check elements
  const auto data0 = BytesArray({0xde, 0xad});
  const auto data1 = BytesArray({0xde, 0xad, 0xbe, 0xef});

  std::unique_ptr<OptRecordRdata::UnknownOpt> opt0 =
      OptRecordRdata::UnknownOpt::CreateForTesting(1, data0);
  std::unique_ptr<OptRecordRdata::UnknownOpt> opt1 =
      OptRecordRdata::UnknownOpt::CreateForTesting(255, data1);

  ASSERT_EQ(*(rdata_obj->GetOpts()[0]), *(opt0.get()));
  ASSERT_EQ(*(rdata_obj->GetOpts()[1]), *(opt1.get()));
}

TEST(OptRecordRdataTest, ParseOptRecordWithShorterSizeThanData) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      0x00, 0xFF,             // OPT code
      0x00, 0x02,             // OPT data size (incorrect, should be 4)
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  DnsRecordParser parser(rdata, 0, /*num_records=*/0);

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, IsNull());
}

TEST(OptRecordRdataTest, ParseOptRecordWithLongerSizeThanData) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      0x00, 0xFF,  // OPT code
      0x00, 0x04,  // OPT data size (incorrect, should be 4)
      0xDE, 0xAD   // OPT data
  };

  DnsRecordParser parser(rdata, 0, /*num_records=*/0);

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, IsNull());
}

TEST(OptRecordRdataTest, CreateEdeOpt) {
  OptRecordRdata::EdeOpt opt0(22, std::string("Don Quixote"));

  constexpr uint8_t expected_data[] = {0x00, 0x16, 'D', 'o', 'n', ' ', 'Q',
                                       'u',  'i',  'x', 'o', 't', 'e'};

  constexpr uint8_t expected_data1[] = {0x00, 0x08, 'M', 'a', 'n', 'h',
                                        'a',  't',  't', 'a', 'n'};

  ASSERT_EQ(opt0.data(), expected_data);
  ASSERT_EQ(opt0.info_code(), 22u);
  ASSERT_EQ(opt0.extra_text(), std::string("Don Quixote"));

  std::unique_ptr<OptRecordRdata::EdeOpt> opt1 =
      OptRecordRdata::EdeOpt::Create(expected_data1);

  ASSERT_EQ(opt1->data(), expected_data1);
  ASSERT_EQ(opt1->info_code(), 8u);
  ASSERT_EQ(opt1->extra_text(), std::string("Manhattan"));
}

TEST(OptRecordRdataTest, TestEdeInfoCode) {
  std::unique_ptr<OptRecordRdata::EdeOpt> edeOpt0 =
      std::make_unique<OptRecordRdata::EdeOpt>(0, "bullettrain");
  std::unique_ptr<OptRecordRdata::EdeOpt> edeOpt1 =
      std::make_unique<OptRecordRdata::EdeOpt>(27, "ferrari");
  std::unique_ptr<OptRecordRdata::EdeOpt> edeOpt2 =
      std::make_unique<OptRecordRdata::EdeOpt>(28, "sukrit ganesh");
  ASSERT_EQ(edeOpt0->GetEnumFromInfoCode(),
            OptRecordRdata::EdeOpt::EdeInfoCode::kOtherError);
  ASSERT_EQ(
      edeOpt1->GetEnumFromInfoCode(),
      OptRecordRdata::EdeOpt::EdeInfoCode::kUnsupportedNsec3IterationsValue);
  ASSERT_EQ(edeOpt2->GetEnumFromInfoCode(),
            OptRecordRdata::EdeOpt::EdeInfoCode::kUnrecognizedErrorCode);
  ASSERT_EQ(OptRecordRdata::EdeOpt::GetEnumFromInfoCode(15),
            OptRecordRdata::EdeOpt::kBlocked);
}

// Test that an Opt EDE record is parsed correctly
TEST(OptRecordRdataTest, ParseEdeOptRecords) {
  const uint8_t rdata[] = {
      // First OPT (non-EDE record)
      0x00, 0x06,              // OPT code (6)
      0x00, 0x04,              // OPT data size (4)
      0xB0, 0xBA, 0xFE, 0x77,  // OPT data (Boba Fett)

      // Second OPT (EDE record)
      0x00, 0x0F,     // OPT code (15 for EDE)
      0x00, 0x05,     // OPT data size (info code + extra text)
      0x00, 0x0D,     // EDE info code (13 for Cached Error)
      'M', 'T', 'A',  // UTF-8 EDE extra text ("MTA")

      // Third OPT (EDE record)
      0x00, 0x0F,         // OPT code (15 for EDE)
      0x00, 0x06,         // OPT data size (info code + extra text)
      0x00, 0x10,         // EDE info code (16 for Censored)
      'M', 'B', 'T', 'A'  // UTF-8 EDE extra text ("MBTA")
  };

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);

  // Test Size of Query
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_EQ(rdata_obj->OptCount(), 3u);

  // Test Unknown Opt
  const auto data = BytesArray({0xb0, 0xba, 0xfe, 0x77});

  std::unique_ptr<OptRecordRdata::UnknownOpt> opt0 =
      OptRecordRdata::UnknownOpt::CreateForTesting(6, data);

  ASSERT_THAT(rdata_obj->GetOpts(), SizeIs(3));
  ASSERT_EQ(*rdata_obj->GetOpts()[0], *opt0.get());

  // Test EDE
  OptRecordRdata::EdeOpt edeOpt0(13, std::string("MTA", 3));
  OptRecordRdata::EdeOpt edeOpt1(16, std::string("MBTA", 4));

  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(2));
  ASSERT_EQ(*rdata_obj->GetEdeOpts()[0], edeOpt0);
  ASSERT_EQ(*rdata_obj->GetEdeOpts()[1], edeOpt1);

  // Check that member variables are alright
  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->data(), edeOpt0.data());
  ASSERT_EQ(rdata_obj->GetEdeOpts()[1]->data(), edeOpt1.data());

  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->extra_text(), std::string("MTA", 3));
  ASSERT_EQ(rdata_obj->GetEdeOpts()[1]->extra_text(), std::string("MBTA", 4));

  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->info_code(), edeOpt0.info_code());
  ASSERT_EQ(rdata_obj->GetEdeOpts()[1]->info_code(), edeOpt1.info_code());
}

TEST(OptRecordRdataTest, SerializeEdeRequest) {
  // This rdata indicates support for Structured EDNS Errors.
  // clang-format off
  const uint8_t rdata[] = {
      // Single OPT (EDE request)
      0x00, 0x0F,  // OPT code (15)
      0x00, 0x02,  // OPT data size (2)
      0x00, 0x00  // Info Code (0) [Other Error]
      // No Extra Text
  };
  // clang-format on

  auto request_rdata = OptRecordRdata::EdeOpt::CreateStructuredErrorsRequest();
  EXPECT_EQ(dns_protocol::kEdnsExtendedDnsError, request_rdata->GetCode());
  EXPECT_EQ(OptRecordRdata::EdeOpt::EdeInfoCode::kOtherError,
            request_rdata->GetEnumFromInfoCode());
  EXPECT_TRUE(request_rdata->extra_text().empty());
  OptRecordRdata opt_rdata;
  opt_rdata.AddOpt(std::move(request_rdata));
  EXPECT_THAT(opt_rdata.buf(), ElementsAreArray(rdata));

  // Check that Create can parse itself.
  const auto parsed_rdata =
      OptRecordRdata::Create(base::as_byte_span(opt_rdata.buf()));
  ASSERT_THAT(parsed_rdata, NotNull());
  ASSERT_EQ(parsed_rdata->OptCount(), 1u);
  const auto parsed_ede_opts = parsed_rdata->GetEdeOpts();
  ASSERT_EQ(parsed_ede_opts.size(), 1u);
  EXPECT_EQ(parsed_ede_opts.front()->GetEnumFromInfoCode(),
            OptRecordRdata::EdeOpt::EdeInfoCode::kOtherError);
  EXPECT_EQ(parsed_ede_opts.front()->extra_text(), "");
}

class FilteringDetailsParsingTest : public testing::Test {
 protected:
  FilteringDetailsParsingTest() {
    feature_list_.InitAndEnableFeature(features::kUseStructuredDnsErrors);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that an Opt filtering details record is parsed correctly
TEST_F(FilteringDetailsParsingTest, ParseFilteringDetailsOptRecords) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"example","id":"abc123"},)"
      R"({"db":"second-example","id":"456","r":"ignored"}],)"
      R"("j":"should be ignored","c":["mailto:ignored@example.com"]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),  // data size
      0x00, 0x0F  // EDE info code (15 for Blocked)
  };
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());

  const std::vector<const OptRecordRdata::EdeOpt*>& ede_opts =
      rdata_obj->GetEdeOpts();
  ASSERT_THAT(ede_opts, SizeIs(1));
  const auto* ede_opt = ede_opts[0];

  const std::vector<OptRecordRdata::EdeOpt::FilteringDetails>& incidents =
      ede_opt->filtering_details();
  ASSERT_EQ(incidents.size(), static_cast<size_t>(2));

  const OptRecordRdata::EdeOpt::FilteringDetails& first = incidents.front();
  EXPECT_EQ(first.resolver_operator_id, "example");
  EXPECT_EQ(first.filtering_incident_id, "abc123");

  const OptRecordRdata::EdeOpt::FilteringDetails& second = incidents[1];
  EXPECT_EQ(second.resolver_operator_id, "second-example");
  EXPECT_EQ(second.filtering_incident_id, "456");
}
TEST_F(FilteringDetailsParsingTest, MissingRoRejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"only-db"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),  // data size
      0x00, 0x0F  // EDE info code (15 for Blocked)
  };
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, Utf8EmojiAccepted) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"💡","id":"123"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  ASSERT_FALSE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, SurrogatePairAccepted) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"\uD83D\uDCA9","id":"pile"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_FALSE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, UnpairedHighSurrogateRejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"\uD800","id":"x"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, UnpairedLowSurrogateRejected) {
  static constexpr std::string_view kJsonExtraText =
      R"("fdbs"[{"db":"\uDEAD","id":"x"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, SwappedSurrogateRejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"\uDEAD\uD800","id":"x"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, NonStringIncRejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"db","id":false}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, KeyOrderVariantAccepted) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"id":"incident","db":"database"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_FALSE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, UnicodeNonCharacterFDD0Rejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"\uFDD0","id":"ok"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

TEST_F(FilteringDetailsParsingTest, UnicodeNonCharacterFFFERejected) {
  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"\uFFFE","id":"ok"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F, 0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),
      0x00, 0x0F};
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));

  const net::OptRecordRdata::EdeOpt* ede_opt = rdata_obj->GetEdeOpts()[0];
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

// Test that FilteringDetails metadata is NOT parsed when the feature is
// disabled, even if the JSON fields "fdbs[].db" "fdbs[].inc" are present.
TEST_F(FilteringDetailsParsingTest, FeatureDisabled) {
  // Disable the feature flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kUseStructuredDnsErrors);

  static constexpr std::string_view kJsonExtraText =
      R"({"fdbs":[{"db":"exampleResolver","id":"abc123"}]})";

  std::vector<uint8_t> rdata = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, static_cast<uint8_t>(2 + kJsonExtraText.size()),  // data size
      0x00, 0x0F  // EDE info code (15 for Blocked)
  };
  rdata.insert(rdata.end(), kJsonExtraText.begin(), kJsonExtraText.end());

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());

  const std::vector<const OptRecordRdata::EdeOpt*>& ede_opts =
      rdata_obj->GetEdeOpts();
  ASSERT_THAT(ede_opts, SizeIs(1));
  const auto* ede_opt = ede_opts[0];

  // Feature is disabled, so metadata should NOT be parsed.
  EXPECT_TRUE(ede_opt->filtering_details().empty());
}

// Test the Opt equality operator (and its subclasses as well)
TEST(OptRecordRdataTest, OptEquality) {
  // `rdata_obj0` second opt has extra text "BIOS"
  // `rdata_obj1` second opt has extra text "BIOO"
  // Note: rdata_obj0 and rdata_obj1 have 2 common Opts and 1 different one.
  OptRecordRdata rdata_obj0;
  const auto data0 = BytesArray({0xb0, 0xba, 0xfe, 0x77});

  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(6, data0));
  rdata_obj0.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(13, std::string("USA", 3)));
  rdata_obj0.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(16, std::string("BIOS", 4)));
  ASSERT_EQ(rdata_obj0.OptCount(), 3u);

  OptRecordRdata rdata_obj1;
  const auto data1 = BytesArray({0xb0, 0xba, 0xfe, 0x77});
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(6, data1));
  rdata_obj1.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(13, std::string("USA", 3)));
  rdata_obj1.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(16, std::string("BIOO", 4)));
  ASSERT_EQ(rdata_obj1.OptCount(), 3u);

  auto opts0 = rdata_obj0.GetOpts();
  auto opts1 = rdata_obj1.GetOpts();
  auto edeOpts0 = rdata_obj0.GetEdeOpts();
  auto edeOpts1 = rdata_obj1.GetEdeOpts();
  ASSERT_THAT(opts0, SizeIs(3));
  ASSERT_THAT(opts1, SizeIs(3));
  ASSERT_THAT(edeOpts0, SizeIs(2));
  ASSERT_THAT(edeOpts1, SizeIs(2));

  // Opt equality
  ASSERT_EQ(*opts0[0], *opts1[0]);
  ASSERT_EQ(*opts0[1], *opts1[1]);
  ASSERT_NE(*opts0[0], *opts1[1]);

  // EdeOpt equality
  ASSERT_EQ(*edeOpts0[0], *edeOpts1[0]);
  ASSERT_NE(*edeOpts0[1], *edeOpts1[1]);

  // EdeOpt equality with Opt
  ASSERT_EQ(*edeOpts0[0], *opts1[1]);
  ASSERT_NE(*edeOpts0[1], *opts1[2]);

  // Opt equality with EdeOpt
  // Should work if raw data matches
  ASSERT_EQ(*opts1[1], *edeOpts0[0]);
  ASSERT_NE(*opts1[2], *edeOpts0[1]);
}

// Check that rdata is null if the data section of an EDE record is too small
// (<2 bytes)
TEST(OptRecordRdataTest, EdeRecordTooSmall) {
  const uint8_t rdata[] = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, 0x01,  // OPT data size (info code + extra text)
      0x00         // Fragment of Info Code
  };

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, IsNull());
}

// Check that an EDE record with no extra text is parsed correctly.
TEST(OptRecordRdataTest, EdeRecordNoExtraText) {
  const uint8_t rdata[] = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, 0x02,  // OPT data size (info code + extra text)
      0x00, 0x05   // Info Code
  };

  const uint8_t expected_data[] = {0x00, 0x05};

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));
  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->data(), expected_data);
  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->info_code(), 5u);
  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->extra_text(), "");
}

// Check that an EDE record with non-UTF-8 fails to parse.
TEST(OptRecordRdataTest, EdeRecordExtraTextNonUTF8) {
  const uint8_t rdata[] = {
      0x00, 0x0F,             // OPT code (15 for EDE)
      0x00, 0x06,             // OPT data size (info code + extra text)
      0x00, 0x05,             // Info Code
      0xB1, 0x05, 0xF0, 0x0D  // Extra Text (non-UTF-8)
  };

  ASSERT_FALSE(base::IsStringUTF8(std::string("\xb1\x05\xf0\x0d", 4)));

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, IsNull());
}

// Check that an EDE record with an unknown info code is parsed correctly.
TEST(OptRecordRdataTest, EdeRecordUnknownInfoCode) {
  const uint8_t rdata[] = {
      0x00, 0x0F,                     // OPT code (15 for EDE)
      0x00, 0x08,                     // OPT data size (info code + extra text)
      0x00, 0x44,                     // Info Code (68 doesn't exist)
      'B',  'O',  'S', 'T', 'O', 'N'  // Extra Text ("BOSTON")
  };

  const uint8_t expected_data[] = {0x00, 0x44, 'B', 'O', 'S', 'T', 'O', 'N'};

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));
  auto* opt = rdata_obj->GetEdeOpts()[0];
  ASSERT_EQ(opt->data(), expected_data);
  ASSERT_EQ(opt->info_code(), 68u);
  ASSERT_EQ(opt->extra_text(), std::string("BOSTON", 6));
  ASSERT_EQ(opt->GetEnumFromInfoCode(),
            OptRecordRdata::EdeOpt::EdeInfoCode::kUnrecognizedErrorCode);
}

TEST(OptRecordRdataTest, CreatePaddingOpt) {
  std::unique_ptr<OptRecordRdata::PaddingOpt> opt0 =
      std::make_unique<OptRecordRdata::PaddingOpt>(12);

  constexpr const uint8_t expected_data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 12 null characters
  };

  // For std::string expected_data1("MASSACHUSETTS");
  constexpr const uint8_t expected_data1[] = {'M', 'A', 'S', 'S', 'A', 'C', 'H',
                                              'U', 'S', 'E', 'T', 'T', 'S'};

  ASSERT_EQ(opt0->data(), expected_data);
  ASSERT_THAT(opt0->data(), SizeIs(12u));

  std::unique_ptr<OptRecordRdata::PaddingOpt> opt1 =
      std::make_unique<OptRecordRdata::PaddingOpt>("MASSACHUSETTS");

  ASSERT_EQ(opt1->data(), expected_data1);
  ASSERT_THAT(opt1->data(), SizeIs(13u));
}

TEST(OptRecordRdataTest, ParsePaddingOpt) {
  const uint8_t rdata[] = {
      // First OPT
      0x00, 0x0C,  // OPT code
      0x00, 0x07,  // OPT data size
      0xB0, 0x03,  // OPT data padding (Book of Boba Fett)
      0x0F, 0xB0, 0xBA, 0xFE, 0x77,
  };

  std::unique_ptr<OptRecordRdata> rdata_obj = OptRecordRdata::Create(rdata);

  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_EQ(rdata_obj->OptCount(), 1u);
  ASSERT_THAT(rdata_obj->GetOpts(), SizeIs(1));
  ASSERT_THAT(rdata_obj->GetPaddingOpts(), SizeIs(1));

  // Check elements
  OptRecordRdata::PaddingOpt opt0(
      std::string("\xb0\x03\x0f\xb0\xba\xfe\x77", 7));

  ASSERT_EQ(*(rdata_obj->GetOpts()[0]), opt0);
  ASSERT_EQ(*(rdata_obj->GetPaddingOpts()[0]), opt0);
  ASSERT_THAT(opt0.data(), SizeIs(7u));
}

TEST(OptRecordRdataTest, AddOptToOptRecord) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t expected_rdata[] = {
      0x00, 0xFF,             // OPT code
      0x00, 0x04,             // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  OptRecordRdata rdata;
  const auto data = BytesArray({0xde, 0xad, 0xbe, 0xef});

  rdata.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(255, data));
  EXPECT_THAT(rdata.buf(), ElementsAreArray(expected_rdata));
}

// Test the OptRecordRdata equality operator.
// Equality must be order sensitive. If Opts are same but inserted in different
// order, test will fail epically.
TEST(OptRecordRdataTest, EqualityIsOptOrderSensitive) {
  // Control rdata
  OptRecordRdata rdata_obj0;
  const auto data0 = BytesArray({0xb0, 0xba, 0xfe, 0x77});
  const auto data1 = BytesArray({0xb1, 0x05, 0xf0, 0x0d});
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(1, data0));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(2, data1));
  ASSERT_EQ(rdata_obj0.OptCount(), 2u);

  // Same as `rdata_obj0`
  OptRecordRdata rdata_obj1;
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(1, data0));
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(2, data1));
  ASSERT_EQ(rdata_obj1.OptCount(), 2u);

  ASSERT_EQ(rdata_obj0, rdata_obj1);

  // Same contents as `rdata_obj0` & `rdata_obj1`, but different order
  OptRecordRdata rdata_obj2;
  rdata_obj2.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(2, data1));
  rdata_obj2.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(1, data0));
  ASSERT_EQ(rdata_obj2.OptCount(), 2u);

  // Order matters! obj0 and obj2 contain same Opts but in different order.
  ASSERT_FALSE(rdata_obj0.IsEqual(&rdata_obj2));

  // Contains only `rdata_obj0` first opt
  // 2nd opt is added later
  OptRecordRdata rdata_obj3;
  rdata_obj3.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(1, data0));
  ASSERT_EQ(rdata_obj3.OptCount(), 1u);

  ASSERT_FALSE(rdata_obj0.IsEqual(&rdata_obj3));

  rdata_obj3.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(2, data1));

  ASSERT_TRUE(rdata_obj0.IsEqual(&rdata_obj3));

  // Test == operator
  ASSERT_TRUE(rdata_obj0 == rdata_obj1);
  ASSERT_EQ(rdata_obj0, rdata_obj1);
  ASSERT_NE(rdata_obj0, rdata_obj2);
}

// Test that GetOpts() follows specified order.
// Sort by key, then by insertion order.
TEST(OptRecordRdataTest, TestGetOptsOrder) {
  OptRecordRdata rdata_obj0;
  const auto data0 = BytesArray({0x33, 0x33});
  const auto data1 = BytesArray({0x11, 0x11});
  const auto data2 = BytesArray({0x22, 0x22});

  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(10, data0));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(5, data1));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(5, data2));
  ASSERT_EQ(rdata_obj0.OptCount(), 3u);

  constexpr const uint8_t expected_data[] = {0x11, 0x11};
  constexpr const uint8_t expected_data1[] = {0x22, 0x22};
  constexpr const uint8_t expected_data2[] = {0x33, 0x33};

  auto opts = rdata_obj0.GetOpts();
  ASSERT_EQ(opts[0]->data(), expected_data);   // opt code 5 (inserted first)
  ASSERT_EQ(opts[1]->data(), expected_data1);  // opt code 5 (inserted second)
  ASSERT_EQ(opts[2]->data(), expected_data2);  // opt code 10
}

}  // namespace
}  // namespace net
