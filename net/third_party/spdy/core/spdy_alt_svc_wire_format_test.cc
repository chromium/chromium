// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/spdy_alt_svc_wire_format.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using ::testing::_;

namespace spdy {

namespace test {

// Expose all private methods of class SpdyAltSvcWireFormat.
class SpdyAltSvcWireFormatPeer {
 public:
  static void SkipWhiteSpace(SpdyStringPiece::const_iterator* c,
                             SpdyStringPiece::const_iterator end) {
    SpdyAltSvcWireFormat::SkipWhiteSpace(c, end);
  }
  static bool PercentDecode(SpdyStringPiece::const_iterator c,
                            SpdyStringPiece::const_iterator end,
                            SpdyString* output) {
    return SpdyAltSvcWireFormat::PercentDecode(c, end, output);
  }
  static bool ParseAltAuthority(SpdyStringPiece::const_iterator c,
                                SpdyStringPiece::const_iterator end,
                                SpdyString* host,
                                uint16_t* port) {
    return SpdyAltSvcWireFormat::ParseAltAuthority(c, end, host, port);
  }
  static bool ParsePositiveInteger16(SpdyStringPiece::const_iterator c,
                                     SpdyStringPiece::const_iterator end,
                                     uint16_t* max_age) {
    return SpdyAltSvcWireFormat::ParsePositiveInteger16(c, end, max_age);
  }
  static bool ParsePositiveInteger32(SpdyStringPiece::const_iterator c,
                                     SpdyStringPiece::const_iterator end,
                                     uint32_t* max_age) {
    return SpdyAltSvcWireFormat::ParsePositiveInteger32(c, end, max_age);
  }
};

}  // namespace test

namespace {

// Generate header field values, possibly with multiply defined parameters and
// random case, and corresponding AlternativeService entries.
void FuzzHeaderFieldValue(
    int i,
    SpdyString* header_field_value,
    SpdyAltSvcWireFormat::AlternativeService* expected_altsvc) {
  if (!header_field_value->empty()) {
    header_field_value->push_back(',');
  }
  // TODO(b/77515496): use struct of bools instead of int |i| to generate the
  // header field value.
  bool is_ietf_format_quic = (i & 1 << 0) != 0;
  if (i & 1 << 0) {
    expected_altsvc->protocol_id = "hq";
    header_field_value->append("hq=\"");
  } else {
    expected_altsvc->protocol_id = "a=b%c";
    header_field_value->append("a%3Db%25c=\"");
  }
  if (i & 1 << 1) {
    expected_altsvc->host = "foo\"bar\\baz";
    header_field_value->append("foo\\\"bar\\\\baz");
  } else {
    expected_altsvc->host = "";
  }
  expected_altsvc->port = 42;
  header_field_value->append(":42\"");
  if (i & 1 << 2) {
    header_field_value->append(" ");
  }
  if (i & 3 << 3) {
    expected_altsvc->max_age = 1111;
    header_field_value->append(";");
    if (i & 1 << 3) {
      header_field_value->append(" ");
    }
    header_field_value->append("mA=1111");
    if (i & 2 << 3) {
      header_field_value->append(" ");
    }
  }
  if (i & 1 << 5) {
    header_field_value->append("; J=s");
  }
  if (i & 1 << 6) {
    if (is_ietf_format_quic) {
      if (i & 1 << 7) {
        expected_altsvc->version.push_back(0x923457e);
        header_field_value->append("; quic=923457E");
      } else {
        expected_altsvc->version.push_back(1);
        expected_altsvc->version.push_back(0xFFFFFFFF);
        header_field_value->append("; quic=1; quic=fFfFffFf");
      }
    } else {
      if (i & i << 7) {
        expected_altsvc->version.push_back(24);
        header_field_value->append("; v=\"24\"");
      } else {
        expected_altsvc->version.push_back(1);
        expected_altsvc->version.push_back(65535);
        header_field_value->append("; v=\"1,65535\"");
      }
    }
  }
  if (i & 1 << 8) {
    expected_altsvc->max_age = 999999999;
    header_field_value->append("; Ma=999999999");
  }
  if (i & 1 << 9) {
    header_field_value->append(";");
  }
  if (i & 1 << 10) {
    header_field_value->append(" ");
  }
  if (i & 1 << 11) {
    header_field_value->append(",");
  }
  if (i & 1 << 12) {
    header_field_value->append(" ");
  }
}

// Generate AlternativeService entries and corresponding header field values in
// canonical form, that is, what SerializeHeaderFieldValue() should output.
void FuzzAlternativeService(int i,
                            SpdyAltSvcWireFormat::AlternativeService* altsvc,
                            SpdyString* expected_header_field_value) {
  if (!expected_header_field_value->empty()) {
    expected_header_field_value->push_back(',');
  }
  altsvc->protocol_id = "a=b%c";
  altsvc->port = 42;
  expected_header_field_value->append("a%3Db%25c=\"");
  if (i & 1 << 0) {
    altsvc->host = "foo\"bar\\baz";
    expected_header_field_value->append("foo\\\"bar\\\\baz");
  }
  expected_header_field_value->append(":42\"");
  if (i & 1 << 1) {
    altsvc->max_age = 1111;
    expected_header_field_value->append("; ma=1111");
  }
  if (i & 1 << 2) {
    altsvc->version.push_back(24);
    altsvc->version.push_back(25);
    expected_header_field_value->append("; v=\"24,25\"");
  }
}

// Tests of public API.

TEST(SpdyAltSvcWireFormatTest, DefaultValues) {
  SpdyAltSvcWireFormat::AlternativeService altsvc;
  EXPECT_EQ("", altsvc.protocol_id);
  EXPECT_EQ("", altsvc.host);
  EXPECT_EQ(0u, altsvc.port);
  EXPECT_EQ(86400u, altsvc.max_age);
  EXPECT_TRUE(altsvc.version.empty());
}

TEST(SpdyAltSvcWireFormatTest, ParseInvalidEmptyHeaderFieldValue) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  ASSERT_FALSE(SpdyAltSvcWireFormat::ParseHeaderFieldValue("", &altsvc_vector));
}

TEST(SpdyAltSvcWireFormatTest, ParseHeaderFieldValueClear) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  ASSERT_TRUE(
      SpdyAltSvcWireFormat::ParseHeaderFieldValue("clear", &altsvc_vector));
  EXPECT_EQ(0u, altsvc_vector.size());
}

