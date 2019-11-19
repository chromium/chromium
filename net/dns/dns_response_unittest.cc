// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include <memory>

#include "base/big_endian.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsRecordParserTest, Constructor) {
  const char data[] = { 0 };

  EXPECT_FALSE(DnsRecordParser().IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 1, 0).IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 1, 1).IsValid());

  EXPECT_FALSE(DnsRecordParser(data, 1, 0).AtEnd());
  EXPECT_TRUE(DnsRecordParser(data, 1, 1).AtEnd());
}

TEST(DnsRecordParserTest, ReadName) {
  const uint8_t data[] = {
      // all labels "foo.example.com"
      0x03, 'f', 'o', 'o', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c',
      'o', 'm',
      // byte 0x10
      0x00,
      // byte 0x11
      // part label, part pointer, "bar.example.com"
      0x03, 'b', 'a', 'r', 0xc0, 0x04,
      // byte 0x17
      // all pointer to "bar.example.com", 2 jumps
      0xc0, 0x11,
      // byte 0x1a
  };

  std::string out;
  DnsRecordParser parser(data, sizeof(data), 0);
  ASSERT_TRUE(parser.IsValid());

  EXPECT_EQ(0x11u, parser.ReadName(data + 0x00, &out));
  EXPECT_EQ("foo.example.com", out);
  // Check that the last "." is never stored.
  out.clear();
  EXPECT_EQ(0x1u, parser.ReadName(data + 0x10, &out));
  EXPECT_EQ("", out);
  out.clear();
  EXPECT_EQ(0x6u, parser.ReadName(data + 0x11, &out));
  EXPECT_EQ("bar.example.com", out);
  out.clear();
  EXPECT_EQ(0x2u, parser.ReadName(data + 0x17, &out));
  EXPECT_EQ("bar.example.com", out);

  // Parse name without storing it.
  EXPECT_EQ(0x11u, parser.ReadName(data + 0x00, nullptr));
  EXPECT_EQ(0x1u, parser.ReadName(data + 0x10, nullptr));
  EXPECT_EQ(0x6u, parser.ReadName(data + 0x11, nullptr));
  EXPECT_EQ(0x2u, parser.ReadName(data + 0x17, nullptr));

  // Check that it works even if initial position is different.
  parser = DnsRecordParser(data, sizeof(data), 0x12);
  EXPECT_EQ(0x6u, parser.ReadName(data + 0x11, nullptr));
}

TEST(DnsRecordParserTest, ReadNameFail) {
  const uint8_t data[] = {
      // label length beyond packet
      0x30, 'x', 'x', 0x00,
      // pointer offset beyond packet
      0xc0, 0x20,
      // pointer loop
      0xc0, 0x08, 0xc0, 0x06,
      // incorrect label type (currently supports only direct and pointer)
      0x80, 0x00,
      // truncated name (missing root label)
      0x02, 'x', 'x',
  };

  DnsRecordParser parser(data, sizeof(data), 0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0u, parser.ReadName(data + 0x00, &out));
  EXPECT_EQ(0u, parser.ReadName(data + 0x04, &out));
  EXPECT_EQ(0u, parser.ReadName(data + 0x08, &out));
  EXPECT_EQ(0u, parser.ReadName(data + 0x0a, &out));
  EXPECT_EQ(0u, parser.ReadName(data + 0x0c, &out));
  EXPECT_EQ(0u, parser.ReadName(data + 0x0e, &out));
}

// Returns an RFC 1034 style domain name with a length of |name_len|.
// Also writes the expected dotted string representation into |dotted_str|,
// which must be non-null.
std::vector<uint8_t> BuildRfc1034Name(const size_t name_len,
                                      std::string* dotted_str) {
  CHECK(dotted_str != nullptr);
  auto ChoosePrintableCharLambda = [](uint8_t n) { return n % 26 + 'A'; };
  const size_t max_label_len = 63;
  std::vector<uint8_t> data;

  dotted_str->clear();
  while (data.size() < name_len) {
    // Write the null label representing the root node.
    if (data.size() == name_len - 1) {
      data.push_back(0);
      break;
    }

    // Compute the size of the next label.
    //
    // Suppose |name_len| is 8 and |data.size()| is 4. We want |label_len| to be
    // 2 so that we are correctly aligned to put 0 in the final position.
    //
    //    3  'A' 'B' 'C'  _   _   _   _
    //    0   1   2   3   4   5   6   7
    const size_t label_len =
        std::min(name_len - data.size() - 2, max_label_len);
    // Write the length octet
    data.push_back(label_len);

    // Write |label_len| bytes of label data
    const size_t size_with_label = data.size() + label_len;
    while (data.size() < size_with_label) {
      const uint8_t chr = ChoosePrintableCharLambda(data.size());
      data.push_back(chr);
      dotted_str->push_back(chr);

      CHECK(data.size() <= name_len);
    }

    // Write a trailing dot after every label
    dotted_str->push_back('.');
  }

  // Omit the final dot
  if (!dotted_str->empty())
    dotted_str->pop_back();

  CHECK(data.size() == name_len);
  return data;
}

