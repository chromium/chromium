// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_query.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/opt_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using ::testing::ElementsAreArray;

std::tuple<const char*, size_t> AsTuple(const IOBufferWithSize* buf) {
  return std::make_tuple(buf->data(), buf->size());
}

bool ParseAndCreateDnsQueryFromRawPacket(const uint8_t* data,
                                         size_t length,
                                         std::unique_ptr<DnsQuery>* out) {
  auto packet = base::MakeRefCounted<IOBufferWithSize>(length);
  memcpy(packet->data(), data, length);
  *out = std::make_unique<DnsQuery>(packet);
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
const base::span<const uint8_t> kQName = base::as_byte_span(kQNameData);

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
  EXPECT_THAT(q1.qname(), ElementsAreArray(kQName));

  std::string_view question(reinterpret_cast<const char*>(query_data) + 12, 21);
  EXPECT_EQ(question, q1.question());
}

TEST(DnsQueryTest, CopiesAreIndependent) {
  DnsQuery q1(26 /* id */, kQName, dns_protocol::kTypeAAAA);

  DnsQuery q2(q1);

  EXPECT_EQ(q1.id(), q2.id());
  EXPECT_EQ(std::string_view(q1.io_buffer()->data(), q1.io_buffer()->size()),
            std::string_view(q2.io_buffer()->data(), q2.io_buffer()->size()));
  EXPECT_NE(q1.io_buffer(), q2.io_buffer());
}

TEST(DnsQueryTest, Clone) {
  DnsQuery q1(0, kQName, dns_protocol::kTypeA);
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

  OptRecordRdata opt_rdata;
  opt_rdata.AddOpt(
      OptRecordRdata::UnknownOpt::CreateForTesting(255, "\xde\xad\xbe\xef"));
  DnsQuery q1(0xbeef, kQName, dns_protocol::kTypeA, &opt_rdata);
  EXPECT_EQ(dns_protocol::kTypeA, q1.qtype());

  EXPECT_THAT(AsTuple(q1.io_buffer()), ElementsAreArray(query_data));

  std::string_view question(reinterpret_cast<const char*>(query_data) + 12, 21);
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
  EXPECT_THAT(parsed_query.qname(), ElementsAreArray(kQName));
  EXPECT_EQ(parsed_query.qtype(), dns_protocol::kTypeAAAA);
}

TEST(DnsQueryTest, Block128Padding_LongName) {
  std::optional<std::vector<uint8_t>> qname =
      dns_names_util::DottedNameToNetwork(
          "really.long.domain.name.that.will.push.us.past.the.128.byte.block."
          "size.because.it.would.be.nice.to.test.something.realy.long.like."
          "that.com");
  ASSERT_TRUE(qname.has_value());
  DnsQuery query(112 /* id */, qname.value(), dns_protocol::kTypeAAAA,
                 nullptr /* opt_rdata */,
                 DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);

  // Query is expected to pad into a second 128-byte block.
  EXPECT_EQ(query.io_buffer()->size(), 256);
  EXPECT_THAT(query.qname(), ElementsAreArray(qname.value()));

  // Ensure created query still parses as expected.
  DnsQuery parsed_query(query.io_buffer());
  ASSERT_TRUE(parsed_query.Parse(query.io_buffer()->size()));
  EXPECT_THAT(parsed_query.qname(), ElementsAreArray(qname.value()));
  EXPECT_EQ(parsed_query.qtype(), dns_protocol::kTypeAAAA);
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
  EXPECT_EQ(query->id(), 0x1234);
  EXPECT_THAT(query->qname(), ElementsAreArray(kQName));
  EXPECT_EQ(query->qtype(), dns_protocol::kTypeA);
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
  EXPECT_EQ(query->id(), 0x1234);
  EXPECT_THAT(query->qname(), ElementsAreArray(kQName));
  EXPECT_EQ(query->qtype(), dns_protocol::kTypeAAAA);
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
    raw_ptr<const uint8_t> data;
    size_t size;
  } testcases[] = {
      {kQueryTruncatedQuestion, std::size(kQueryTruncatedQuestion)},
      {kQueryTwoQuestions, std::size(kQueryTwoQuestions)},
      {kQueryInvalidDNSDomainName1, std::size(kQueryInvalidDNSDomainName1)},
      {kQueryInvalidDNSDomainName2, std::size(kQueryInvalidDNSDomainName2)}};
  std::unique_ptr<DnsQuery> query;
  for (const auto& testcase : testcases) {
    EXPECT_FALSE(ParseAndCreateDnsQueryFromRawPacket(testcase.data,
                                                     testcase.size, &query));
  }
}

TEST(DnsQueryParseTest, ParsesLongName) {
  const char kHeader[] =
      "\x6f\x15"   // ID
      "\x00\x00"   // FLAGS
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  std::string long_name;
  for (int i = 0; i <= dns_protocol::kMaxNameLength - 10; i += 10) {
    long_name.append("\x09loongname");
  }
  uint8_t remaining = dns_protocol::kMaxNameLength - long_name.size() - 1;
  long_name.append(1, remaining);
  for (int i = 0; i < remaining; ++i) {
    long_name.append("a", 1);
  }
  ASSERT_LE(long_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength));
  long_name.append("\x00", 1);

  std::string data(kHeader, sizeof(kHeader) - 1);
  data.append(long_name);
  data.append(
      "\x00\x01"   // TYPE=A
      "\x00\x01",  // CLASS=IN
      4);

  auto packet = base::MakeRefCounted<IOBufferWithSize>(data.size());
  memcpy(packet->data(), data.data(), data.size());
  DnsQuery query(packet);

  EXPECT_TRUE(query.Parse(data.size()));
}

// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsTooLongName) {
  const char kHeader[] =
      "\x5f\x15"   // ID
      "\x00\x00"   // FLAGS
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  std::string long_name;
  for (int i = 0; i <= dns_protocol::kMaxNameLength; i += 10) {
    long_name.append("\x09loongname");
  }
  ASSERT_GT(long_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength));
  long_name.append("\x00", 1);

  std::string data(kHeader, sizeof(kHeader) - 1);
  data.append(long_name);
  data.append(
      "\x00\x01"   // TYPE=A
      "\x00\x01",  // CLASS=IN
      4);

  auto packet = base::MakeRefCounted<IOBufferWithSize>(data.size());
  memcpy(packet->data(), data.data(), data.size());
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(data.size()));
}

// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsTooLongSingleLabelName) {
  const char kHeader[] =
      "\x5f\x15"   // ID
      "\x00\x00"   // FLAGS
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  std::string long_name;
  long_name.append(1, static_cast<char>(dns_protocol::kMaxNameLength));
  long_name.append(dns_protocol::kMaxNameLength, 'a');
  ASSERT_GT(long_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength));
  long_name.append("\x00", 1);

  std::string data(kHeader, sizeof(kHeader) - 1);
  data.append(long_name);
  data.append(
      "\x00\x01"   // TYPE=A
      "\x00\x01",  // CLASS=IN
      4);

  auto packet = base::MakeRefCounted<IOBufferWithSize>(data.size());
  memcpy(packet->data(), data.data(), data.size());
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(data.size()));
}

// Test that a query cannot be parsed with a name extending past the end of the
// data.
// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsNonendedName) {
  const char kData[] =
      "\x5f\x15"                    // ID
      "\x00\x00"                    // FLAGS
      "\x00\x01"                    // 1 question
      "\x00\x00"                    // 0 answers
      "\x00\x00"                    // 0 authority records
      "\x00\x00"                    // 0 additional records
      "\003www\006google\006test";  // Nonended name.

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

// Test that a query cannot be parsed with a name without final null
// termination. Parsing should assume the name has not ended and find the first
// byte of the TYPE field instead, making the actual type unparsable.
// Tests against incorrect name null termination, which is anti-pattern #4 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsNameWithoutTerminator) {
  const char kData[] =
      "\x5f\x15"                   // ID
      "\x00\x00"                   // FLAGS
      "\x00\x01"                   // 1 question
      "\x00\x00"                   // 0 answers
      "\x00\x00"                   // 0 authority records
      "\x00\x00"                   // 0 additional records
      "\003www\006google\004test"  // Name without termination.
      "\x00\x01"                   // TYPE=A
      "\x00\x01";                  // CLASS=IN

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

TEST(DnsQueryParseTest, FailsQueryWithNoQuestions) {
  const char kData[] =
      "\x5f\x15"   // ID
      "\x00\x00"   // FLAGS
      "\x00\x00"   // 0 questions
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

TEST(DnsQueryParseTest, FailsQueryWithMultipleQuestions) {
  const char kData[] =
      "\x5f\x15"                       // ID
      "\x00\x00"                       // FLAGS
      "\x00\x02"                       // 2 questions
      "\x00\x00"                       // 0 answers
      "\x00\x00"                       // 0 authority records
      "\x00\x00"                       // 0 additional records
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x01"                       // TYPE=A
      "\x00\x01"                       // CLASS=IN
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x1c"                       // TYPE=AAAA
      "\x00\x01";                      // CLASS=IN

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

// Test that if more questions are at the end of the buffer than the number of
// questions claimed in the query header, the extra questions are safely
// ignored.
TEST(DnsQueryParseTest, IgnoresExtraQuestion) {
  const char kData[] =
      "\x5f\x15"                       // ID
      "\x00\x00"                       // FLAGS
      "\x00\x01"                       // 1 question
      "\x00\x00"                       // 0 answers
      "\x00\x00"                       // 0 authority records
      "\x00\x00"                       // 0 additional records
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x01"                       // TYPE=A
      "\x00\x01"                       // CLASS=IN
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x1c"                       // TYPE=AAAA
      "\x00\x01";                      // CLASS=IN

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_TRUE(query.Parse(sizeof(kData) - 1));

  std::string expected_qname("\003www\006google\004test\000", 17);
  EXPECT_THAT(query.qname(), ElementsAreArray(expected_qname));

  EXPECT_EQ(query.qtype(), dns_protocol::kTypeA);
}

// Test that the query fails to parse if it does not contain the number of
// questions claimed in the query header.
// Tests against incorrect record count field validation, which is anti-pattern
// #5 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsQueryWithMissingQuestion) {
  const char kData[] =
      "\x5f\x15"   // ID
      "\x00\x00"   // FLAGS
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

// Test that DnsQuery parsing disallows name compression pointers (which should
// never be useful when only single-question queries are parsed).
// Indirectly tests against incorrect name compression pointer validation, which
// is anti-pattern #6 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsQueryParseTest, FailsQueryWithNamePointer) {
  const char kData[] =
      "\x5f\x15"                   // ID
      "\x00\x00"                   // FLAGS
      "\x00\x01"                   // 1 question
      "\x00\x00"                   // 0 answers
      "\x00\x00"                   // 0 authority records
      "\x00\x00"                   // 0 additional records
      "\003www\006google\300\035"  // Name with pointer to byte 29
      "\x00\x01"                   // TYPE=A
      "\x00\x01"                   // CLASS=IN
      "\004test\000";              // Byte 29 (name pointer destination): test.

  auto packet = base::MakeRefCounted<IOBufferWithSize>(sizeof(kData) - 1);
  memcpy(packet->data(), kData, sizeof(kData) - 1);
  DnsQuery query(packet);

  EXPECT_FALSE(query.Parse(sizeof(kData) - 1));
}

}  // namespace

}  // namespace net
