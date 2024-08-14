// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/big_endian.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

std::string_view MakeStringPiece(const uint8_t* data, unsigned size) {
  const char* data_cc = reinterpret_cast<const char*>(data);
  return std::string_view(data_cc, size);
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);

  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_EQ(rdata_obj->OptCount(), 2u);

  // Check contains
  ASSERT_TRUE(rdata_obj->ContainsOptCode(1));
  ASSERT_FALSE(rdata_obj->ContainsOptCode(30));

  // Check elements

  // Note: When passing string or std::string_view as argument, make sure to
  // construct arguments with length. Otherwise, strings containing a '\0'
  // character will be truncated.
  // https://crbug.com/1348679

  std::unique_ptr<OptRecordRdata::UnknownOpt> opt0 =
      OptRecordRdata::UnknownOpt::CreateForTesting(1,
                                                   std::string("\xde\xad", 2));
  std::unique_ptr<OptRecordRdata::UnknownOpt> opt1 =
      OptRecordRdata::UnknownOpt::CreateForTesting(
          255, std::string("\xde\xad\xbe\xef", 4));

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
  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
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
  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
  ASSERT_THAT(rdata_obj, IsNull());
}

TEST(OptRecordRdataTest, CreateEdeOpt) {
  OptRecordRdata::EdeOpt opt0(22, std::string("Don Quixote"));

  ASSERT_EQ(opt0.data(), std::string("\x00\x16"
                                     "Don Quixote",
                                     13));
  ASSERT_EQ(opt0.info_code(), 22u);
  ASSERT_EQ(opt0.extra_text(), std::string("Don Quixote"));

  std::unique_ptr<OptRecordRdata::EdeOpt> opt1 =
      OptRecordRdata::EdeOpt::Create(std::string("\x00\x08"
                                                 "Manhattan",
                                                 11));

  ASSERT_EQ(opt1->data(), std::string("\x00\x08"
                                      "Manhattan",
                                      11));
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);

  // Test Size of Query
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_EQ(rdata_obj->OptCount(), 3u);

  // Test Unknown Opt
  std::unique_ptr<OptRecordRdata::UnknownOpt> opt0 =
      OptRecordRdata::UnknownOpt::CreateForTesting(
          6, std::string("\xb0\xba\xfe\x77", 4));

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

// Test the Opt equality operator (and its subclasses as well)
TEST(OptRecordRdataTest, OptEquality) {
  // `rdata_obj0` second opt has extra text "BIOS"
  // `rdata_obj1` second opt has extra text "BIOO"
  // Note: rdata_obj0 and rdata_obj1 have 2 common Opts and 1 different one.
  OptRecordRdata rdata_obj0;
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      6, std::string("\xb0\xba\xfe\x77", 4)));
  rdata_obj0.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(13, std::string("USA", 3)));
  rdata_obj0.AddOpt(
      std::make_unique<OptRecordRdata::EdeOpt>(16, std::string("BIOS", 4)));
  ASSERT_EQ(rdata_obj0.OptCount(), 3u);

  OptRecordRdata rdata_obj1;
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      6, std::string("\xb0\xba\xfe\x77", 4)));
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
  ASSERT_THAT(rdata_obj, IsNull());
}

// Check that an EDE record with no extra text is parsed correctly.
TEST(OptRecordRdataTest, EdeRecordNoExtraText) {
  const uint8_t rdata[] = {
      0x00, 0x0F,  // OPT code (15 for EDE)
      0x00, 0x02,  // OPT data size (info code + extra text)
      0x00, 0x05   // Info Code
  };

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));
  ASSERT_EQ(rdata_obj->GetEdeOpts()[0]->data(), std::string("\x00\x05", 2));
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);
  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->GetEdeOpts(), SizeIs(1));
  auto* opt = rdata_obj->GetEdeOpts()[0];
  ASSERT_EQ(opt->data(), std::string("\x00\x44"
                                     "BOSTON",
                                     8));
  ASSERT_EQ(opt->info_code(), 68u);
  ASSERT_EQ(opt->extra_text(), std::string("BOSTON", 6));
  ASSERT_EQ(opt->GetEnumFromInfoCode(),
            OptRecordRdata::EdeOpt::EdeInfoCode::kUnrecognizedErrorCode);
}