TEST(DnsRecordParserTest, ReadNameGoodLength) {
  const size_t name_len_cases[] = {1, 10, 40, 250, 254, 255};

  for (auto name_len : name_len_cases) {
    std::string expected_name;
    const std::vector<uint8_t> data_vector =
        BuildRfc1034Name(name_len, &expected_name);
    const uint8_t* data = data_vector.data();

    DnsRecordParser parser(data, name_len, 0);
    ASSERT_TRUE(parser.IsValid());

    std::string out;
    EXPECT_EQ(name_len, parser.ReadName(data, &out));
    EXPECT_EQ(expected_name, out);
  }
}

TEST(DnsRecordParserTest, ReadNameTooLongFail) {
  const size_t name_len_cases[] = {256, 257, 258, 300, 10000};

  for (auto name_len : name_len_cases) {
    std::string expected_name;
    const std::vector<uint8_t> data_vector =
        BuildRfc1034Name(name_len, &expected_name);
    const uint8_t* data = data_vector.data();

    DnsRecordParser parser(data, name_len, 0);
    ASSERT_TRUE(parser.IsValid());

    std::string out;
    EXPECT_EQ(0u, parser.ReadName(data, &out));
  }
}

TEST(DnsRecordParserTest, ReadRecord) {
  const uint8_t data[] = {
      // Type CNAME record.
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00, 0x00,
      0x05,                    // TYPE is CNAME.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x01, 0x24, 0x74,  // TTL is 0x00012474.
      0x00, 0x06,              // RDLENGTH is 6 bytes.
      0x03, 'f', 'o', 'o',     // compressed name in record
      0xc0, 0x00,
      // Type A record.
      0x03, 'b', 'a', 'r',     // compressed owner name
      0xc0, 0x00, 0x00, 0x01,  // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x20, 0x13, 0x55,  // TTL is 0x00201355.
      0x00, 0x04,              // RDLENGTH is 4 bytes.
      0x7f, 0x02, 0x04, 0x01,  // IP is 127.2.4.1
  };

  std::string out;
  DnsRecordParser parser(data, sizeof(data), 0);

  DnsResourceRecord record;
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_EQ("example.com", record.name);
  EXPECT_EQ(dns_protocol::kTypeCNAME, record.type);
  EXPECT_EQ(dns_protocol::kClassIN, record.klass);
  EXPECT_EQ(0x00012474u, record.ttl);
  EXPECT_EQ(6u, record.rdata.length());
  EXPECT_EQ(6u, parser.ReadName(record.rdata.data(), &out));
  EXPECT_EQ("foo.example.com", out);
  EXPECT_FALSE(parser.AtEnd());

  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_EQ("bar.example.com", record.name);
  EXPECT_EQ(dns_protocol::kTypeA, record.type);
  EXPECT_EQ(dns_protocol::kClassIN, record.klass);
  EXPECT_EQ(0x00201355u, record.ttl);
  EXPECT_EQ(4u, record.rdata.length());
  EXPECT_EQ(base::StringPiece("\x7f\x02\x04\x01"), record.rdata);
  EXPECT_TRUE(parser.AtEnd());

  // Test truncated record.
  parser = DnsRecordParser(data, sizeof(data) - 2, 0);
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_FALSE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParse) {
  // This includes \0 at the end.
  const char qname_data[] = "\x0A""codereview""\x08""chromium""\x03""org";
  const base::StringPiece qname(qname_data, sizeof(qname_data));
  // Compilers want to copy when binding temporary to const &, so must use heap.
  std::unique_ptr<DnsQuery> query(
      new DnsQuery(0xcafe, qname, dns_protocol::kTypeA));

  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x81, 0x80,  // Standard query response, RA, no error
      0x00, 0x01,  // 1 question
      0x00, 0x02,  // 2 RRs (answers)
      0x00, 0x00,  // 0 authority RRs
      0x00, 0x01,  // 1 additional RRs

      // Question
      // This part is echoed back from the respective query.
      0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h',
      'r', 'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
      0x01,        // TYPE is A.
      0x00, 0x01,  // CLASS is IN.

      // Answer 1
      0xc0, 0x0c,  // NAME is a pointer to name in Question section.
      0x00, 0x05,  // TYPE is CNAME.
      0x00, 0x01,  // CLASS is IN.
      0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
      0x24, 0x74, 0x00, 0x12,  // RDLENGTH is 18 bytes.
      // ghs.l.google.com in DNS format.
      0x03, 'g', 'h', 's', 0x01, 'l', 0x06, 'g', 'o', 'o', 'g', 'l', 'e', 0x03,
      'c', 'o', 'm', 0x00,

      // Answer 2
      0xc0, 0x35,              // NAME is a pointer to name in Answer 1.
      0x00, 0x01,              // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x00,              // TTL (4 bytes) is 53 seconds.
      0x00, 0x35, 0x00, 0x04,  // RDLENGTH is 4 bytes.
      0x4a, 0x7d,              // RDATA is the IP: 74.125.95.121
      0x5f, 0x79,

      // Additional 1
      0x00,                    // NAME is empty (root domain).
      0x00, 0x29,              // TYPE is OPT.
      0x10, 0x00,              // CLASS is max UDP payload size (4096).
      0x00, 0x00, 0x00, 0x00,  // TTL (4 bytes) is rcode, version and flags.
      0x00, 0x08,              // RDLENGTH
      0x00, 0xFF,              // OPT code
      0x00, 0x04,              // OPT data size
      0xDE, 0xAD, 0xBE, 0xEF   // OPT data
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  // Reject too short.
  EXPECT_FALSE(resp.InitParse(query->io_buffer()->size() - 1, *query));
  EXPECT_FALSE(resp.IsValid());

  // Reject wrong id.
  std::unique_ptr<DnsQuery> other_query = query->CloneWithNewId(0xbeef);
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *other_query));
  EXPECT_FALSE(resp.IsValid());

  // Reject wrong question.
  std::unique_ptr<DnsQuery> wrong_query(
      new DnsQuery(0xcafe, qname, dns_protocol::kTypeCNAME));
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *wrong_query));
  EXPECT_FALSE(resp.IsValid());

  // Accept matching question.
  EXPECT_TRUE(resp.InitParse(sizeof(response_data), *query));
  EXPECT_TRUE(resp.IsValid());

  // Check header access.
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(2u, resp.answer_count());
  EXPECT_EQ(1u, resp.additional_answer_count());

  // Check question access.
  EXPECT_EQ(query->qname(), resp.qname());
  EXPECT_EQ(query->qtype(), resp.qtype());
  EXPECT_EQ("codereview.chromium.org", resp.GetDottedName());

  DnsResourceRecord record;
  DnsRecordParser parser = resp.Parser();
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_FALSE(parser.AtEnd());
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_FALSE(parser.AtEnd());
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_TRUE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParseInvalidFlags) {
  // This includes \0 at the end.
  const char qname_data[] =
      "\x0A"
      "codereview"
      "\x08"
      "chromium"
      "\x03"
      "org";
  const base::StringPiece qname(qname_data, sizeof(qname_data));
  // Compilers want to copy when binding temporary to const &, so must use heap.
  std::unique_ptr<DnsQuery> query(
      new DnsQuery(0xcafe, qname, dns_protocol::kTypeA));

  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x01, 0x80,  // RA, no error. Note the absence of the required QR bit.
      0x00, 0x01,  // 1 question
      0x00, 0x01,  // 1 RRs (answers)
      0x00, 0x00,  // 0 authority RRs
      0x00, 0x00,  // 0 additional RRs

      // Question
      // This part is echoed back from the respective query.
      0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h',
      'r', 'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
      0x01,        // TYPE is A.
      0x00, 0x01,  // CLASS is IN.

      // Answer 1
      0xc0, 0x0c,  // NAME is a pointer to name in Question section.
      0x00, 0x05,  // TYPE is CNAME.
      0x00, 0x01,  // CLASS is IN.
      0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
      0x24, 0x74, 0x00, 0x12,  // RDLENGTH is 18 bytes.
      // ghs.l.google.com in DNS format.
      0x03, 'g', 'h', 's', 0x01, 'l', 0x06, 'g', 'o', 'o', 'g', 'l', 'e', 0x03,
      'c', 'o', 'm', 0x00,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *query));
  EXPECT_FALSE(resp.IsValid());
}

