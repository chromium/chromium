// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_response.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsRecordParserTest, Constructor) {
  const uint8_t data[] = {0};

  EXPECT_FALSE(DnsRecordParser().IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 0, 0).IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 1, 0).IsValid());

  EXPECT_FALSE(DnsRecordParser(data, 0, 0).AtEnd());
  EXPECT_TRUE(DnsRecordParser(data, 1, 0).AtEnd());
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
  DnsRecordParser parser(data, 0, /*num_records=*/0);
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
  parser = DnsRecordParser(data, 0x12, /*num_records=*/0);
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

  DnsRecordParser parser(data, 0, /*num_records=*/0);
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
  // Impossible length. If length not zero, need at least 2 to allow label
  // length and label contents.
  CHECK_NE(name_len, 1u);

  CHECK(dotted_str != nullptr);
  auto ChoosePrintableCharLambda = [](uint8_t n) { return n % 26 + 'A'; };
  const size_t max_label_len = 63;
  std::vector<uint8_t> data;

  dotted_str->clear();
  while (data.size() < name_len) {
    // Compute the size of the next label.
    //
    // No need to account for next label length because the final zero length is
    // not considered included in overall length.
    size_t label_len = std::min(name_len - data.size() - 1, max_label_len);
    // Need to ensure the remainder is not 1 because that would leave room for a
    // label length but not a label.
    if (name_len - data.size() - label_len - 1 == 1) {
      CHECK_GT(label_len, 1u);
      label_len -= 1;
    }

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

  // Final zero-length label (not considered included in overall length).
  data.push_back(0);

  return data;
}

TEST(DnsRecordParserTest, ReadNameGoodLength) {
  const size_t name_len_cases[] = {2, 10, 40, 250, 254, 255};

  for (auto name_len : name_len_cases) {
    std::string expected_name;
    const std::vector<uint8_t> data_vector =
        BuildRfc1034Name(name_len, &expected_name);
    ASSERT_EQ(data_vector.size(), name_len + 1);
    const uint8_t* data = data_vector.data();

    DnsRecordParser parser(data_vector, 0, /*num_records=*/0);
    ASSERT_TRUE(parser.IsValid());

    std::string out;
    EXPECT_EQ(data_vector.size(), parser.ReadName(data, &out));
    EXPECT_EQ(expected_name, out);
  }
}

// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, ReadNameTooLongFail) {
  const size_t name_len_cases[] = {256, 257, 258, 300, 10000};

  for (auto name_len : name_len_cases) {
    std::string expected_name;
    const std::vector<uint8_t> data_vector =
        BuildRfc1034Name(name_len, &expected_name);
    ASSERT_EQ(data_vector.size(), name_len + 1);
    const uint8_t* data = data_vector.data();

    DnsRecordParser parser(data_vector, 0, /*num_records=*/0);
    ASSERT_TRUE(parser.IsValid());

    std::string out;
    EXPECT_EQ(0u, parser.ReadName(data, &out));
  }
}

// Tests against incorrect name compression pointer validation, which is anti-
// pattern #6 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectsNamesWithLoops) {
  const char kData[] =
      "\003www\007example\300\031"  // www.example with pointer to byte 25
      "aaaaaaaaaaa"                 // Garbage data to spread things out.
      "\003foo\300\004";            // foo with pointer to byte 4.

  DnsRecordParser parser(base::byte_span_from_cstring(kData), /*offset=*/0,
                         /*num_records=*/0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0u, parser.ReadName(kData, &out));
}

// Tests against incorrect name compression pointer validation, which is anti-
// pattern #6 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectsNamesPointingOutsideData) {
  const char kData[] =
      "\003www\007example\300\031";  // www.example with pointer to byte 25

  DnsRecordParser parser(base::byte_span_from_cstring(kData), /*offset=*/0,
                         /*num_records=*/0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0u, parser.ReadName(kData, &out));
}