// Fuzz test of ParseHeaderFieldValue() with optional whitespaces, ignored
// parameters, duplicate parameters, trailing space, trailing alternate service
// separator, etc.  Single alternative service at a time.
TEST(SpdyAltSvcWireFormatTest, ParseHeaderFieldValue) {
  for (int i = 0; i < 1 << 13; ++i) {
    SpdyString header_field_value;
    SpdyAltSvcWireFormat::AlternativeService expected_altsvc;
    FuzzHeaderFieldValue(i, &header_field_value, &expected_altsvc);
    SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
    ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(header_field_value,
                                                            &altsvc_vector));
    ASSERT_EQ(1u, altsvc_vector.size());
    EXPECT_EQ(expected_altsvc.protocol_id, altsvc_vector[0].protocol_id);
    EXPECT_EQ(expected_altsvc.host, altsvc_vector[0].host);
    EXPECT_EQ(expected_altsvc.port, altsvc_vector[0].port);
    EXPECT_EQ(expected_altsvc.max_age, altsvc_vector[0].max_age);
    EXPECT_EQ(expected_altsvc.version, altsvc_vector[0].version);

    // Roundtrip test starting with |altsvc_vector|.
    SpdyString reserialized_header_field_value =
        SpdyAltSvcWireFormat::SerializeHeaderFieldValue(altsvc_vector);
    SpdyAltSvcWireFormat::AlternativeServiceVector roundtrip_altsvc_vector;
    ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
        reserialized_header_field_value, &roundtrip_altsvc_vector));
    ASSERT_EQ(1u, roundtrip_altsvc_vector.size());
    EXPECT_EQ(expected_altsvc.protocol_id,
              roundtrip_altsvc_vector[0].protocol_id);
    EXPECT_EQ(expected_altsvc.host, roundtrip_altsvc_vector[0].host);
    EXPECT_EQ(expected_altsvc.port, roundtrip_altsvc_vector[0].port);
    EXPECT_EQ(expected_altsvc.max_age, roundtrip_altsvc_vector[0].max_age);
    EXPECT_EQ(expected_altsvc.version, roundtrip_altsvc_vector[0].version);
  }
}

