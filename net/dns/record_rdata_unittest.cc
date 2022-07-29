// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_rdata.h"

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

base::StringPiece MakeStringPiece(const std::vector<uint8_t>& vec) {
  return MakeStringPiece(vec.data(), vec.size());
}

TEST(RecordRdataTest, ParseSrvRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t
      record[] =
          {
              0x00, 0x01, 0x00, 0x02, 0x00, 0x50, 0x03, 'w',  'w',
              'w',  0x06, 'g',  'o',  'o',  'g',  'l',  'e',  0x03,
              'c',  'o',  'm',  0x00, 0x01, 0x01, 0x01, 0x02, 0x01,
              0x03, 0x04, 'w',  'w',  'w',  '2',  0xc0, 0x0a,  // Pointer to
                                                               // "google.com"
          };

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  const unsigned first_record_len = 22;
  base::StringPiece record1_strpiece = MakeStringPiece(
      record, first_record_len);
  base::StringPiece record2_strpiece = MakeStringPiece(
      record + first_record_len, sizeof(record) - first_record_len);

  std::unique_ptr<SrvRecordRdata> record1_obj =
      SrvRecordRdata::Create(record1_strpiece, parser);
  ASSERT_TRUE(record1_obj != nullptr);
  ASSERT_EQ(1, record1_obj->priority());
  ASSERT_EQ(2, record1_obj->weight());
  ASSERT_EQ(80, record1_obj->port());

  ASSERT_EQ("www.google.com", record1_obj->target());

  std::unique_ptr<SrvRecordRdata> record2_obj =
      SrvRecordRdata::Create(record2_strpiece, parser);
  ASSERT_TRUE(record2_obj != nullptr);
  ASSERT_EQ(257, record2_obj->priority());
  ASSERT_EQ(258, record2_obj->weight());
  ASSERT_EQ(259, record2_obj->port());

  ASSERT_EQ("www2.google.com", record2_obj->target());

  ASSERT_TRUE(record1_obj->IsEqual(record1_obj.get()));
  ASSERT_FALSE(record1_obj->IsEqual(record2_obj.get()));
}

TEST(RecordRdataTest, ParseARecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {
      0x7F, 0x00, 0x00, 0x01  // 127.0.0.1
  };

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<ARecordRdata> record_obj =
      ARecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("127.0.0.1", record_obj->address().ToString());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseAAAARecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {
      0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09  // 1234:5678::9A
  };

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<AAAARecordRdata> record_obj =
      AAAARecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("1234:5678::9", record_obj->address().ToString());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseCnameRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm', 0x00};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<CnameRecordRdata> record_obj =
      CnameRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("www.google.com", record_obj->cname());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParsePtrRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm', 0x00};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<PtrRecordRdata> record_obj =
      PtrRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ("www.google.com", record_obj->ptrdomain());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseTxtRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w', 'w', 'w',  0x06, 'g', 'o', 'o',
                            'g',  'l', 'e', 0x03, 'c',  'o', 'm'};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<TxtRecordRdata> record_obj =
      TxtRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  std::vector<std::string> expected;
  expected.push_back("www");
  expected.push_back("google");
  expected.push_back("com");

  ASSERT_EQ(expected, record_obj->texts());

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, ParseNsecRecord) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.

  const uint8_t record[] = {0x03, 'w',  'w',  'w',  0x06, 'g', 'o',
                            'o',  'g',  'l',  'e',  0x03, 'c', 'o',
                            'm',  0x00, 0x00, 0x02, 0x40, 0x01};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_TRUE(record_obj != nullptr);

  ASSERT_EQ(16u, record_obj->bitmap_length());

  EXPECT_FALSE(record_obj->GetBit(0));
  EXPECT_TRUE(record_obj->GetBit(1));
  for (int i = 2; i < 15; i++) {
    EXPECT_FALSE(record_obj->GetBit(i));
  }
  EXPECT_TRUE(record_obj->GetBit(15));

  ASSERT_TRUE(record_obj->IsEqual(record_obj.get()));
}