TEST(DnsRecordParserTest, ParsesValidPointer) {
  const char kData[] =
      "\003www\007example\300\022"  // www.example with pointer to byte 25.
      "aaaa"                        // Garbage data to spread things out.
      "\004test\000";               // .test

  DnsRecordParser parser(base::byte_span_from_cstring(kData), /*offset=*/0,
                         /*num_records=*/0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(14u, parser.ReadName(kData, &out));
  EXPECT_EQ(out, "www.example.test");
}

// Per RFC 1035, section 4.1.4, the first 2 bits of a DNS name label determine
// if it is a length label (if the bytes are 00) or a pointer label (if the
// bytes are 11). It is a common DNS parsing bug to treat 01 or 10 as pointer
// labels, but these are reserved and invalid. Such labels should always result
// in DnsRecordParser rejecting the name.
//
// Tests against incorrect name compression pointer validation, which is anti-
// pattern #6 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectsNamesWithInvalidLabelTypeAsPointer) {
  const char kData[] =
      "\003www\007example\200\022"  // www.example with invalid label as pointer
      "aaaa"                        // Garbage data to spread things out.
      "\004test\000";               // .test

  DnsRecordParser parser(base::byte_span_from_cstring(kData), /*offset=*/0,
                         /*num_records=*/0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0u, parser.ReadName(kData, &out));
}

// Per RFC 1035, section 4.1.4, the first 2 bits of a DNS name label determine
// if it is a length label (if the bytes are 00) or a pointer label (if the
// bytes are 11). Such labels should always result in DnsRecordParser rejecting
// the name.
//
// Tests against incorrect name compression pointer validation, which is anti-
// pattern #6 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectsNamesWithInvalidLabelTypeAsLength) {
  const char kData[] =
      "\003www\007example\104"  // www.example with invalid label as length
      "test\000";  // test. (in case \104 is interpreted as length=4)

  // Append a bunch of zeroes to the buffer in case \104 is interpreted as a
  // long length.
  std::string data(kData, sizeof(kData) - 1);
  data.append(256, '\000');

  DnsRecordParser parser(base::as_byte_span(data), /*offset=*/0,
                         /*num_records=*/0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0u, parser.ReadName(data.data(), &out));
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
  DnsRecordParser parser(data, 0, /*num_records=*/2);

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
  EXPECT_EQ(std::string_view("\x7f\x02\x04\x01"), record.rdata);
  EXPECT_TRUE(parser.AtEnd());

  // Test truncated record.
  auto span = base::span(data);
  parser = DnsRecordParser(span.first(span.size() - 2), 0, /*num_records=*/2);
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_FALSE(parser.AtEnd());
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsRecordParserTest, ReadsRecordWithLongName) {
  std::string dotted_name;
  const std::vector<uint8_t> dns_name =
      BuildRfc1034Name(dns_protocol::kMaxNameLength, &dotted_name);

  std::string data(reinterpret_cast<const char*>(dns_name.data()),
                   dns_name.size());
  data.append(
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01",  // 192.168.0.1
      14);

  DnsRecordParser parser(base::as_byte_span(data), 0, /*num_records=*/1);

  DnsResourceRecord record;
  EXPECT_TRUE(parser.ReadRecord(&record));
}

// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectRecordWithTooLongName) {
  std::string dotted_name;
  const std::vector<uint8_t> dns_name =
      BuildRfc1034Name(dns_protocol::kMaxNameLength + 1, &dotted_name);

  std::string data(reinterpret_cast<const char*>(dns_name.data()),
                   dns_name.size());
  data.append(
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01",  // 192.168.0.1
      14);

  DnsRecordParser parser(base::as_byte_span(data), 0, /*num_records=*/1);

  DnsResourceRecord record;
  EXPECT_FALSE(parser.ReadRecord(&record));
}

// Test that a record cannot be parsed with a name extending past the end of the
// data.
// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectRecordWithNonendedName) {
  const char kNonendedName[] = "\003www\006google\006www";

  DnsRecordParser parser(base::byte_span_from_cstring(kNonendedName), 0,
                         /*num_records=*/1);

  DnsResourceRecord record;
  EXPECT_FALSE(parser.ReadRecord(&record));
}