// Fuzz test of ParseHeaderFieldValue() with optional whitespaces, ignored
// parameters, duplicate parameters, trailing space, trailing alternate service
// separator, etc.  Possibly multiple alternative service at a time.
TEST(SpdyAltSvcWireFormatTest, ParseHeaderFieldValueMultiple) {
  for (int i = 0; i < 1 << 13;) {
    SpdyString header_field_value;
    SpdyAltSvcWireFormat::AlternativeServiceVector expected_altsvc_vector;
    // This will generate almost two hundred header field values with two,
    // three, four, five, six, and seven alternative services each, and
    // thousands with a single one.
    do {
      SpdyAltSvcWireFormat::AlternativeService expected_altsvc;
      FuzzHeaderFieldValue(i, &header_field_value, &expected_altsvc);
      expected_altsvc_vector.push_back(expected_altsvc);
      ++i;
    } while (i % 6 < i % 7);
    SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
    ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(header_field_value,
                                                            &altsvc_vector));
    ASSERT_EQ(expected_altsvc_vector.size(), altsvc_vector.size());
    for (unsigned int j = 0; j < altsvc_vector.size(); ++j) {
      EXPECT_EQ(expected_altsvc_vector[j].protocol_id,
                altsvc_vector[j].protocol_id);
      EXPECT_EQ(expected_altsvc_vector[j].host, altsvc_vector[j].host);
      EXPECT_EQ(expected_altsvc_vector[j].port, altsvc_vector[j].port);
      EXPECT_EQ(expected_altsvc_vector[j].max_age, altsvc_vector[j].max_age);
      EXPECT_EQ(expected_altsvc_vector[j].version, altsvc_vector[j].version);
    }

    // Roundtrip test starting with |altsvc_vector|.
    SpdyString reserialized_header_field_value =
        SpdyAltSvcWireFormat::SerializeHeaderFieldValue(altsvc_vector);
    SpdyAltSvcWireFormat::AlternativeServiceVector roundtrip_altsvc_vector;
    ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
        reserialized_header_field_value, &roundtrip_altsvc_vector));
    ASSERT_EQ(expected_altsvc_vector.size(), roundtrip_altsvc_vector.size());
    for (unsigned int j = 0; j < roundtrip_altsvc_vector.size(); ++j) {
      EXPECT_EQ(expected_altsvc_vector[j].protocol_id,
                roundtrip_altsvc_vector[j].protocol_id);
      EXPECT_EQ(expected_altsvc_vector[j].host,
                roundtrip_altsvc_vector[j].host);
      EXPECT_EQ(expected_altsvc_vector[j].port,
                roundtrip_altsvc_vector[j].port);
      EXPECT_EQ(expected_altsvc_vector[j].max_age,
                roundtrip_altsvc_vector[j].max_age);
      EXPECT_EQ(expected_altsvc_vector[j].version,
                roundtrip_altsvc_vector[j].version);
    }
  }
}

TEST(SpdyAltSvcWireFormatTest, SerializeEmptyHeaderFieldValue) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  EXPECT_EQ("clear",
            SpdyAltSvcWireFormat::SerializeHeaderFieldValue(altsvc_vector));
}

