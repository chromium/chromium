// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_parsed.h"

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
  DnsRecordParser parser(kT1ResponseDatagram, sizeof(kT1ResponseDatagram),
                         sizeof(dns_protocol::Header));
  std::unique_ptr<const RecordParsed> record;
  const CnameRecordRdata* rdata;

  parser.SkipQuestion();
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
  DnsRecordParser parser1(kT1ResponseDatagram, sizeof(kT1ResponseDatagram),
                         sizeof(dns_protocol::Header));
  parser1.SkipQuestion();
  std::unique_ptr<const RecordParsed> record1 =
      RecordParsed::CreateFrom(&parser1, base::Time());

  DnsRecordParser parser2(kT1ResponseWithCacheFlushBit,
                          sizeof(kT1ResponseWithCacheFlushBit),
                          0);

  std::unique_ptr<const RecordParsed> record2 =
      RecordParsed::CreateFrom(&parser2, base::Time());

  EXPECT_FALSE(record1->IsEqual(record2.get(), false));
  EXPECT_TRUE(record1->IsEqual(record2.get(), true));
  EXPECT_FALSE(record2->IsEqual(record1.get(), false));
  EXPECT_TRUE(record2->IsEqual(record1.get(), true));
}

}  //namespace net