// Test that a record cannot be parsed with a name without final null
// termination. Parsing should assume the name has not ended and find the first
// byte of the TYPE field instead, making the remainder of the record
// unparsable.
// Tests against incorrect name null termination, which is anti-pattern #4 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsRecordParserTest, RejectRecordNameMissingNullTermination) {
  const char kData[] =
      "\003www\006google\004test"  // Name without termination.
      "\x00\x01"                   // TYPE=A
      "\x00\x01"                   // CLASS=IN
      "\x00\x01\x51\x80"           // TTL=1 day
      "\x00\x04"                   // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01";          // 192.168.0.1

  DnsRecordParser parser(base::byte_span_from_cstring(kData), 0,
                         /*num_records=*/1);

  DnsResourceRecord record;
  EXPECT_FALSE(parser.ReadRecord(&record));
}

// Test that no more records can be parsed once the claimed number of records
// have been parsed.
TEST(DnsRecordParserTest, RejectReadingTooManyRecords) {
  const char kData[] =
      "\003www\006google\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01"  // 192.168.0.1
      "\003www\010chromium\004test\000"
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x02";  // 192.168.0.2

  DnsRecordParser parser(
      base::byte_span_from_cstring(kData), /*offset=*/0,
      /*num_records=*/1);  // Claim 1 record despite there being 2 in `kData`.

  DnsResourceRecord record1;
  EXPECT_TRUE(parser.ReadRecord(&record1));

  // Expect second record cannot be parsed because only 1 was expected.
  DnsResourceRecord record2;
  EXPECT_FALSE(parser.ReadRecord(&record2));
}

// Test that no more records can be parsed once the end of the buffer is
// reached, even if more records are claimed.
TEST(DnsRecordParserTest, RejectReadingPastEnd) {
  const char kData[] =
      "\003www\006google\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01"  // 192.168.0.1
      "\003www\010chromium\004test\000"
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x02";  // 192.168.0.2

  DnsRecordParser parser(
      base::byte_span_from_cstring(kData), /*offset=*/0,
      /*num_records=*/3);  // Claim 3 record despite there being 2 in `kData`.

  DnsResourceRecord record;
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_TRUE(parser.ReadRecord(&record));
  EXPECT_FALSE(parser.ReadRecord(&record));
}

TEST(DnsResponseTest, InitParse) {
  // This includes \0 at the end.
  const char qname[] =
      "\x0A"
      "codereview"
      "\x08"
      "chromium"
      "\x03"
      "org";
  // Compilers want to copy when binding temporary to const &, so must use heap.
  auto query = std::make_unique<DnsQuery>(0xcafe, base::as_byte_span(qname),
                                          dns_protocol::kTypeA);

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

  EXPECT_FALSE(resp.id());

  // Reject too short.
  EXPECT_FALSE(resp.InitParse(query->io_buffer()->size() - 1, *query));
  EXPECT_FALSE(resp.IsValid());
  EXPECT_FALSE(resp.id());

  // Reject wrong id.
  std::unique_ptr<DnsQuery> other_query = query->CloneWithNewId(0xbeef);
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *other_query));
  EXPECT_FALSE(resp.IsValid());
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));

  // Reject wrong question.
  auto wrong_query = std::make_unique<DnsQuery>(
      0xcafe, base::as_byte_span(qname), dns_protocol::kTypeCNAME);
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *wrong_query));
  EXPECT_FALSE(resp.IsValid());
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));

  // Accept matching question.
  EXPECT_TRUE(resp.InitParse(sizeof(response_data), *query));
  EXPECT_TRUE(resp.IsValid());

  // Check header access.
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(2u, resp.answer_count());
  EXPECT_EQ(1u, resp.additional_answer_count());

  // Check question access.
  std::optional<std::vector<uint8_t>> response_qname =
      dns_names_util::DottedNameToNetwork(resp.GetSingleDottedName());
  ASSERT_TRUE(response_qname.has_value());
  EXPECT_THAT(query->qname(),
              testing::ElementsAreArray(response_qname.value()));
  EXPECT_EQ(query->qtype(), resp.GetSingleQType());
  EXPECT_EQ("codereview.chromium.org", resp.GetSingleDottedName());

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
  const char qname[] =
      "\x0A"
      "codereview"
      "\x08"
      "chromium"
      "\x03"
      "org";
  // Compilers want to copy when binding temporary to const &, so must use heap.
  auto query = std::make_unique<DnsQuery>(0xcafe, base::as_byte_span(qname),
                                          dns_protocol::kTypeA);

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
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));
}