// Test ParseHeaderFieldValue() and SerializeHeaderFieldValue() on the same pair
// of |expected_header_field_value| and |altsvc|, with and without hostname and
// each
// parameter.  Single alternative service at a time.
TEST(SpdyAltSvcWireFormatTest, RoundTrip) {
  for (int i = 0; i < 1 << 3; ++i) {
    SpdyAltSvcWireFormat::AlternativeService altsvc;
    SpdyString expected_header_field_value;
    FuzzAlternativeService(i, &altsvc, &expected_header_field_value);

    // Test ParseHeaderFieldValue().
    SpdyAltSvcWireFormat::AlternativeServiceVector parsed_altsvc_vector;
    ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
        expected_header_field_value, &parsed_altsvc_vector));
    ASSERT_EQ(1u, parsed_altsvc_vector.size());
    EXPECT_EQ(altsvc.protocol_id, parsed_altsvc_vector[0].protocol_id);
    EXPECT_EQ(altsvc.host, parsed_altsvc_vector[0].host);
    EXPECT_EQ(altsvc.port, parsed_altsvc_vector[0].port);
    EXPECT_EQ(altsvc.max_age, parsed_altsvc_vector[0].max_age);
    EXPECT_EQ(altsvc.version, parsed_altsvc_vector[0].version);

    // Test SerializeHeaderFieldValue().
    SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
    altsvc_vector.push_back(altsvc);
    EXPECT_EQ(expected_header_field_value,
              SpdyAltSvcWireFormat::SerializeHeaderFieldValue(altsvc_vector));
  }
}

// Test ParseHeaderFieldValue() and SerializeHeaderFieldValue() on the same pair
// of |expected_header_field_value| and |altsvc|, with and without hostname and
// each
// parameter.  Multiple alternative services at a time.
TEST(SpdyAltSvcWireFormatTest, RoundTripMultiple) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  SpdyString expected_header_field_value;
  for (int i = 0; i < 1 << 3; ++i) {
    SpdyAltSvcWireFormat::AlternativeService altsvc;
    FuzzAlternativeService(i, &altsvc, &expected_header_field_value);
    altsvc_vector.push_back(altsvc);
  }

  // Test ParseHeaderFieldValue().
  SpdyAltSvcWireFormat::AlternativeServiceVector parsed_altsvc_vector;
  ASSERT_TRUE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
      expected_header_field_value, &parsed_altsvc_vector));
  ASSERT_EQ(altsvc_vector.size(), parsed_altsvc_vector.size());
  auto expected_it = altsvc_vector.begin();
  auto parsed_it = parsed_altsvc_vector.begin();
  for (; expected_it != altsvc_vector.end(); ++expected_it, ++parsed_it) {
    EXPECT_EQ(expected_it->protocol_id, parsed_it->protocol_id);
    EXPECT_EQ(expected_it->host, parsed_it->host);
    EXPECT_EQ(expected_it->port, parsed_it->port);
    EXPECT_EQ(expected_it->max_age, parsed_it->max_age);
    EXPECT_EQ(expected_it->version, parsed_it->version);
  }

  // Test SerializeHeaderFieldValue().
  EXPECT_EQ(expected_header_field_value,
            SpdyAltSvcWireFormat::SerializeHeaderFieldValue(altsvc_vector));
}

// ParseHeaderFieldValue() should return false on malformed field values:
// invalid percent encoding, unmatched quotation mark, empty port, non-numeric
// characters in numeric fields.
TEST(SpdyAltSvcWireFormatTest, ParseHeaderFieldValueInvalid) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  const char* invalid_field_value_array[] = {"a%",
                                             "a%x",
                                             "a%b",
                                             "a%9z",
                                             "a=",
                                             "a=\"",
                                             "a=\"b\"",
                                             "a=\":\"",
                                             "a=\"c:\"",
                                             "a=\"c:foo\"",
                                             "a=\"c:42foo\"",
                                             "a=\"b:42\"bar",
                                             "a=\"b:42\" ; m",
                                             "a=\"b:42\" ; min-age",
                                             "a=\"b:42\" ; ma",
                                             "a=\"b:42\" ; ma=",
                                             "a=\"b:42\" ; v=\"..\"",
                                             "a=\"b:42\" ; ma=ma",
                                             "a=\"b:42\" ; ma=123bar",
                                             "a=\"b:42\" ; v=24",
                                             "a=\"b:42\" ; v=24,25",
                                             "a=\"b:42\" ; v=\"-3\"",
                                             "a=\"b:42\" ; v=\"1.2\"",
                                             "a=\"b:42\" ; v=\"24,\""};
  for (const char* invalid_field_value : invalid_field_value_array) {
    EXPECT_FALSE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
        invalid_field_value, &altsvc_vector))
        << invalid_field_value;
  }
}