TEST(DnsResponseTest, InitParseWithoutQuery) {
  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), kT0ResponseDatagram,
         sizeof(kT0ResponseDatagram));

  // Accept matching question.
  EXPECT_TRUE(resp.InitParseWithoutQuery(sizeof(kT0ResponseDatagram)));
  EXPECT_TRUE(resp.IsValid());

  // Check header access.
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(kT0RecordCount, resp.answer_count());

  // Check question access.
  EXPECT_EQ(kT0Qtype, resp.qtype());
  EXPECT_EQ(kT0HostName, resp.GetDottedName());

  DnsResourceRecord record;
  DnsRecordParser parser = resp.Parser();
  for (unsigned i = 0; i < kT0RecordCount; i ++) {
    EXPECT_FALSE(parser.AtEnd());
    EXPECT_TRUE(parser.ReadRecord(&record));
  }
  EXPECT_TRUE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParseWithoutQueryNoQuestions) {
  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x81, 0x80,  // Standard query response, RA, no error
      0x00, 0x00,  // No question
      0x00, 0x01,  // 2 RRs (answers)
      0x00, 0x00,  // 0 authority RRs
      0x00, 0x00,  // 0 additional RRs

      // Answer 1
      0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h',
      'r', 'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
      0x01,                    // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x00,              // TTL (4 bytes) is 53 seconds.
      0x00, 0x35, 0x00, 0x04,  // RDLENGTH is 4 bytes.
      0x4a, 0x7d,              // RDATA is the IP: 74.125.95.121
      0x5f, 0x79,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_TRUE(resp.InitParseWithoutQuery(sizeof(response_data)));

  // Check header access.
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(0x1u, resp.answer_count());

  DnsResourceRecord record;
  DnsRecordParser parser = resp.Parser();

  EXPECT_FALSE(parser.AtEnd());
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_EQ("codereview.chromium.org", record.name);
  EXPECT_EQ(0x00000035u, record.ttl);
  EXPECT_EQ(dns_protocol::kTypeA, record.type);

  EXPECT_TRUE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParseWithoutQueryInvalidFlags) {
  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x01, 0x80,  // RA, no error. Note the absence of the required QR bit.
      0x00, 0x00,  // No question
      0x00, 0x01,  // 2 RRs (answers)
      0x00, 0x00,  // 0 authority RRs
      0x00, 0x00,  // 0 additional RRs

      // Answer 1
      0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h',
      'r', 'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
      0x01,                    // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x00,              // TTL (4 bytes) is 53 seconds.
      0x00, 0x35, 0x00, 0x04,  // RDLENGTH is 4 bytes.
      0x4a, 0x7d,              // RDATA is the IP: 74.125.95.121
      0x5f, 0x79,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_FALSE(resp.InitParseWithoutQuery(sizeof(response_data)));
}