TEST(DnsResponseTest, InitParseRejectsResponseWithoutQuestions) {
  const char kResponse[] =
      "\x02\x45"                       // ID=581
      "\x81\x80"                       // Standard query response, RA, no error
      "\x00\x00"                       // 0 questions
      "\x00\x01"                       // 1 answers
      "\x00\x00"                       // 0 authority records
      "\x00\x00"                       // 0 additional records
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x01"                       // TYPE=A
      "\x00\x01"                       // CLASS=IN
      "\x00\x00\x2a\x30"               // TTL=3 hours
      "\x00\x04"                       // RDLENGTH=4 bytes
      "\xa0\xa0\xa0\xa0";              // 10.10.10.10

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  // Validate that the response is fine if not matching against a query.
  ASSERT_TRUE(resp.InitParseWithoutQuery(sizeof(kResponse) - 1));

  const char kQueryName[] = "\003www\006google\004test";
  DnsQuery query(
      /*id=*/581, base::as_byte_span(kQueryName), dns_protocol::kTypeA);
  EXPECT_FALSE(resp.InitParse(sizeof(kResponse) - 1, query));
}

TEST(DnsResponseTest, InitParseRejectsResponseWithTooManyQuestions) {
  const char kResponse[] =
      "\x02\x46"                       // ID=582
      "\x81\x80"                       // Standard query response, RA, no error
      "\x00\x02"                       // 2 questions
      "\x00\x00"                       // 0 answers
      "\x00\x00"                       // 0 authority records
      "\x00\x00"                       // 0 additional records
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x01"                       // TYPE=A
      "\x00\x01"                       // CLASS=IN
      "\003www\010chromium\004test\000"  // www.chromium.test
      "\x00\x01"                         // TYPE=A
      "\x00\x01";                        // CLASS=IN

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  // Validate that the response is fine if not matching against a query.
  ASSERT_TRUE(resp.InitParseWithoutQuery(sizeof(kResponse) - 1));

  const char kQueryName[] = "\003www\006google\004test";
  DnsQuery query(
      /*id=*/582, base::as_byte_span(kQueryName), dns_protocol::kTypeA);
  EXPECT_FALSE(resp.InitParse(sizeof(kResponse) - 1, query));
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
  EXPECT_EQ(kT0Qtype, resp.GetSingleQType());
  EXPECT_EQ(kT0HostName, resp.GetSingleDottedName());

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
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(0u, resp.question_count());
  EXPECT_EQ(0x1u, resp.answer_count());

  EXPECT_THAT(resp.dotted_qnames(), testing::IsEmpty());
  EXPECT_THAT(resp.qtypes(), testing::IsEmpty());

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
  EXPECT_THAT(resp.id(), testing::Optional(0xcafe));
}