// ParseHeaderFieldValue() should return false on a field values truncated
// before closing quotation mark, without trying to access memory beyond the end
// of the input.
TEST(SpdyAltSvcWireFormatTest, ParseTruncatedHeaderFieldValue) {
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  const char* field_value_array[] = {"a=\":137\"", "a=\"foo:137\"",
                                     "a%25=\"foo\\\"bar\\\\baz:137\""};
  for (const SpdyString& field_value : field_value_array) {
    for (size_t len = 1; len < field_value.size(); ++len) {
      EXPECT_FALSE(SpdyAltSvcWireFormat::ParseHeaderFieldValue(
          field_value.substr(0, len), &altsvc_vector))
          << len;
    }
  }
}

// Tests of private methods.

// Test SkipWhiteSpace().
TEST(SpdyAltSvcWireFormatTest, SkipWhiteSpace) {
  SpdyStringPiece input("a \tb  ");
  SpdyStringPiece::const_iterator c = input.begin();
  test::SpdyAltSvcWireFormatPeer::SkipWhiteSpace(&c, input.end());
  ASSERT_EQ(input.begin(), c);
  ++c;
  test::SpdyAltSvcWireFormatPeer::SkipWhiteSpace(&c, input.end());
  ASSERT_EQ(input.begin() + 3, c);
  ++c;
  test::SpdyAltSvcWireFormatPeer::SkipWhiteSpace(&c, input.end());
  ASSERT_EQ(input.end(), c);
}

// Test PercentDecode() on valid input.
TEST(SpdyAltSvcWireFormatTest, PercentDecodeValid) {
  SpdyStringPiece input("");
  SpdyString output;
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::PercentDecode(
      input.begin(), input.end(), &output));
  EXPECT_EQ("", output);

  input = SpdyStringPiece("foo");
  output.clear();
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::PercentDecode(
      input.begin(), input.end(), &output));
  EXPECT_EQ("foo", output);

  input = SpdyStringPiece("%2ca%5Cb");
  output.clear();
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::PercentDecode(
      input.begin(), input.end(), &output));
  EXPECT_EQ(",a\\b", output);
}

// Test PercentDecode() on invalid input.
TEST(SpdyAltSvcWireFormatTest, PercentDecodeInvalid) {
  const char* invalid_input_array[] = {"a%", "a%x", "a%b", "%J22", "%9z"};
  for (const char* invalid_input : invalid_input_array) {
    SpdyStringPiece input(invalid_input);
    SpdyString output;
    EXPECT_FALSE(test::SpdyAltSvcWireFormatPeer::PercentDecode(
        input.begin(), input.end(), &output))
        << input;
  }
}

// Test ParseAltAuthority() on valid input.
TEST(SpdyAltSvcWireFormatTest, ParseAltAuthorityValid) {
  SpdyStringPiece input(":42");
  SpdyString host;
  uint16_t port;
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParseAltAuthority(
      input.begin(), input.end(), &host, &port));
  EXPECT_TRUE(host.empty());
  EXPECT_EQ(42, port);

  input = SpdyStringPiece("foo:137");
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParseAltAuthority(
      input.begin(), input.end(), &host, &port));
  EXPECT_EQ("foo", host);
  EXPECT_EQ(137, port);

  input = SpdyStringPiece("[2003:8:0:16::509d:9615]:443");
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParseAltAuthority(
      input.begin(), input.end(), &host, &port));
  EXPECT_EQ("[2003:8:0:16::509d:9615]", host);
  EXPECT_EQ(443, port);
}

// Test ParseAltAuthority() on invalid input: empty string, no port, zero port,
// non-digit characters following port.
TEST(SpdyAltSvcWireFormatTest, ParseAltAuthorityInvalid) {
  const char* invalid_input_array[] = {"",
                                       ":",
                                       "foo:",
                                       ":bar",
                                       ":0",
                                       "foo:0",
                                       ":12bar",
                                       "foo:23bar",
                                       " ",
                                       ":12 ",
                                       "foo:12 ",
                                       "[2003:8:0:16::509d:9615]",
                                       "[2003:8:0:16::509d:9615]:",
                                       "[2003:8:0:16::509d:9615]foo:443",
                                       "[2003:8:0:16::509d:9615:443",
                                       "2003:8:0:16::509d:9615]:443"};
  for (const char* invalid_input : invalid_input_array) {
    SpdyStringPiece input(invalid_input);
    SpdyString host;
    uint16_t port;
    EXPECT_FALSE(test::SpdyAltSvcWireFormatPeer::ParseAltAuthority(
        input.begin(), input.end(), &host, &port))
        << input;
  }
}