TEST(OptRecordRdataTest, CreatePaddingOpt) {
  std::unique_ptr<OptRecordRdata::PaddingOpt> opt0 =
      std::make_unique<OptRecordRdata::PaddingOpt>(12);

  ASSERT_EQ(opt0->data(), std::string(12, '\0'));
  ASSERT_THAT(opt0->data(), SizeIs(12u));

  std::unique_ptr<OptRecordRdata::PaddingOpt> opt1 =
      std::make_unique<OptRecordRdata::PaddingOpt>("MASSACHUSETTS");

  ASSERT_EQ(opt1->data(), std::string("MASSACHUSETTS"));
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

  std::string_view rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));
  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece);

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
  rdata.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      255, std::string("\xde\xad\xbe\xef", 4)));
  EXPECT_THAT(rdata.buf(), ElementsAreArray(expected_rdata));
}

// Test the OptRecordRdata equality operator.
// Equality must be order sensitive. If Opts are same but inserted in different
// order, test will fail epically.
TEST(OptRecordRdataTest, EqualityIsOptOrderSensitive) {
  // Control rdata
  OptRecordRdata rdata_obj0;
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      1, std::string("\xb0\xba\xfe\x77", 4)));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      2, std::string("\xb1\x05\xf0\x0d", 4)));
  ASSERT_EQ(rdata_obj0.OptCount(), 2u);

  // Same as `rdata_obj0`
  OptRecordRdata rdata_obj1;
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      1, std::string("\xb0\xba\xfe\x77", 4)));
  rdata_obj1.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      2, std::string("\xb1\x05\xf0\x0d", 4)));
  ASSERT_EQ(rdata_obj1.OptCount(), 2u);

  ASSERT_EQ(rdata_obj0, rdata_obj1);

  // Same contents as `rdata_obj0` & `rdata_obj1`, but different order
  OptRecordRdata rdata_obj2;
  rdata_obj2.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      2, std::string("\xb1\x05\xf0\x0d", 4)));
  rdata_obj2.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      1, std::string("\xb0\xba\xfe\x77", 4)));
  ASSERT_EQ(rdata_obj2.OptCount(), 2u);

  // Order matters! obj0 and obj2 contain same Opts but in different order.
  ASSERT_FALSE(rdata_obj0.IsEqual(&rdata_obj2));

  // Contains only `rdata_obj0` first opt
  // 2nd opt is added later
  OptRecordRdata rdata_obj3;
  rdata_obj3.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      1, std::string("\xb0\xba\xfe\x77", 4)));
  ASSERT_EQ(rdata_obj3.OptCount(), 1u);

  ASSERT_FALSE(rdata_obj0.IsEqual(&rdata_obj3));

  rdata_obj3.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      2, std::string("\xb1\x05\xf0\x0d", 4)));

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
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      10, std::string("\x33\x33", 2)));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      5, std::string("\x11\x11", 2)));
  rdata_obj0.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
      5, std::string("\x22\x22", 2)));
  ASSERT_EQ(rdata_obj0.OptCount(), 3u);

  auto opts = rdata_obj0.GetOpts();
  ASSERT_EQ(opts[0]->data(),
            std::string("\x11\x11", 2));  // opt code 5 (inserted first)
  ASSERT_EQ(opts[1]->data(),
            std::string("\x22\x22", 2));  // opt code 5 (inserted second)
  ASSERT_EQ(opts[2]->data(), std::string("\x33\x33", 2));  // opt code 10
}

}  // namespace
}  // namespace net
