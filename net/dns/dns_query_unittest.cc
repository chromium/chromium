// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_query.h"

#include <tuple>

#include "base/stl_util.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using ::testing::ElementsAreArray;

std::tuple<char*, size_t> AsTuple(const IOBufferWithSize* buf) {
  return std::make_tuple(buf->data(), buf->size());
}

bool ParseAndCreateDnsQueryFromRawPacket(const uint8_t* data,
                                         size_t length,
                                         std::unique_ptr<DnsQuery>* out) {
  auto packet = base::MakeRefCounted<IOBufferWithSize>(length);
  memcpy(packet->data(), data, length);
  out->reset(new DnsQuery(packet));
  return (*out)->Parse(length);
}

// This includes \0 at the end.
const char kQNameData[] =
    "\x03"
    "www"
    "\x07"
    "example"
    "\x03"
    "com";
const base::StringPiece kQName(kQNameData, sizeof(kQNameData));

TEST(DnsQueryTest, Constructor) {
  // This includes \0 at the end.
  const uint8_t query_data[] = {
      // Header
      0xbe, 0xef, 0x01, 0x00,  // Flags -- set RD (recursion desired) bit.
      0x00, 0x01,              // Set QDCOUNT (question count) to 1, all the
                               // rest are 0 for a query.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

      // Question
      0x03, 'w', 'w', 'w',  // QNAME: www.example.com in DNS format.
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,

      0x00, 0x01,  // QTYPE: A query.
      0x00, 0x01,  // QCLASS: IN class.
  };

  DnsQuery q1(0xbeef, kQName, dns_protocol::kTypeA);
  EXPECT_EQ(dns_protocol::kTypeA, q1.qtype());
  EXPECT_THAT(AsTuple(q1.io_buffer()), ElementsAreArray(query_data));
  EXPECT_EQ(kQName, q1.qname());

  base::StringPiece question(reinterpret_cast<const char*>(query_data) + 12,
                             21);
  EXPECT_EQ(question, q1.question());
}

TEST(DnsQueryTest, Clone) {
  base::StringPiece qname(kQNameData, sizeof(kQNameData));

  DnsQuery q1(0, qname, dns_protocol::kTypeA);
  EXPECT_EQ(0, q1.id());
  std::unique_ptr<DnsQuery> q2 = q1.CloneWithNewId(42);
  EXPECT_EQ(42, q2->id());
  EXPECT_EQ(q1.io_buffer()->size(), q2->io_buffer()->size());
  EXPECT_EQ(q1.qtype(), q2->qtype());
  EXPECT_EQ(q1.question(), q2->question());
}

TEST(DnsQueryTest, EDNS0) {
  const uint8_t query_data[] = {
      // Header
      0xbe, 0xef, 0x01, 0x00,  // Flags -- set RD (recursion desired) bit.
      // Set QDCOUNT (question count) and ARCOUNT (additional count) to 1, all
      // the rest are 0 for a query.
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      // Question
      0x03, 'w', 'w', 'w',  // QNAME: www.example.com in DNS format.
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,

      0x00, 0x01,  // QTYPE: A query.
      0x00, 0x01,  // QCLASS: IN class.

      // Additional
      0x00,                    // QNAME: empty (root domain)
      0x00, 0x29,              // TYPE: OPT
      0x10, 0x00,              // CLASS: max UDP payload size
      0x00, 0x00, 0x00, 0x00,  // TTL: rcode, version and flags
      0x00, 0x08,              // RDATA length
      0x00, 0xFF,              // OPT code
      0x00, 0x04,              // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF   // OPT data
  };

  base::StringPiece qname(kQNameData, sizeof(kQNameData));
  OptRecordRdata opt_rdata;
  opt_rdata.AddOpt(OptRecordRdata::Opt(255, "\xde\xad\xbe\xef"));
  DnsQuery q1(0xbeef, qname, dns_protocol::kTypeA, &opt_rdata);
  EXPECT_EQ(dns_protocol::kTypeA, q1.qtype());

  EXPECT_THAT(AsTuple(q1.io_buffer()), ElementsAreArray(query_data));

  base::StringPiece question(reinterpret_cast<const char*>(query_data) + 12,
                             21);
  EXPECT_EQ(question, q1.question());
}

TEST(DnsQueryTest, Block128Padding) {
  DnsQuery query(46 /* id */, kQName, dns_protocol::kTypeAAAA,
                 nullptr /* opt_rdata */,
                 DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);

  // Query is expected to be short and fit in a single 128-byte padded block.
  EXPECT_EQ(128, query.io_buffer()->size());

  // Ensure created query still parses as expected.
  DnsQuery parsed_query(query.io_buffer());
  ASSERT_TRUE(parsed_query.Parse(query.io_buffer()->size()));
  EXPECT_EQ(kQName, parsed_query.qname());
  EXPECT_EQ(dns_protocol::kTypeAAAA, parsed_query.qtype());
}

TEST(DnsQueryTest, Block128Padding_LongName) {
  std::string qname;
  DNSDomainFromDot(
      "really.long.domain.name.that.will.push.us.past.the.128.byte.block.size."
      "because.it.would.be.nice.to.test.something.realy.long.like.that.com",
      &qname);
  DnsQuery query(112 /* id */, qname, dns_protocol::kTypeAAAA,
                 nullptr /* opt_rdata */,
                 DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);

  // Query is expected to pad into a second 128-byte block.
  EXPECT_EQ(256, query.io_buffer()->size());
  EXPECT_EQ(qname, query.qname());

  // Ensure created query still parses as expected.
  DnsQuery parsed_query(query.io_buffer());
  ASSERT_TRUE(parsed_query.Parse(query.io_buffer()->size()));
  EXPECT_EQ(qname, parsed_query.qname());
  EXPECT_EQ(dns_protocol::kTypeAAAA, parsed_query.qtype());
}