TEST(DnsResponseTest, InitParseWithoutQueryTwoQuestions) {
  const uint8_t response_data[] = {
      // Header
      0xca,
      0xfe,  // ID
      0x81,
      0x80,  // Standard query response, RA, no error
      0x00,
      0x02,  // 2 questions
      0x00,
      0x01,  // 2 RRs (answers)
      0x00,
      0x00,  // 0 authority RRs
      0x00,
      0x00,  // 0 additional RRs

      // Question 1
      0x0a,
      'c',
      'o',
      'd',
      'e',
      'r',
      'e',
      'v',
      'i',
      'e',
      'w',
      0x08,
      'c',
      'h',
      'r',
      'o',
      'm',
      'i',
      'u',
      'm',
      0x03,
      'o',
      'r',
      'g',
      0x00,
      0x00,
      0x01,  // TYPE is A.
      0x00,
      0x01,  // CLASS is IN.

      // Question 2
      0x0b,
      'c',
      'o',
      'd',
      'e',
      'r',
      'e',
      'v',
      'i',
      'e',
      'w',
      '2',
      0xc0,
      0x17,  // pointer to "chromium.org"
      0x00,
      0x01,  // TYPE is A.
      0x00,
      0x01,  // CLASS is IN.

      // Answer 1
      0xc0,
      0x0c,  // NAME is a pointer to name in Question section.
      0x00,
      0x01,  // TYPE is A.
      0x00,
      0x01,  // CLASS is IN.
      0x00,
      0x00,  // TTL (4 bytes) is 53 seconds.
      0x00,
      0x35,
      0x00,
      0x04,  // RDLENGTH is 4 bytes.
      0x4a,
      0x7d,  // RDATA is the IP: 74.125.95.121
      0x5f,
      0x79,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  EXPECT_TRUE(resp.InitParseWithoutQuery(sizeof(response_data)));

  // Check header access.
  EXPECT_EQ(0x8180, resp.flags());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(2u, resp.question_count());
  EXPECT_EQ(0x01u, resp.answer_count());

  EXPECT_THAT(resp.dotted_qnames(),
              testing::ElementsAre("codereview.chromium.org",
                                   "codereview2.chromium.org"));
  EXPECT_THAT(resp.qtypes(),
              testing::ElementsAre(dns_protocol::kTypeA, dns_protocol::kTypeA));

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

TEST(DnsResponseTest, InitParseAllowsQuestionWithLongName) {
  const char kResponseHeader[] =
      "\x02\x45"   // ID=581
      "\x81\x80"   // Standard query response, RA, no error
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  std::string dotted_name;
  const std::vector<uint8_t> dns_name =
      BuildRfc1034Name(dns_protocol::kMaxNameLength, &dotted_name);

  std::string response_data(kResponseHeader, sizeof(kResponseHeader) - 1);
  response_data.append(reinterpret_cast<const char*>(dns_name.data()),
                       dns_name.size());
  response_data.append(
      "\x00\x01"   // TYPE=A
      "\x00\x01",  // CLASS=IN)
      4);

  DnsResponse resp1;
  memcpy(resp1.io_buffer()->data(), response_data.data(), response_data.size());

  EXPECT_TRUE(resp1.InitParseWithoutQuery(response_data.size()));

  DnsQuery query(581, dns_name, dns_protocol::kTypeA);

  DnsResponse resp2(resp1.io_buffer(), response_data.size());
  EXPECT_TRUE(resp2.InitParse(response_data.size(), query));
}

// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsResponseTest, InitParseRejectsQuestionWithTooLongName) {
  const char kResponseHeader[] =
      "\x02\x45"   // ID=581
      "\x81\x80"   // Standard query response, RA, no error
      "\x00\x01"   // 1 question
      "\x00\x00"   // 0 answers
      "\x00\x00"   // 0 authority records
      "\x00\x00";  // 0 additional records

  std::string dotted_name;
  const std::vector<uint8_t> dns_name =
      BuildRfc1034Name(dns_protocol::kMaxNameLength + 1, &dotted_name);

  std::string response_data(kResponseHeader, sizeof(kResponseHeader) - 1);
  response_data.append(reinterpret_cast<const char*>(dns_name.data()),
                       dns_name.size());
  response_data.append(
      "\x00\x01"   // TYPE=A
      "\x00\x01",  // CLASS=IN)
      4);

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data.data(), response_data.size());

  EXPECT_FALSE(resp.InitParseWithoutQuery(response_data.size()));

  // Note that `DnsQuery` disallows construction without a valid name, so
  // `InitParse()` can never be tested with a `query` that matches against a
  // too-long name in the response. Test with an arbitrary valid query name to
  // ensure no issues if this code is exercised after receiving a response with
  // a too-long name.
  const char kQueryName[] = "\005query\004test";
  DnsQuery query(
      /*id=*/581, base::as_byte_span(kQueryName), dns_protocol::kTypeA);
  EXPECT_FALSE(resp.InitParse(response_data.size(), query));
}