TEST(DnsResponseTest, InitParseWithoutQueryTwoQuestions) {
  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x81, 0x80,  // Standard query response, RA, no error
      0x00, 0x02,  // 2 questions
      0x00, 0x01,  // 2 RRs (answers)
      0x00, 0x00,  // 0 authority RRs
      0x00, 0x00,  // 0 additional RRs

      // Question 1
      0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', 0x08, 'c', 'h',
      'r', 'o', 'm', 'i', 'u', 'm', 0x03, 'o', 'r', 'g', 0x00, 0x00,
      0x01,        // TYPE is A.
      0x00, 0x01,  // CLASS is IN.

      // Question 2
      0x0b, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w', '2', 0xc0,
      0x18,        // pointer to "chromium.org"
      0x00, 0x01,  // TYPE is A.
      0x00, 0x01,  // CLASS is IN.

      // Answer 1
      0xc0, 0x0c,              // NAME is a pointer to name in Question section.
      0x00, 0x01,              // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x00,              // TTL (4 bytes) is 53 seconds.
      0x00, 0x35, 0x00, 0x04,  // RDLENGTH is 4 bytes.
      0x4a, 0x7d,              // RDATA is the IP: 74.125.95.121
      0x5f, 0x79,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_TRUE(resp.InitParseWithoutQuery(sizeof(response_data)));

  // Check header access.
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(0x01u, resp.answer_count());

  DnsResourceRecord record;
  DnsRecordParser parser = resp.Parser();

  EXPECT_FALSE(parser.AtEnd());
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_EQ("codereview.chromium.org", record.name);
  EXPECT_EQ(0x35u, record.ttl);
  EXPECT_EQ(dns_protocol::kTypeA, record.type);

  EXPECT_TRUE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParseWithoutQueryPacketTooShort) {
  const uint8_t response_data[] = {
      // Header
      0xca, 0xfe,  // ID
      0x81, 0x80,  // Standard query response, RA, no error
      0x00, 0x00,  // No question
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_FALSE(resp.InitParseWithoutQuery(sizeof(response_data)));
}

void VerifyAddressList(const std::vector<const char*>& ip_addresses,
                       const AddressList& addrlist) {
  ASSERT_EQ(ip_addresses.size(), addrlist.size());

  for (size_t i = 0; i < addrlist.size(); ++i) {
    EXPECT_EQ(ip_addresses[i], addrlist[i].ToStringWithoutPort());
  }
}

TEST(DnsResponseTest, ParseToAddressList) {
  const struct TestCase {
    size_t query_size;
    const uint8_t* response_data;
    size_t response_size;
    const char* const* expected_addresses;
    size_t num_expected_addresses;
    const char* expected_cname;
    int expected_ttl_sec;
  } cases[] = {
      {
          kT0QuerySize,
          kT0ResponseDatagram,
          base::size(kT0ResponseDatagram),
          kT0IpAddresses,
          base::size(kT0IpAddresses),
          kT0CanonName,
          kT0TTL,
      },
      {
          kT1QuerySize,
          kT1ResponseDatagram,
          base::size(kT1ResponseDatagram),
          kT1IpAddresses,
          base::size(kT1IpAddresses),
          kT1CanonName,
          kT1TTL,
      },
      {
          kT2QuerySize,
          kT2ResponseDatagram,
          base::size(kT2ResponseDatagram),
          kT2IpAddresses,
          base::size(kT2IpAddresses),
          kT2CanonName,
          kT2TTL,
      },
      {
          kT3QuerySize,
          kT3ResponseDatagram,
          base::size(kT3ResponseDatagram),
          kT3IpAddresses,
          base::size(kT3IpAddresses),
          kT3CanonName,
          kT3TTL,
      },
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    const TestCase& t = cases[i];
    DnsResponse response(t.response_data, t.response_size, t.query_size);
    AddressList addr_list;
    base::TimeDelta ttl;
    EXPECT_EQ(DnsResponse::DNS_PARSE_OK,
              response.ParseToAddressList(&addr_list, &ttl));
    std::vector<const char*> expected_addresses(
        t.expected_addresses,
        t.expected_addresses + t.num_expected_addresses);
    VerifyAddressList(expected_addresses, addr_list);
    EXPECT_EQ(t.expected_cname, addr_list.canonical_name());
    EXPECT_EQ(base::TimeDelta::FromSeconds(t.expected_ttl_sec), ttl);
  }
}

const uint8_t kResponseTruncatedRecord[] = {
    // Header: 1 question, 1 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'a', type = A, TTL = 0xFF, RDATA = 10.10.10.10
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A,  // Truncated RDATA.
};

const uint8_t kResponseTruncatedCNAME[] = {
    // Header: 1 question, 1 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'a', type = CNAME, TTL = 0xFF, RDATA = 'foo' (truncated)
    0x01, 'a', 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x03,
    0x03, 'f', 'o',  // Truncated name.
};

const uint8_t kResponseNameMismatch[] = {
    // Header: 1 question, 1 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'b', type = A, TTL = 0xFF, RDATA = 10.10.10.10
    0x01, 'b', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0A,
};

const uint8_t kResponseNameMismatchInChain[] = {
    // Header: 1 question, 3 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'a', type = CNAME, TTL = 0xFF, RDATA = 'b'
    0x01, 'a', 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x03,
    0x01, 'b', 0x00,
    // Answer: name = 'b', type = A, TTL = 0xFF, RDATA = 10.10.10.10
    0x01, 'b', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0A,
    // Answer: name = 'c', type = A, TTL = 0xFF, RDATA = 10.10.10.11
    0x01, 'c', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0B,
};

const uint8_t kResponseSizeMismatch[] = {
    // Header: 1 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = AAAA (0x1c)
    0x01, 'a', 0x00, 0x00, 0x1c, 0x00, 0x01,
    // Answer: name = 'a', type = AAAA, TTL = 0xFF, RDATA = 10.10.10.10
    0x01, 'a', 0x00, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0A,
};

const uint8_t kResponseCNAMEAfterAddress[] = {
    // Header: 2 answer RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'a', type = A, TTL = 0xFF, RDATA = 10.10.10.10.
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0A,
    // Answer: name = 'a', type = CNAME, TTL = 0xFF, RDATA = 'b'
    0x01, 'a', 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x03,
    0x01, 'b', 0x00,
};

const uint8_t kResponseNoAddresses[] = {
    // Header: 1 question, 1 answer RR, 1 authority RR
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    // Question: name = 'a', type = A (0x1)
    0x01, 'a', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer: name = 'a', type = CNAME, TTL = 0xFF, RDATA = 'b'
    0x01, 'a', 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x03,
    0x01, 'b', 0x00,
    // Authority section
    // Answer: name = 'b', type = A, TTL = 0xFF, RDATA = 10.10.10.10
    0x01, 'b', 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04,
    0x0A, 0x0A, 0x0A, 0x0A,
};

TEST(DnsResponseTest, ParseToAddressListFail) {
  const struct TestCase {
    const uint8_t* data;
    size_t size;
    DnsResponse::Result expected_result;
  } cases[] = {
      {kResponseTruncatedRecord, base::size(kResponseTruncatedRecord),
       DnsResponse::DNS_MALFORMED_RESPONSE},
      {kResponseTruncatedCNAME, base::size(kResponseTruncatedCNAME),
       DnsResponse::DNS_MALFORMED_CNAME},
      {kResponseNameMismatch, base::size(kResponseNameMismatch),
       DnsResponse::DNS_NAME_MISMATCH},
      {kResponseNameMismatchInChain, base::size(kResponseNameMismatchInChain),
       DnsResponse::DNS_NAME_MISMATCH},
      {kResponseSizeMismatch, base::size(kResponseSizeMismatch),
       DnsResponse::DNS_SIZE_MISMATCH},
      {kResponseCNAMEAfterAddress, base::size(kResponseCNAMEAfterAddress),
       DnsResponse::DNS_CNAME_AFTER_ADDRESS},
      // Not actually a failure, just an empty result.
      {kResponseNoAddresses, base::size(kResponseNoAddresses),
       DnsResponse::DNS_PARSE_OK},
  };

  const size_t kQuerySize = 12 + 7;

  for (size_t i = 0; i < base::size(cases); ++i) {
    const TestCase& t = cases[i];

    DnsResponse response(t.data, t.size, kQuerySize);
    AddressList addr_list;
    base::TimeDelta ttl;
    EXPECT_EQ(t.expected_result,
              response.ParseToAddressList(&addr_list, &ttl));
  }
}

TEST(DnsResponseWriteTest, SingleARecordAnswer) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  net::DnsResourceRecord answer;
  answer.name = "www.example.com";
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  DnsResponse response(0x1234 /* response_id */, true /* is_authoritative*/,
                       answers, {} /* authority_records */,
                       {} /* additional records */, base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleARecordAnswerWithFinalDotInName) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  net::DnsResourceRecord answer;
  answer.name = "www.example.com.";  // FQDN with the final dot.
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  DnsResponse response(0x1234 /* response_id */, true /* is_authoritative*/,
                       answers, {} /* authority_records */,
                       {} /* additional records */, base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleARecordAnswerWithQuestion) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x01,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,        // null label
      0x00, 0x01,  // type A Record
      0x00, 0x01,  // class IN
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  std::string dotted_name("www.example.com");
  std::string dns_name;
  ASSERT_TRUE(DNSDomainFromDot(dotted_name, &dns_name));
  OptRecordRdata opt_rdata;
  opt_rdata.AddOpt(OptRecordRdata::Opt(255, "\xde\xad\xbe\xef"));
  base::Optional<DnsQuery> query;
  query.emplace(0x1234 /* id */, dns_name, dns_protocol::kTypeA, &opt_rdata);
  net::DnsResourceRecord answer;
  answer.name = dotted_name;
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  DnsResponse response(0x1234 /* id */, true /* is_authoritative*/, answers,
                       {} /* authority_records */, {} /* additional records */,
                       query);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest,
     SingleAnswerWithQuestionConstructedFromSizeInflatedQuery) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x01,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,        // null label
      0x00, 0x01,  // type A Record
      0x00, 0x01,  // class IN
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  std::string dotted_name("www.example.com");
  std::string dns_name;
  ASSERT_TRUE(DNSDomainFromDot(dotted_name, &dns_name));
  size_t buf_size =
      sizeof(dns_protocol::Header) + dns_name.size() + 2 /* qtype */ +
      2 /* qclass */ +
      10 /* extra bytes that inflate the internal buffer of a query */;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(buf_size);
  memset(buf->data(), 0, buf->size());
  base::BigEndianWriter writer(buf->data(), buf_size);
  writer.WriteU16(0x1234);                              // id
  writer.WriteU16(0);                                   // flags, is query
  writer.WriteU16(1);                                   // qdcount
  writer.WriteU16(0);                                   // ancount
  writer.WriteU16(0);                                   // nscount
  writer.WriteU16(0);                                   // arcount
  writer.WriteBytes(dns_name.data(), dns_name.size());  // qname
  writer.WriteU16(dns_protocol::kTypeA);                // qtype
  writer.WriteU16(dns_protocol::kClassIN);              // qclass
  // buf contains 10 extra zero bytes.
  base::Optional<DnsQuery> query;
  query.emplace(buf);
  query->Parse(buf_size);
  net::DnsResourceRecord answer;
  answer.name = dotted_name;
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  DnsResponse response(0x1234 /* id */, true /* is_authoritative*/, answers,
                       {} /* authority_records */, {} /* additional records */,
                       query);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleQuadARecordAnswer) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e',  'x',  'a',
      'm',  'p',  'l',  'e',  0x03, 'c',  'o',  'm',
      0x00,                                            // null label
      0x00, 0x1c,                                      // type AAAA Record
      0x00, 0x01,                                      // class IN
      0x00, 0x00, 0x00, 0x78,                          // TTL, 120 seconds
      0x00, 0x10,                                      // rdlength, 128 bits
      0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  net::DnsResourceRecord answer;
  answer.name = "www.example.com";
  answer.type = dns_protocol::kTypeAAAA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string(
      "\xfd\x12\x34\x56\x78\x9a\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01", 16));
  std::vector<DnsResourceRecord> answers(1, answer);
  DnsResponse response(0x1234 /* id */, true /* is_authoritative*/, answers,
                       {} /* authority_records */, {} /* additional records */,
                       base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest,
     SingleARecordAnswerWithQuestionAndNsecAdditionalRecord) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x01,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x01,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,        // null label
      0x00, 0x01,  // type A Record
      0x00, 0x01,  // class IN
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x2f,              // type NSEC Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x05,              // rdlength, 5 bytes
      0xc0, 0x0c,              // pointer to the previous "www.example.com"
      0x00, 0x01, 0x40,        // type bit map of type A: window block 0, bitmap
                               // length 1, bitmap with bit 1 set
  };
  std::string dotted_name("www.example.com");
  std::string dns_name;
  ASSERT_TRUE(DNSDomainFromDot(dotted_name, &dns_name));
  base::Optional<DnsQuery> query;
  query.emplace(0x1234 /* id */, dns_name, dns_protocol::kTypeA);
  net::DnsResourceRecord answer;
  answer.name = dotted_name;
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  net::DnsResourceRecord additional_record;
  additional_record.name = dotted_name;
  additional_record.type = dns_protocol::kTypeNSEC;
  additional_record.klass = dns_protocol::kClassIN;
  additional_record.ttl = 120;  // 120 seconds.
  // Bitmap for "www.example.com" with type A set.
  additional_record.SetOwnedRdata(std::string("\xc0\x0c\x00\x01\x40", 5));
  std::vector<DnsResourceRecord> additional_records(1, additional_record);
  DnsResponse response(0x1234 /* id */, true /* is_authoritative*/, answers,
                       {} /* authority_records */, additional_records, query);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, TwoAnswersWithAAndQuadARecords) {
  const char response_data[] = {
      0x12, 0x34,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x02,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e',  'x',  'a',  'm',  'p', 'l', 'e',
      0x03, 'c',  'o',  'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
      0x07, 'e',  'x',  'a',  'm',  'p',  'l',  'e',  0x03, 'o', 'r', 'g',
      0x00,                                            // null label
      0x00, 0x1c,                                      // type AAAA Record
      0x00, 0x01,                                      // class IN
      0x00, 0x00, 0x00, 0x3c,                          // TTL, 60 seconds
      0x00, 0x10,                                      // rdlength, 128 bits
      0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  net::DnsResourceRecord answer1;
  answer1.name = "www.example.com";
  answer1.type = dns_protocol::kTypeA;
  answer1.klass = dns_protocol::kClassIN;
  answer1.ttl = 120;  // 120 seconds.
  answer1.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  net::DnsResourceRecord answer2;
  answer2.name = "example.org";
  answer2.type = dns_protocol::kTypeAAAA;
  answer2.klass = dns_protocol::kClassIN;
  answer2.ttl = 60;
  answer2.SetOwnedRdata(std::string(
      "\xfd\x12\x34\x56\x78\x9a\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01", 16));
  std::vector<DnsResourceRecord> answers(2);
  answers[0] = answer1;
  answers[1] = answer2;
  DnsResponse response(0x1234 /* id */, true /* is_authoritative*/, answers,
                       {} /* authority_records */, {} /* additional records */,
                       base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, AnswerWithAuthorityRecord) {
  const char response_data[] = {
      0x12, 0x35,  // ID
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x00,  // number of answer rr
      0x00, 0x01,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x00, 0x01,              // class IN
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  DnsResourceRecord record;
  record.name = "www.example.com";
  record.type = dns_protocol::kTypeA;
  record.klass = dns_protocol::kClassIN;
  record.ttl = 120;  // 120 seconds.
  record.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> authority_records(1, record);
  DnsResponse response(0x1235 /* response_id */, true /* is_authoritative*/,
                       {} /* answers */, authority_records,
                       {} /* additional records */, base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, AnswerWithRcode) {
  const char response_data[] = {
      0x12, 0x12,  // ID
      0x80, 0x03,  // flags (response with non-existent domain)
      0x00, 0x00,  // number of questions
      0x00, 0x00,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
  };
  DnsResponse response(0x1212 /* response_id */, false /* is_authoritative*/,
                       {} /* answers */, {} /* authority_records */,
                       {} /* additional records */, base::nullopt,
                       dns_protocol::kRcodeNXDOMAIN);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(response_data, sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
  EXPECT_EQ(dns_protocol::kRcodeNXDOMAIN, response.rcode());
}

// CNAME answers are always allowed for any question.
TEST(DnsResponseWriteTest, AAAAQuestionAndCnameAnswer) {
  const std::string kName = "www.example.com";
  std::string dns_name;
  ASSERT_TRUE(DNSDomainFromDot(kName, &dns_name));

  DnsResourceRecord answer;
  answer.name = kName;
  answer.type = dns_protocol::kTypeCNAME;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(dns_name);
  std::vector<DnsResourceRecord> answers(1, answer);

  base::Optional<DnsQuery> query(base::in_place, 114 /* id */, dns_name,
                                 dns_protocol::kTypeAAAA);

  DnsResponse response(114 /* response_id */, true /* is_authoritative*/,
                       answers, {} /* authority_records */,
                       {} /* additional records */, query);

  EXPECT_TRUE(response.IsValid());
}

TEST(DnsResponseWriteTest, WrittenResponseCanBeParsed) {
  std::string dotted_name("www.example.com");
  net::DnsResourceRecord answer;
  answer.name = dotted_name;
  answer.type = dns_protocol::kTypeA;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(std::string("\xc0\xa8\x00\x01", 4));
  std::vector<DnsResourceRecord> answers(1, answer);
  net::DnsResourceRecord additional_record;
  additional_record.name = dotted_name;
  additional_record.type = dns_protocol::kTypeNSEC;
  additional_record.klass = dns_protocol::kClassIN;
  additional_record.ttl = 120;  // 120 seconds.
  additional_record.SetOwnedRdata(std::string("\xc0\x0c\x00\x01\x04", 5));
  std::vector<DnsResourceRecord> additional_records(1, additional_record);
  DnsResponse response(0x1234 /* response_id */, true /* is_authoritative*/,
                       answers, {} /* authority_records */, additional_records,
                       base::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  EXPECT_EQ(1u, response.answer_count());
  EXPECT_EQ(1u, response.additional_answer_count());
  auto parser = response.Parser();
  net::DnsResourceRecord parsed_record;
  EXPECT_TRUE(parser.ReadRecord(&parsed_record));
  // Answer with an A record.
  EXPECT_EQ(answer.name, parsed_record.name);
  EXPECT_EQ(answer.type, parsed_record.type);
  EXPECT_EQ(answer.klass, parsed_record.klass);
  EXPECT_EQ(answer.ttl, parsed_record.ttl);
  EXPECT_EQ(answer.owned_rdata, parsed_record.rdata);
  // Additional NSEC record.
  EXPECT_TRUE(parser.ReadRecord(&parsed_record));
  EXPECT_EQ(additional_record.name, parsed_record.name);
  EXPECT_EQ(additional_record.type, parsed_record.type);
  EXPECT_EQ(additional_record.klass, parsed_record.klass);
  EXPECT_EQ(additional_record.ttl, parsed_record.ttl);
  EXPECT_EQ(additional_record.owned_rdata, parsed_record.rdata);
}

}  // namespace

}  // namespace net
