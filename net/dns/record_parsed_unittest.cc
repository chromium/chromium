// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_parsed.h"

#include <memory>

#include "base/time/time.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

static const uint8_t kT1ResponseWithCacheFlushBit[] = {
    0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h', 'r',
    'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
    0x05,        // TYPE is CNAME.
    0x80, 0x01,  // CLASS is IN with cache flush bit set.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x12,  // RDLENGTH is 18 bytes.
    // ghs.l.google.com in DNS format.
    0x03, 'g', 'h', 's', 0x01, 'l', 0x06, 'g', 'o', 'o', 'g', 'l', 'e', 0x03,
    'c', 'o', 'm', 0x00};

TEST(RecordParsedTest, ParseSingleRecord) {
  DnsRecordParser parser(kT1ResponseDatagram, sizeof(dns_protocol::Header),
                         kT1RecordCount);
  std::unique_ptr<const RecordParsed> record;
  const CnameRecordRdata* rdata;

  std::string dotted_qname;
  uint16_t qtype;
  parser.ReadQuestion(dotted_qname, qtype);

  record = RecordParsed::CreateFrom(&parser, base::Time());
  EXPECT_TRUE(record != nullptr);

  ASSERT_EQ("codereview.chromium.org", record->name());
  ASSERT_EQ(dns_protocol::kTypeCNAME, record->type());
  ASSERT_EQ(dns_protocol::kClassIN, record->klass());

  rdata = record->rdata<CnameRecordRdata>();
  ASSERT_TRUE(rdata != nullptr);
  ASSERT_EQ(kT1CanonName, rdata->cname());

  ASSERT_FALSE(record->rdata<SrvRecordRdata>());
  ASSERT_TRUE(record->IsEqual(record.get(), true));
}

TEST(RecordParsedTest, CacheFlushBitCompare) {
  DnsRecordParser parser1(kT1ResponseDatagram, sizeof(dns_protocol::Header),
                          kT1RecordCount);
  std::string dotted_qname;
  uint16_t qtype;
  parser1.ReadQuestion(dotted_qname, qtype);

  std::unique_ptr<const RecordParsed> record1 =
      RecordParsed::CreateFrom(&parser1, base::Time());

  DnsRecordParser parser2(kT1ResponseWithCacheFlushBit, 0, kT1RecordCount);

  std::unique_ptr<const RecordParsed> record2 =
      RecordParsed::CreateFrom(&parser2, base::Time());

  EXPECT_FALSE(record1->IsEqual(record2.get(), false));
  EXPECT_TRUE(record1->IsEqual(record2.get(), true));
  EXPECT_FALSE(record2->IsEqual(record1.get(), false));
  EXPECT_TRUE(record2->IsEqual(record1.get(), true));
}

TEST(RecordParsedTest, ParseUnknownRdata) {
  static const char kRecordData[] =
      // NAME="foo.test"
      "\003foo\004test\000"
      // TYPE=MD (an obsolete type that will likely never be recognized by
      // Chrome)
      "\000\003"
      // CLASS=IN
      "\000\001"
      // TTL=30 seconds
      "\000\000\000\036"
      // RDLENGTH=12 bytes
      "\000\014"
      // RDATA="garbage data"
      "garbage data";
  DnsRecordParser parser(base::byte_span_from_cstring(kRecordData),
                         0 /* offset */,
                         /*num_records=*/1);

  std::unique_ptr<const RecordParsed> record =
      RecordParsed::CreateFrom(&parser, base::Time());

  ASSERT_TRUE(record);
  EXPECT_EQ(record->name(), "foo.test");
  EXPECT_EQ(record->type(), 3u);
  EXPECT_EQ(record->klass(), dns_protocol::kClassIN);
  EXPECT_EQ(record->ttl(), 30u);
  EXPECT_FALSE(record->rdata<ARecordRdata>());
  EXPECT_FALSE(record->rdata_for_testing());
}

TEST(RecordParsedTest, EqualityHandlesUnknownRdata) {
  static constexpr char kData[] =
      // NAME="foo.test"
      "\003foo\004test\000"
      // TYPE=MD (an obsolete type that will likely never be recognized by
      // Chrome)
      "\000\003"
      // CLASS=IN
      "\000\001"
      // TTL=30 seconds
      "\000\000\000\036"
      // RDLENGTH=12 bytes
      "\000\014"
      // RDATA="garbage data"
      "garbage data"
      // NAME="foo.test"
      "\003foo\004test\000"
      // TYPE=A
      "\000\001"
      // CLASS=IN
      "\000\001"
      // TTL=30 seconds
      "\000\000\000\036"
      // RDLENGTH=4 bytes
      "\000\004"
      // RDATA=8.8.8.8
      "\010\010\010\010";
  DnsRecordParser parser(base::byte_span_from_cstring(kData), 0 /* offset */,
                         /*num_records=*/2);

  std::unique_ptr<const RecordParsed> unknown_record =
      RecordParsed::CreateFrom(&parser, base::Time());
  ASSERT_TRUE(unknown_record);
  ASSERT_FALSE(unknown_record->rdata_for_testing());

  std::unique_ptr<const RecordParsed> known_record =
      RecordParsed::CreateFrom(&parser, base::Time());
  ASSERT_TRUE(known_record);
  ASSERT_TRUE(known_record->rdata_for_testing());

  EXPECT_TRUE(
      unknown_record->IsEqual(unknown_record.get(), false /* is_mdns */));
  EXPECT_TRUE(
      unknown_record->IsEqual(unknown_record.get(), true /* is_mdns */));
  EXPECT_TRUE(known_record->IsEqual(known_record.get(), false /* is_mdns */));
  EXPECT_TRUE(known_record->IsEqual(known_record.get(), true /* is_mdns */));
  EXPECT_FALSE(
      unknown_record->IsEqual(known_record.get(), false /* is_mdns */));
  EXPECT_FALSE(unknown_record->IsEqual(known_record.get(), true /* is_mdns */));
  EXPECT_FALSE(
      known_record->IsEqual(unknown_record.get(), false /* is_mdns */));
  EXPECT_FALSE(known_record->IsEqual(unknown_record.get(), true /* is_mdns */));
}

TEST(RecordParsedTest, RejectMalformedRdata) {
  static const char kRecordData[] =
      // NAME="foo.test"
      "\003foo\004test\000"
      // TYPE=PTR
      "\000\014"
      // CLASS=IN
      "\000\001"
      // TTL=31 seconds
      "\000\000\000\037"
      // RDLENGTH=1 byte
      "\000\001"
      // RDATA=truncated name
      "\001";
  DnsRecordParser parser(base::byte_span_from_cstring(kRecordData),
                         0 /* offset */,
                         /*num_records=*/1);

  std::unique_ptr<const RecordParsed> record =
      RecordParsed::CreateFrom(&parser, base::Time());

  EXPECT_FALSE(record);
}

}  // namespace net