TEST(DnsQueryParseTest, SingleQuestionForTypeARecord) {
  const uint8_t query_data[] = {
      0x12, 0x34,  // ID
      0x00, 0x00,  // flags
      0x00, 0x01,  // number of questions
      0x00, 0x00,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w', 'w', 0x07, 'e', 'x', 'a',
      'm',  'p',  'l', 'e', 0x03, 'c', 'o', 'm',
      0x00,        // null label
      0x00, 0x01,  // type A Record
      0x00, 0x01,  // class IN
  };
  std::unique_ptr<DnsQuery> query;
  EXPECT_TRUE(ParseAndCreateDnsQueryFromRawPacket(query_data,
                                                  sizeof(query_data), &query));
  EXPECT_EQ(0x1234, query->id());
  base::StringPiece qname(kQNameData, sizeof(kQNameData));
  EXPECT_EQ(qname, query->qname());
  EXPECT_EQ(dns_protocol::kTypeA, query->qtype());
}

TEST(DnsQueryParseTest, SingleQuestionForTypeAAAARecord) {
  const uint8_t query_data[] = {
      0x12, 0x34,  // ID
      0x00, 0x00,  // flags
      0x00, 0x01,  // number of questions
      0x00, 0x00,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w', 'w', 0x07, 'e', 'x', 'a',
      'm',  'p',  'l', 'e', 0x03, 'c', 'o', 'm',
      0x00,        // null label
      0x00, 0x1c,  // type AAAA Record
      0x00, 0x01,  // class IN
  };
  std::unique_ptr<DnsQuery> query;
  EXPECT_TRUE(ParseAndCreateDnsQueryFromRawPacket(query_data,
                                                  sizeof(query_data), &query));
  EXPECT_EQ(0x1234, query->id());
  base::StringPiece qname(kQNameData, sizeof(kQNameData));
  EXPECT_EQ(qname, query->qname());
  EXPECT_EQ(dns_protocol::kTypeAAAA, query->qtype());
}

const uint8_t kQueryTruncatedQuestion[] = {
    0x12, 0x34,  // ID
    0x00, 0x00,  // flags
    0x00, 0x02,  // number of questions
    0x00, 0x00,  // number of answer rr
    0x00, 0x00,  // number of name server rr
    0x00, 0x00,  // number of additional rr
    0x03, 'w',  'w', 'w', 0x07, 'e', 'x', 'a',
    'm',  'p',  'l', 'e', 0x03, 'c', 'o', 'm',
    0x00,        // null label
    0x00, 0x01,  // type A Record
    0x00,        // class IN, truncated
};

const uint8_t kQueryTwoQuestions[] = {
    0x12, 0x34,  // ID
    0x00, 0x00,  // flags
    0x00, 0x02,  // number of questions
    0x00, 0x00,  // number of answer rr
    0x00, 0x00,  // number of name server rr
    0x00, 0x00,  // number of additional rr
    0x03, 'w',  'w', 'w', 0x07, 'e', 'x', 'a', 'm',  'p', 'l', 'e',
    0x03, 'c',  'o', 'm',
    0x00,        // null label
    0x00, 0x01,  // type A Record
    0x00, 0x01,  // class IN
    0x07, 'e',  'x', 'a', 'm',  'p', 'l', 'e', 0x03, 'o', 'r', 'g',
    0x00,        // null label
    0x00, 0x1c,  // type AAAA Record
    0x00, 0x01,  // class IN
};

const uint8_t kQueryInvalidDNSDomainName1[] = {
    0x12, 0x34,            // ID
    0x00, 0x00,            // flags
    0x00, 0x01,            // number of questions
    0x00, 0x00,            // number of answer rr
    0x00, 0x00,            // number of name server rr
    0x00, 0x00,            // number of additional rr
    0x02, 'w',  'w', 'w',  // wrong label length
    0x07, 'e',  'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm',
    0x00,        // null label
    0x00, 0x01,  // type A Record
    0x00, 0x01,  // class IN
};

const uint8_t kQueryInvalidDNSDomainName2[] = {
    0x12, 0x34,  // ID
    0x00, 0x00,  // flags
    0x00, 0x01,  // number of questions
    0x00, 0x00,  // number of answer rr
    0x00, 0x00,  // number of name server rr
    0x00, 0x00,  // number of additional rr
    0xc0, 0x02,  // illegal name pointer
    0x00, 0x01,  // type A Record
    0x00, 0x01,  // class IN
};

TEST(DnsQueryParseTest, FailsInvalidQueries) {
  const struct TestCase {
    const uint8_t* data;
    size_t size;
  } testcases[] = {
      {kQueryTruncatedQuestion, base::size(kQueryTruncatedQuestion)},
      {kQueryTwoQuestions, base::size(kQueryTwoQuestions)},
      {kQueryInvalidDNSDomainName1, base::size(kQueryInvalidDNSDomainName1)},
      {kQueryInvalidDNSDomainName2, base::size(kQueryInvalidDNSDomainName2)}};
  std::unique_ptr<DnsQuery> query;
  for (const auto& testcase : testcases) {
    EXPECT_FALSE(ParseAndCreateDnsQueryFromRawPacket(testcase.data,
                                                     testcase.size, &query));
  }
}

}  // namespace

}  // namespace net