// Test that `InitParse[...]()` rejects a response with a question name
// extending past the end of the response.
// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsResponseTest, InitParseRejectsQuestionWithNonendedName) {
  const char kResponse[] =
      "\x02\x45"                    // ID
      "\x81\x80"                    // Standard query response, RA, no error
      "\x00\x01"                    // 1 question
      "\x00\x00"                    // 0 answers
      "\x00\x00"                    // 0 authority records
      "\x00\x00"                    // 0 additional records
      "\003www\006google\006test";  // Name extending past the end.

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  EXPECT_FALSE(resp.InitParseWithoutQuery(sizeof(kResponse) - 1));

  const char kQueryName[] = "\003www\006google\006testtt";
  DnsQuery query(
      /*id=*/581, base::as_byte_span(kQueryName), dns_protocol::kTypeA);
  EXPECT_FALSE(resp.InitParse(sizeof(kResponse) - 1, query));
}

// Test that `InitParse[...]()` rejects responses that do not contain at least
// the claimed number of questions.
// Tests against incorrect record count field validation, which is anti-pattern
// #5 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsResponseTest, InitParseRejectsResponseWithMissingQuestions) {
  const char kResponse[] =
      "\x02\x45"                       // ID
      "\x81\x80"                       // Standard query response, RA, no error
      "\x00\x03"                       // 3 questions
      "\x00\x00"                       // 0 answers
      "\x00\x00"                       // 0 authority records
      "\x00\x00"                       // 0 additional records
      "\003www\006google\004test\000"  // www.google.test
      "\x00\x01"                       // TYPE=A
      "\x00\x01"                       // CLASS=IN
      "\003www\010chromium\004test\000"  // www.chromium.test
      "\x00\x01"                         // TYPE=A
      "\x00\x01";                        // CLASS=IN
  // Missing third question.

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  EXPECT_FALSE(resp.InitParseWithoutQuery(sizeof(kResponse) - 1));

  const char kQueryName[] = "\003www\006google\004test";
  DnsQuery query(
      /*id=*/581, base::as_byte_span(kQueryName), dns_protocol::kTypeA);
  EXPECT_FALSE(resp.InitParse(sizeof(kResponse) - 1, query));
}

