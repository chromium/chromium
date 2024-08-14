// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/record_rdata.h"

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  const unsigned first_record_len = 22;
  std::string_view record1_strpiece = MakeStringPiece(record, first_record_len);
  std::string_view record2_strpiece = MakeStringPiece(
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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

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

  DnsRecordParser parser(record, 0, /*num_records=*/0);
  std::string_view record_strpiece = MakeStringPiece(record, sizeof(record));

  std::unique_ptr<NsecRecordRdata> record_obj =
      NsecRecordRdata::Create(record_strpiece, parser);
  ASSERT_FALSE(record_obj);
}

}  // namespace
}  // namespace net
