// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/big_endian.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

base::StringPiece MakeStringPiece(const uint8_t* data, unsigned size) {
  const char* data_cc = reinterpret_cast<const char*>(data);
  return base::StringPiece(data_cc, size);
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

  DnsRecordParser parser(rdata, sizeof(rdata), 0, /*num_records=*/0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);

  ASSERT_THAT(rdata_obj, NotNull());
  ASSERT_THAT(rdata_obj->opts(), SizeIs(2));

  EXPECT_THAT(
      rdata_obj->opts(),
      testing::UnorderedElementsAre(
          std::make_pair(1, OptRecordRdata::Opt(1, "\xde\xad")),
          std::make_pair(255, OptRecordRdata::Opt(255, "\xde\xad\xbe\xef"))));

  ASSERT_TRUE(rdata_obj->IsEqual(rdata_obj.get()));
}

TEST(OptRecordRdataTest, ParseOptRecordWithShorterSizeThanData) {
  // This is just the rdata portion of an OPT record, rather than a complete
  // record.
  const uint8_t rdata[] = {
      0x00, 0xFF,             // OPT code
      0x00, 0x02,             // OPT data size (incorrect, should be 4)
      0xDE, 0xAD, 0xBE, 0xEF  // OPT data
  };

  DnsRecordParser parser(rdata, sizeof(rdata), 0, /*num_records=*/0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);
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

  DnsRecordParser parser(rdata, sizeof(rdata), 0, /*num_records=*/0);
  base::StringPiece rdata_strpiece = MakeStringPiece(rdata, sizeof(rdata));

  std::unique_ptr<OptRecordRdata> rdata_obj =
      OptRecordRdata::Create(rdata_strpiece, parser);
  ASSERT_THAT(rdata_obj, IsNull());
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
  rdata.AddOpt(OptRecordRdata::Opt(255, "\xde\xad\xbe\xef"));
  EXPECT_THAT(rdata.buf(), ElementsAreArray(expected_rdata));
}

}  // namespace
}  // namespace net