// Test ParseInteger() on valid input.
TEST(SpdyAltSvcWireFormatTest, ParseIntegerValid) {
  SpdyStringPiece input("3");
  uint16_t value;
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
      input.begin(), input.end(), &value));
  EXPECT_EQ(3, value);

  input = SpdyStringPiece("1337");
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
      input.begin(), input.end(), &value));
  EXPECT_EQ(1337, value);
}

// Test ParseIntegerValid() on invalid input: empty, zero, non-numeric, trailing
// non-numeric characters.
TEST(SpdyAltSvcWireFormatTest, ParseIntegerInvalid) {
  const char* invalid_input_array[] = {"", " ", "a", "0", "00", "1 ", "12b"};
  for (const char* invalid_input : invalid_input_array) {
    SpdyStringPiece input(invalid_input);
    uint16_t value;
    EXPECT_FALSE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
        input.begin(), input.end(), &value))
        << input;
  }
}

// Test ParseIntegerValid() around overflow limit.
TEST(SpdyAltSvcWireFormatTest, ParseIntegerOverflow) {
  // Largest possible uint16_t value.
  SpdyStringPiece input("65535");
  uint16_t value16;
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
      input.begin(), input.end(), &value16));
  EXPECT_EQ(65535, value16);

  // Overflow uint16_t, ParsePositiveInteger16() should return false.
  input = SpdyStringPiece("65536");
  ASSERT_FALSE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
      input.begin(), input.end(), &value16));

  // However, even if overflow is not checked for, 65536 overflows to 0, which
  // returns false anyway.  Check for a larger number which overflows to 1.
  input = SpdyStringPiece("65537");
  ASSERT_FALSE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger16(
      input.begin(), input.end(), &value16));

  // Largest possible uint32_t value.
  input = SpdyStringPiece("4294967295");
  uint32_t value32;
  ASSERT_TRUE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger32(
      input.begin(), input.end(), &value32));
  EXPECT_EQ(4294967295, value32);

  // Overflow uint32_t, ParsePositiveInteger32() should return false.
  input = SpdyStringPiece("4294967296");
  ASSERT_FALSE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger32(
      input.begin(), input.end(), &value32));

  // However, even if overflow is not checked for, 4294967296 overflows to 0,
  // which returns false anyway.  Check for a larger number which overflows to
  // 1.
  input = SpdyStringPiece("4294967297");
  ASSERT_FALSE(test::SpdyAltSvcWireFormatPeer::ParsePositiveInteger32(
      input.begin(), input.end(), &value32));
}

// Test parsing an Alt-Svc entry with IP literal hostname.
// Regression test for https://crbug.com/664173.
TEST(SpdyAltSvcWireFormatTest, ParseIPLiteral) {
  const char* input =
      "quic=\"[2003:8:0:16::509d:9615]:443\"; v=\"36,35\"; ma=60";
  SpdyAltSvcWireFormat::AlternativeServiceVector altsvc_vector;
  ASSERT_TRUE(
      SpdyAltSvcWireFormat::ParseHeaderFieldValue(input, &altsvc_vector));
  EXPECT_EQ(1u, altsvc_vector.size());
  EXPECT_EQ("quic", altsvc_vector[0].protocol_id);
  EXPECT_EQ("[2003:8:0:16::509d:9615]", altsvc_vector[0].host);
  EXPECT_EQ(443u, altsvc_vector[0].port);
  EXPECT_EQ(60u, altsvc_vector[0].max_age);
  EXPECT_THAT(altsvc_vector[0].version, ::testing::ElementsAre(36, 35));
}

}  // namespace

}  // namespace spdy