TEST(RecordRdataTest, CreateNsecRecordWithEmptyBitmapReturnsNull) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.
  // This record has a bitmap that is 0 bytes long.
  const uint8_t record[] = {0x03, 'w', 'w',  'w', 0x06, 'g', 'o',  'o',  'g',
                            'l',  'e', 0x03, 'c', 'o',  'm', 0x00, 0x00, 0x00};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_FALSE(record_obj);
}

TEST(RecordRdataTest, CreateNsecRecordWithOversizedBitmapReturnsNull) {
  // These are just the rdata portions of the DNS records, rather than complete
  // records, but it works well enough for this test.
  // This record has a bitmap that is 33 bytes long. The maximum size allowed by
  // RFC 3845, Section 2.1.2, is 32 bytes.
  const uint8_t record[] = {
      0x03, 'w',  'w',  'w',  0x06, 'g',  'o',  'o',  'g',  'l',  'e',
      0x03, 'c',  'o',  'm',  0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  DnsRecordParser parser(record, sizeof(record), 0, /*num_records=*/0);
  base::StringPiece record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_FALSE(record_obj);
}

// Test that for arbitrary IntegrityRecordRdata r, Parse(Serialize(r)) == r.
TEST(RecordRdataTest, IntegrityParseSerializeInverseProperty) {
  IntegrityRecordRdata record(IntegrityRecordRdata::Random());

  EXPECT_TRUE(record.IsIntact());
  absl::optional<std::vector<uint8_t>> serialized = record.Serialize();
  EXPECT_TRUE(serialized);

  std::unique_ptr<IntegrityRecordRdata> reparsed =
      IntegrityRecordRdata::Create(MakeStringPiece(*serialized));
  EXPECT_TRUE(reparsed);
  EXPECT_TRUE(reparsed->IsEqual(&record));
}

TEST(RecordRdataTest, IntegrityEmptyNonceCornerCase) {
  const IntegrityRecordRdata::Nonce empty_nonce;
  IntegrityRecordRdata record(empty_nonce);
  EXPECT_TRUE(record.IsIntact());

  absl::optional<std::vector<uint8_t>> serialized = record.Serialize();
  EXPECT_TRUE(serialized);
  std::unique_ptr<IntegrityRecordRdata> reparsed =
      IntegrityRecordRdata::Create(MakeStringPiece(*serialized));
  EXPECT_TRUE(reparsed);
  EXPECT_TRUE(reparsed->IsIntact());
  EXPECT_TRUE(reparsed->IsEqual(&record));
  EXPECT_EQ(reparsed->nonce().size(), 0u);
}

TEST(RecordRdataTest, IntegrityMoveConstructor) {
  IntegrityRecordRdata record_a(IntegrityRecordRdata::Random());
  EXPECT_TRUE(record_a.IsIntact());
  absl::optional<std::vector<uint8_t>> serialized_a = record_a.Serialize();
  EXPECT_TRUE(serialized_a);

  IntegrityRecordRdata record_b = std::move(record_a);
  EXPECT_TRUE(record_b.IsIntact());
  absl::optional<std::vector<uint8_t>> serialized_b = record_b.Serialize();
  EXPECT_TRUE(serialized_b);

  EXPECT_EQ(serialized_a, serialized_b);
}

TEST(RecordRdataTest, IntegrityRandomRecordsDiffer) {
  IntegrityRecordRdata record_a(IntegrityRecordRdata::Random());
  IntegrityRecordRdata record_b(IntegrityRecordRdata::Random());
  EXPECT_TRUE(!record_a.IsEqual(&record_b));
}

TEST(RecordRdataTest, IntegritySerialize) {
  IntegrityRecordRdata record({'A'});
  EXPECT_TRUE(record.IsIntact());
  const absl::optional<std::vector<uint8_t>> serialized = record.Serialize();
  EXPECT_TRUE(serialized);

  // Expected payload contains the SHA256 hash of 'A'. For the lazy:
  //   $ echo -n A | sha256sum | cut -f1 -d' ' | sed -e 's/\(..\)/0x\1, /g'
  const std::vector<uint8_t> expected = {
      0, 1, 'A',  // Length prefix and nonce
                  // Begin digest
      0x55, 0x9a, 0xea, 0xd0, 0x82, 0x64, 0xd5, 0x79, 0x5d, 0x39, 0x09, 0x71,
      0x8c, 0xdd, 0x05, 0xab, 0xd4, 0x95, 0x72, 0xe8, 0x4f, 0xe5, 0x55, 0x90,
      0xee, 0xf3, 0x1a, 0x88, 0xa0, 0x8f, 0xdf, 0xfd,  // End digest
  };

  EXPECT_TRUE(*serialized == expected);
}

TEST(RecordRdataTest, IntegrityParse) {
  const std::vector<uint8_t> serialized = {
      0,    6,    'f',  'o',  'o',  'b',  'a',  'r',  // Length prefix and nonce
      0xc3, 0xab, 0x8f, 0xf1, 0x37, 0x20, 0xe8, 0xad, 0x90,  // Begin digest
      0x47, 0xdd, 0x39, 0x46, 0x6b, 0x3c, 0x89, 0x74, 0xe5, 0x92, 0xc2,
      0xfa, 0x38, 0x3d, 0x4a, 0x39, 0x60, 0x71, 0x4c, 0xae, 0xf0, 0xc4,
      0xf2,  // End digest
  };
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_TRUE(record->IsIntact());
}

TEST(RecordRdataTest, IntegrityBadParseEmptyRdata) {
  const std::vector<uint8_t> serialized = {};
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_FALSE(record->IsIntact());
}

TEST(RecordRdataTest, IntegrityBadParseTruncatedNonce) {
  const std::vector<uint8_t> serialized = {
      0, 6, 'f', 'o', 'o'  // Length prefix and truncated nonce
  };
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_FALSE(record->IsIntact());
}

TEST(RecordRdataTest, IntegrityBadParseTruncatedDigest) {
  const std::vector<uint8_t> serialized = {
      0, 6, 'f', 'o', 'o', 'b', 'a', 'r',  // Length prefix and nonce
                                           // Begin Digest
      0xc3, 0xab, 0x8f, 0xf1, 0x37, 0x20, 0xe8, 0xad, 0x90, 0x47, 0xdd, 0x39,
      0x46, 0x6b, 0x3c, 0x89, 0x74, 0xe5, 0x92, 0xc2, 0xfa, 0x38, 0x3d,
      0x4a,  // End digest
  };
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_FALSE(record->IsIntact());
}

TEST(RecordRdataTest, IntegrityBadParseExtraBytes) {
  const std::vector<uint8_t> serialized = {
      0, 6, 'f', 'o', 'o', 'b', 'a', 'r',  // Length prefix and nonce
                                           // Begin digest
      0xc3, 0xab, 0x8f, 0xf1, 0x37, 0x20, 0xe8, 0xad, 0x90, 0x47, 0xdd, 0x39,
      0x46, 0x6b, 0x3c, 0x89, 0x74, 0xe5, 0x92, 0xc2, 0xfa, 0x38, 0x3d, 0x4a,
      0x39, 0x60, 0x71, 0x4c, 0xae, 0xf0, 0xc4, 0xf2,  // End digest
      'e', 'x', 't', 'r', 'a'                          // Trailing bytes
  };
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_FALSE(record->IsIntact());
}

TEST(RecordRdataTest, IntegrityCorruptedDigest) {
  const std::vector<uint8_t> serialized = {
      0,    6,    'f',  'o',  'o',  'b',  'a',  'r',  // Length prefix and nonce
      0xde, 0xad, 0xbe, 0xef, 0x37, 0x20, 0xe8, 0xad, 0x90,  // Begin digest
      0x47, 0xdd, 0x39, 0x46, 0x6b, 0x3c, 0x89, 0x74, 0xe5, 0x92, 0xc2,
      0xfa, 0x38, 0x3d, 0x4a, 0x39, 0x60, 0x71, 0x4c, 0xae, 0xf0, 0xc4,
      0xf2,  // End digest
  };
  auto record = IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  EXPECT_TRUE(record);
  EXPECT_FALSE(record->IsIntact());
}

}  // namespace
}  // namespace net