// Test that a parsed DnsResponse only allows parsing the number of records
// claimed in the response header.
// Tests against incorrect record count field validation, which is anti-pattern
// #5 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsResponseTest, ParserLimitedToNumClaimedRecords) {
  const char kResponse[] =
      "\x02\x45"  // ID
      "\x81\x80"  // Standard query response, RA, no error
      "\x00\x01"  // 1 question
      "\x00\x01"  // 1 answers
      "\x00\x02"  // 2 authority records
      "\x00\x01"  // 1 additional records
      "\003www\006google\004test\000"
      "\x00\x01"  // TYPE=A
      "\x00\x01"  // CLASS=IN
      // 6 total records.
      "\003www\006google\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01"  // 192.168.0.1
      "\003www\010chromium\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x02"  // 192.168.0.2
      "\003www\007google1\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x03"  // 192.168.0.3
      "\003www\011chromium1\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x04"  // 192.168.0.4
      "\003www\007google2\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x05"  // 192.168.0.5
      "\003www\011chromium2\004test\000"
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x06";  // 192.168.0.6

  DnsResponse resp1;
  memcpy(resp1.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  ASSERT_TRUE(resp1.InitParseWithoutQuery(sizeof(kResponse) - 1));
  DnsRecordParser parser1 = resp1.Parser();
  ASSERT_TRUE(parser1.IsValid());

  // Response header only claims 4 records, so expect parser to only allow
  // parsing that many, ignoring extra records in the data.
  DnsResourceRecord record;
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_FALSE(parser1.ReadRecord(&record));
  EXPECT_FALSE(parser1.ReadRecord(&record));

  // Repeat using InitParse()
  DnsResponse resp2;
  memcpy(resp2.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  const char kQueryName[] = "\003www\006google\004test";
  DnsQuery query(
      /*id=*/581, base::as_byte_span(kQueryName), dns_protocol::kTypeA);

  ASSERT_TRUE(resp2.InitParse(sizeof(kResponse) - 1, query));
  DnsRecordParser parser2 = resp2.Parser();
  ASSERT_TRUE(parser2.IsValid());

  // Response header only claims 4 records, so expect parser to only allow
  // parsing that many, ignoring extra records in the data.
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_FALSE(parser2.ReadRecord(&record));
  EXPECT_FALSE(parser2.ReadRecord(&record));
}

// Test that a parsed DnsResponse does not allow parsing past the end of the
// input, even if more records are claimed in the response header.
// Tests against incorrect record count field validation, which is anti-pattern
// #5 from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST(DnsResponseTest, ParserLimitedToBufferSize) {
  const char kResponse[] =
      "\x02\x45"  // ID
      "\x81\x80"  // Standard query response, RA, no error
      "\x00\x01"  // 1 question
      "\x00\x01"  // 1 answers
      "\x00\x02"  // 2 authority records
      "\x00\x01"  // 1 additional records
      "\003www\006google\004test\000"
      "\x00\x01"  // TYPE=A
      "\x00\x01"  // CLASS=IN
      // 2 total records.
      "\003www\006google\004test\000"
      "\x00\x01"          // TYPE=A
      "\x00\x01"          // CLASS=IN
      "\x00\x01\x51\x80"  // TTL=1 day
      "\x00\x04"          // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x01"  // 192.168.0.1
      "\003www\010chromium\004test\000"
      "\x00\x01"           // TYPE=A
      "\x00\x01"           // CLASS=IN
      "\x00\x01\x51\x80"   // TTL=1 day
      "\x00\x04"           // RDLENGTH=4 bytes
      "\xc0\xa8\x00\x02";  // 192.168.0.2

  DnsResponse resp1;
  memcpy(resp1.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  ASSERT_TRUE(resp1.InitParseWithoutQuery(sizeof(kResponse) - 1));
  DnsRecordParser parser1 = resp1.Parser();
  ASSERT_TRUE(parser1.IsValid());

  // Response header claims 4 records, but only 2 present in input.
  DnsResourceRecord record;
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_TRUE(parser1.ReadRecord(&record));
  EXPECT_FALSE(parser1.ReadRecord(&record));
  EXPECT_FALSE(parser1.ReadRecord(&record));

  // Repeat using InitParse()
  DnsResponse resp2;
  memcpy(resp2.io_buffer()->data(), kResponse, sizeof(kResponse) - 1);

  ASSERT_TRUE(resp2.InitParseWithoutQuery(sizeof(kResponse) - 1));
  DnsRecordParser parser2 = resp2.Parser();
  ASSERT_TRUE(parser2.IsValid());

  // Response header claims 4 records, but only 2 present in input.
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_TRUE(parser2.ReadRecord(&record));
  EXPECT_FALSE(parser2.ReadRecord(&record));
  EXPECT_FALSE(parser2.ReadRecord(&record));
}

TEST(DnsResponseWriteTest, SingleARecordAnswer) {
  const uint8_t response_data[] = {
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
                       {} /* additional records */, std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleARecordAnswerWithFinalDotInName) {
  const uint8_t response_data[] = {
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
                       {} /* additional records */, std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleARecordAnswerWithQuestion) {
  const uint8_t response_data[] = {
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
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(dotted_name);
  ASSERT_TRUE(dns_name.has_value());

  OptRecordRdata opt_rdata;
  opt_rdata.AddOpt(
      OptRecordRdata::UnknownOpt::CreateForTesting(255, "\xde\xad\xbe\xef"));

  std::optional<DnsQuery> query;
  query.emplace(0x1234 /* id */, dns_name.value(), dns_protocol::kTypeA,
                &opt_rdata);
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
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest,
     SingleAnswerWithQuestionConstructedFromSizeInflatedQuery) {
  const uint8_t response_data[] = {
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
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(dotted_name);
  ASSERT_TRUE(dns_name.has_value());
  size_t buf_size =
      sizeof(dns_protocol::Header) + dns_name.value().size() + 2 /* qtype */ +
      2 /* qclass */ +
      10 /* extra bytes that inflate the internal buffer of a query */;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(buf_size);
  std::ranges::fill(buf->span(), 0);
  auto writer = base::SpanWriter(buf->span());
  writer.WriteU16BigEndian(0x1234);                  // id
  writer.WriteU16BigEndian(0);                       // flags, is query
  writer.WriteU16BigEndian(1);                       // qdcount
  writer.WriteU16BigEndian(0);                       // ancount
  writer.WriteU16BigEndian(0);                       // nscount
  writer.WriteU16BigEndian(0);                       // arcount
  writer.Write(dns_name.value());                    // qname
  writer.WriteU16BigEndian(dns_protocol::kTypeA);    // qtype
  writer.WriteU16BigEndian(dns_protocol::kClassIN);  // qclass
  // buf contains 10 extra zero bytes.
  std::optional<DnsQuery> query;
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
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, SingleQuadARecordAnswer) {
  const uint8_t response_data[] = {
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
                       std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest,
     SingleARecordAnswerWithQuestionAndNsecAdditionalRecord) {
  const uint8_t response_data[] = {
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
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(dotted_name);
  ASSERT_TRUE(dns_name.has_value());
  std::optional<DnsQuery> query;
  query.emplace(0x1234 /* id */, dns_name.value(), dns_protocol::kTypeA);
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
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, TwoAnswersWithAAndQuadARecords) {
  const uint8_t response_data[] = {
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
                       std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, AnswerWithAuthorityRecord) {
  const uint8_t response_data[] = {
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
                       {} /* additional records */, std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
}

TEST(DnsResponseWriteTest, AnswerWithRcode) {
  const uint8_t response_data[] = {
      0x12, 0x12,  // ID
      0x80, 0x03,  // flags (response with non-existent domain)
      0x00, 0x00,  // number of questions
      0x00, 0x00,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
  };
  DnsResponse response(0x1212 /* response_id */, false /* is_authoritative*/,
                       {} /* answers */, {} /* authority_records */,
                       {} /* additional records */, std::nullopt,
                       dns_protocol::kRcodeNXDOMAIN);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response(response.io_buffer()->data(),
                              response.io_buffer_size());
  EXPECT_EQ(expected_response, actual_response);
  EXPECT_EQ(dns_protocol::kRcodeNXDOMAIN, response.rcode());
}

// CNAME answers are always allowed for any question.
TEST(DnsResponseWriteTest, AAAAQuestionAndCnameAnswer) {
  const std::string kName = "www.example.com";
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(kName);
  ASSERT_TRUE(dns_name.has_value());

  DnsResourceRecord answer;
  answer.name = kName;
  answer.type = dns_protocol::kTypeCNAME;
  answer.klass = dns_protocol::kClassIN;
  answer.ttl = 120;  // 120 seconds.
  answer.SetOwnedRdata(
      std::string(reinterpret_cast<char*>(dns_name.value().data()),
                  dns_name.value().size()));
  std::vector<DnsResourceRecord> answers(1, answer);

  std::optional<DnsQuery> query(std::in_place, 114 /* id */, dns_name.value(),
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
                       std::nullopt);
  ASSERT_NE(nullptr, response.io_buffer());
  EXPECT_TRUE(response.IsValid());
  EXPECT_THAT(response.id(), testing::Optional(0x1234));
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

TEST(DnsResponseWriteTest, CreateEmptyNoDataResponse) {
  DnsResponse response = DnsResponse::CreateEmptyNoDataResponse(
      /*id=*/4,
      /*is_authoritative=*/true, base::as_byte_span("\x04name\x04test\x00"),
      dns_protocol::kTypeA);

  EXPECT_TRUE(response.IsValid());
  EXPECT_THAT(response.id(), testing::Optional(4));
  EXPECT_TRUE(response.flags() & dns_protocol::kFlagAA);
  EXPECT_EQ(response.question_count(), 1u);
  EXPECT_EQ(response.answer_count(), 0u);
  EXPECT_EQ(response.authority_count(), 0u);
  EXPECT_EQ(response.additional_answer_count(), 0u);

  EXPECT_THAT(response.qtypes(), testing::ElementsAre(dns_protocol::kTypeA));
  EXPECT_THAT(response.dotted_qnames(), testing::ElementsAre("name.test"));
}

}  // namespace

}  // namespace net
