// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_names_util.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span_reader.h"
#include "base/numerics/safe_conversions.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::dns_names_util {
namespace {

using ::testing::Eq;
using ::testing::Optional;

// ToBytes converts a char* to a std::vector<uint8_t> and includes the
// terminating NUL in the result.
std::vector<uint8_t> ToBytes(const char* in) {
  size_t size = strlen(in) + 1;
  std::vector<uint8_t> out(size, 0);
  memcpy(out.data(), in, size);
  return out;
}

TEST(DnsNamesUtilTest, DottedNameToNetworkWithValidation) {
  EXPECT_THAT(
      DottedNameToNetwork("com", /*require_valid_internet_hostname=*/true),
      Optional(ToBytes("\003com")));
  EXPECT_THAT(DottedNameToNetwork("google.com",
                                  /*require_valid_internet_hostname=*/true),
              Optional(ToBytes("\x006google\003com")));
  EXPECT_THAT(DottedNameToNetwork("www.google.com",
                                  /*require_valid_internet_hostname=*/true),
              Optional(ToBytes("\003www\006google\003com")));
}

TEST(DnsNamesUtilTest, DottedNameToNetwork) {
  EXPECT_THAT(
      DottedNameToNetwork("com", /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\003com")));
  EXPECT_THAT(DottedNameToNetwork("google.com",
                                  /*require_valid_internet_hostname=*/false),
              Optional(ToBytes("\x006google\003com")));
  EXPECT_THAT(DottedNameToNetwork("www.google.com",
                                  /*require_valid_internet_hostname=*/false),
              Optional(ToBytes("\003www\006google\003com")));
}

TEST(DnsNamesUtilTest, DottedNameToNetworkWithValidationRejectsEmptyLabels) {
  EXPECT_FALSE(DottedNameToNetwork("", /*require_valid_internet_hostname=*/true)
                   .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(".", /*require_valid_internet_hostname=*/true)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork("..", /*require_valid_internet_hostname=*/true)
          .has_value());
  EXPECT_FALSE(DottedNameToNetwork(".google.com",
                                   /*require_valid_internet_hostname=*/true)
                   .has_value());
  EXPECT_FALSE(DottedNameToNetwork("www..google.com",
                                   /*require_valid_internet_hostname=*/true)
                   .has_value());
}

TEST(DnsNamesUtilTest, DottedNameToNetworkRejectsEmptyLabels) {
  EXPECT_FALSE(
      DottedNameToNetwork("", /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(".", /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork("..", /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(DottedNameToNetwork(".google.com",
                                   /*require_valid_internet_hostname=*/false)
                   .has_value());
  EXPECT_FALSE(DottedNameToNetwork("www..google.com",
                                   /*require_valid_internet_hostname=*/false)
                   .has_value());
}

TEST(DnsNamesUtilTest,
     DottedNameToNetworkWithValidationAcceptsEmptyLabelAtEnd) {
  EXPECT_THAT(DottedNameToNetwork("www.google.com.",
                                  /*require_valid_internet_hostname=*/true),
              Optional(ToBytes("\003www\006google\003com")));
}

TEST(DnsNamesUtilTest, DottedNameToNetworkAcceptsEmptyLabelAtEnd) {
  EXPECT_THAT(DottedNameToNetwork("www.google.com.",
                                  /*require_valid_internet_hostname=*/false),
              Optional(ToBytes("\003www\006google\003com")));
}

TEST(DnsNamesUtilTest, DottedNameToNetworkWithValidationAllowsLongNames) {
  // Label is 63 chars: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "z23456789a123456789a123456789a123456789a123456789a123456789a123",
          /*require_valid_internet_hostname=*/true),
      Optional(ToBytes("\077z23456789a123456789a123456789a123456789a123456"
                       "789a123456789a123")));
  EXPECT_THAT(
      DottedNameToNetwork(
          "z23456789a123456789a123456789a123456789a123456789a123456789a123.",
          /*require_valid_internet_hostname=*/true),
      Optional(ToBytes("\077z23456789a123456789a123456789a123456789a123456"
                       "789a123456789a123")));

  // 253 characters in the name: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abc",
          /*require_valid_internet_hostname=*/true),
      Optional(ToBytes("\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\003abc")));

  // 253 characters in the name plus final dot: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abc.",
          /*require_valid_internet_hostname=*/true),
      Optional(ToBytes("\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\003abc")));
}

TEST(DnsNamesUtilTest, DottedNameToNetworkAllowsLongNames) {
  // Label is 63 chars: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "z23456789a123456789a123456789a123456789a123456789a123456789a123",
          /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\077z23456789a123456789a123456789a123456789a123456"
                       "789a123456789a123")));
  // Label is 63 chars: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "z23456789a123456789a123456789a123456789a123456789a123456789a123.",
          /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\077z23456789a123456789a123456789a123456789a123456"
                       "789a123456789a123")));

  // 253 characters in the name: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abc",
          /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\003abc")));

  // 253 characters in the name plus final dot: still valid
  EXPECT_THAT(
      DottedNameToNetwork(
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
          "abcdefghi.abc.",
          /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi"
                       "\011abcdefghi\003abc")));
}

TEST(DnsNamesUtilTest, DottedNameToNetworkWithValidationRejectsTooLongNames) {
  // Label is too long: invalid
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789a123456789a123456789a123456789a123456789a123456789a1234",
          /*require_valid_internet_hostname=*/true)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789a123456789a123456789a123456789a123456789a123456789a1234.",
          /*require_valid_internet_hostname=*/true)
          .has_value());

  // 254 characters in the name: invalid
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.1234",
          /*require_valid_internet_hostname=*/true)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.1234.",
          /*require_valid_internet_hostname=*/true)
          .has_value());

  // 255 characters in the name: invalid before even trying to add a final
  // zero-length termination
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.12345",
          /*require_valid_internet_hostname=*/true)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.12345.",
          /*require_valid_internet_hostname=*/true)
          .has_value());
}

TEST(DnsNamesUtilTest, DottedNameToNetworkRejectsTooLongNames) {
  // Label is too long: invalid
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789a123456789a123456789a123456789a123456789a123456789a1234",
          /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789a123456789a123456789a123456789a123456789a123456789a1234.",
          /*require_valid_internet_hostname=*/false)
          .has_value());

  // 254 characters in the name: invalid
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.1234",
          /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.1234.",
          /*require_valid_internet_hostname=*/false)
          .has_value());

  // 255 characters in the name: invalid before even trying to add a final
  // zero-length termination
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.12345",
          /*require_valid_internet_hostname=*/false)
          .has_value());
  EXPECT_FALSE(
      DottedNameToNetwork(
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.123456789.123456789.123456789.123456789.123456789."
          "123456789.12345.",
          /*require_valid_internet_hostname=*/false)
          .has_value());
}

TEST(DnsNamesUtilTest,
     DottedNameToNetworkWithValidationRejectsRestrictedCharacters) {
  EXPECT_FALSE(DottedNameToNetwork("foo,bar.com",
                                   /*require_valid_internet_hostname=*/true)
                   .has_value());
  EXPECT_FALSE(DottedNameToNetwork("_ipp._tcp.local.foo printer (bar)",
                                   /*require_valid_internet_hostname=*/true)
                   .has_value());
}

TEST(DnsNamesUtilTest, DottedNameToNetworkAcceptsRestrictedCharacters) {
  EXPECT_THAT(DottedNameToNetwork("foo,bar.com",
                                  /*require_valid_internet_hostname=*/false),
              Optional(ToBytes("\007foo,bar\003com")));

  EXPECT_THAT(
      DottedNameToNetwork("_ipp._tcp.local.foo printer (bar)",
                          /*require_valid_internet_hostname=*/false),
      Optional(ToBytes("\004_ipp\004_tcp\005local\021foo printer (bar)")));
}

TEST(DnsNamesUtilTest, NetworkToDottedNameAdvancesReader) {
  {
    auto reader = base::SpanReader(base::byte_span_from_cstring("\003com"));
    EXPECT_THAT(NetworkToDottedName(reader), Optional(Eq("com")));
    EXPECT_EQ(reader.remaining(), 0u);
  }
  {
    auto reader =
        base::SpanReader(base::byte_span_from_cstring("\003com\0ZZZ"));
    EXPECT_THAT(NetworkToDottedName(reader), Optional(Eq("com")));
    EXPECT_EQ(reader.remaining(), 3u);
  }
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleSimpleNames) {
  std::string dns_name = "\003foo";
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("foo")));

  dns_name += "\003bar";
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("foo.bar")));

  dns_name += "\002uk";
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("foo.bar.uk")));

  dns_name += '\0';
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("foo.bar.uk")));
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleEmpty) {
  std::string dns_name;

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("")));

  dns_name += '\0';
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("")));
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectEmptyIncomplete) {
  std::string dns_name;

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  false /* require_complete */),
              Optional(Eq("")));

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name),
                                true /* require_complete */),
            std::nullopt);
}

// Test `require_complete` functionality given an input with terminating zero-
// length label.
TEST(DnsNamesUtilTest, NetworkToDottedNameComplete) {
  std::string dns_name("\003foo\004test");
  dns_name += '\0';

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  false /* require_complete */),
              Optional(Eq("foo.test")));

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  true /* require_complete */),
              Optional(Eq("foo.test")));
}

// Test `require_complete` functionality given an input without terminating
// zero-length label.
TEST(DnsNamesUtilTest, NetworkToDottedNameNotComplete) {
  std::string dns_name("\003boo\004test");

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  false /* require_complete */),
              Optional(Eq("boo.test")));

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name),
                                true /* require_complete */),
            std::nullopt);
}

TEST(DnsNamesUtilTest,
     NetworkToDottedNameShouldRejectEmptyWhenRequiringComplete) {
  std::string dns_name;

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  false /* require_complete */),
              Optional(Eq("")));

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name),
                                true /* require_complete */),
            std::nullopt);

  dns_name += '\0';
  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name),
                                  true /* require_complete */),
              Optional(Eq("")));
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectCompression) {
  std::string dns_name = CreateNamePointer(152);

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);

  dns_name = "\005hello";
  dns_name += CreateNamePointer(152);

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

// Test that extra input past the terminating zero-length label are ignored.
TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleExcessInput) {
  std::string dns_name("\004cool\004name\004test");
  dns_name += '\0';
  dns_name += "blargh!";

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("cool.name.test")));

  dns_name = "\002hi";
  dns_name += '\0';
  dns_name += "goodbye";

  EXPECT_THAT(NetworkToDottedName(base::as_byte_span(dns_name)),
              Optional(Eq("hi")));
}

// Test that input is malformed if it ends mid label.
TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectTruncatedNames) {
  std::string dns_name = "\07cheese";
  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);

  dns_name = "\006cheesy\05test";
  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleLongSingleLabel) {
  std::string dns_name(1, static_cast<char>(dns_protocol::kMaxLabelLength));
  for (int i = 0; i < dns_protocol::kMaxLabelLength; ++i) {
    dns_name += 'a';
  }

  EXPECT_NE(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleLongSecondLabel) {
  std::string dns_name("\003foo");
  dns_name += static_cast<char>(dns_protocol::kMaxLabelLength);
  for (int i = 0; i < dns_protocol::kMaxLabelLength; ++i) {
    dns_name += 'a';
  }

  EXPECT_NE(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectTooLongSingleLabel) {
  std::string dns_name(1, static_cast<char>(dns_protocol::kMaxLabelLength));
  for (int i = 0; i < dns_protocol::kMaxLabelLength + 1; ++i) {
    dns_name += 'a';
  }

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectTooLongSecondLabel) {
  std::string dns_name("\003foo");
  dns_name += static_cast<char>(dns_protocol::kMaxLabelLength);
  for (int i = 0; i < dns_protocol::kMaxLabelLength + 1; ++i) {
    dns_name += 'a';
  }

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

#if CHAR_MIN < 0
TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectCharMinLabels) {
  ASSERT_GT(static_cast<uint8_t>(CHAR_MIN), dns_protocol::kMaxLabelLength);

  std::string dns_name;
  dns_name += base::checked_cast<char>(CHAR_MIN);

  // Wherever possible, make the name otherwise valid.
  if (static_cast<uint8_t>(CHAR_MIN) < UINT8_MAX) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(CHAR_MIN); ++i) {
      dns_name += 'a';
    }
  }

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}
#endif  // if CHAR_MIN < 0

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleLongName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - 1 - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  ASSERT_EQ(dns_name.size(), static_cast<size_t>(dns_protocol::kMaxNameLength));

  EXPECT_NE(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectTooLongName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength + 1;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 1));

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldHandleLongCompleteName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - 1 - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  dns_name += '\0';
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 1));

  EXPECT_NE(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, NetworkToDottedNameShouldRejectTooLongCompleteName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength + 1;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  dns_name += '\0';
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 2));

  EXPECT_EQ(NetworkToDottedName(base::as_byte_span(dns_name)), std::nullopt);
}

TEST(DnsNamesUtilTest, ValidDnsNames) {
  constexpr std::string_view kGoodHostnames[] = {
      "www.noodles.blorg",   "1www.noodles.blorg",    "www.2noodles.blorg",
      "www.n--oodles.blorg", "www.noodl_es.blorg",    "www.no-_odles.blorg",
      "www_.noodles.blorg",  "www.noodles.blorg.",    "_privet._tcp.local",
      "%20%20noodles.blorg", "noo dles.blorg ",       "noo dles_ipp._tcp.local",
      "www.nood(les).blorg", "noo dl(es)._tcp.local",
  };

  for (std::string_view good_hostname : kGoodHostnames) {
    EXPECT_TRUE(IsValidDnsName(good_hostname));
    EXPECT_TRUE(IsValidDnsRecordName(good_hostname));
  }
}

TEST(DnsNamesUtilTest, EmptyNotValidDnsName) {
  EXPECT_FALSE(IsValidDnsName(""));
  EXPECT_FALSE(IsValidDnsRecordName(""));
}

TEST(DnsNamesUtilTest, EmptyLabelNotValidDnsName) {
  EXPECT_FALSE(IsValidDnsName("www..test"));
  EXPECT_FALSE(IsValidDnsName(".foo.test"));

  EXPECT_FALSE(IsValidDnsRecordName("www..test"));
  EXPECT_FALSE(IsValidDnsRecordName(".foo.test"));
}

TEST(DnsNameUtilTest, LongLabelsInValidDnsNames) {
  EXPECT_TRUE(IsValidDnsName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a123"));
  EXPECT_TRUE(IsValidDnsName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a123."));

  EXPECT_TRUE(IsValidDnsRecordName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a123"));
  EXPECT_TRUE(IsValidDnsRecordName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a123."));
}

TEST(DnsNameUtilTest, TooLongLabelsInInvalidDnsNames) {
  EXPECT_FALSE(IsValidDnsName(
      "123456789a123456789a123456789a123456789a123456789a123456789a1234"));
  EXPECT_FALSE(IsValidDnsName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a1234."));

  EXPECT_FALSE(IsValidDnsRecordName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a1234"));
  EXPECT_FALSE(IsValidDnsRecordName(
      "z23456789a123456789a123456789a123456789a123456789a123456789a1234."));
}

TEST(DnsNameUtilTest, LongValidDnsNames) {
  EXPECT_TRUE(IsValidDnsName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abc"));
  EXPECT_TRUE(IsValidDnsName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abc."));

  EXPECT_TRUE(IsValidDnsRecordName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abc"));
  EXPECT_TRUE(IsValidDnsRecordName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abc."));
}

TEST(DnsNameUtilTest, TooLongInalidDnsNames) {
  EXPECT_FALSE(IsValidDnsName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcd"));
  EXPECT_FALSE(IsValidDnsName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcd."));

  EXPECT_FALSE(IsValidDnsRecordName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcd"));
  EXPECT_FALSE(IsValidDnsRecordName(
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
      "abcdefghi.abcd."));
}

TEST(DnsNameUtilTest, LocalhostNotValidDnsRecordName) {
  EXPECT_TRUE(IsValidDnsName("localhost"));
  EXPECT_FALSE(IsValidDnsRecordName("localhost"));
}

TEST(DnsNameUtilTest, IpAddressNotValidDnsRecordName) {
  EXPECT_TRUE(IsValidDnsName("1.2.3.4"));
  EXPECT_FALSE(IsValidDnsRecordName("1.2.3.4"));

  EXPECT_TRUE(IsValidDnsName("[2001:4860:4860::8888]"));
  EXPECT_FALSE(IsValidDnsRecordName("[2001:4860:4860::8888]"));

  EXPECT_TRUE(IsValidDnsName("2001:4860:4860::8888"));
  EXPECT_FALSE(IsValidDnsRecordName("2001:4860:4860::8888"));
}

TEST(DnsUtilTest, CanonicalizeNames) {
  EXPECT_EQ(UrlCanonicalizeNameIfAble("GOOGLE.test"), "google.test");

  EXPECT_EQ(UrlCanonicalizeNameIfAble("g{oo}gle.test"), "g{oo}gle.test");
  EXPECT_EQ(UrlCanonicalizeNameIfAble("G{OO}GLE.test"), "g{oo}gle.test");

  // g�gle.test
  EXPECT_EQ(UrlCanonicalizeNameIfAble("g\u00FCgle.test"), "xn--ggle-0ra.test");
  EXPECT_EQ(UrlCanonicalizeNameIfAble("G\u00fcGLE.test"), "xn--ggle-0ra.test");
}

TEST(DnsUtilTest, IgnoreUncanonicalizeableNames) {
  EXPECT_EQ(UrlCanonicalizeNameIfAble(""), "");

  // Forbidden domain code point.
  // https://url.spec.whatwg.org/#forbidden-domain-code-point
  EXPECT_EQ(UrlCanonicalizeNameIfAble("g<oo>gle.test"), "g<oo>gle.test");
  EXPECT_EQ(UrlCanonicalizeNameIfAble("G<OO>GLE.test"), "G<OO>GLE.test");

  // Invalid UTF8 character.
  EXPECT_EQ(UrlCanonicalizeNameIfAble("g\x00FCgle.test"), "g\x00fcgle.test");
  EXPECT_EQ(UrlCanonicalizeNameIfAble("G\x00fcGLE.test"), "G\x00fcGLE.test");

  // Disallowed ASCII character.
  EXPECT_EQ(UrlCanonicalizeNameIfAble("google\n.test"), "google\n.test");
  EXPECT_EQ(UrlCanonicalizeNameIfAble("GOOGLE\n.test"), "GOOGLE\n.test");
}

TEST(DnsNamesUtilTest, ReadU8LengthPrefixed) {
  const uint8_t kArray[] = {'b', '4', 3, 'a', 'b', 'c', 'd'};
  auto reader = base::SpanReader(base::span(kArray));
  EXPECT_TRUE(reader.Skip(2u));
  EXPECT_EQ(reader.remaining(), 5u);
  EXPECT_EQ(reader.num_read(), 2u);
  base::span<const uint8_t> s;
  EXPECT_TRUE(ReadU8LengthPrefixed(reader, &s));
  EXPECT_EQ(s, base::span(kArray).subspan(3u, 3u));
  EXPECT_EQ(reader.remaining(), 1u);
  EXPECT_EQ(reader.num_read(), 6u);
}

TEST(DnsNamesUtilTest, ReadU16LengthPrefixed) {
  const uint8_t kArray[] = {'b', '4', 0, 3, 'a', 'b', 'c', 'd'};
  auto reader = base::SpanReader(base::span(kArray));
  EXPECT_TRUE(reader.Skip(2u));
  EXPECT_EQ(reader.remaining(), 6u);
  EXPECT_EQ(reader.num_read(), 2u);
  base::span<const uint8_t> s;
  EXPECT_TRUE(ReadU16LengthPrefixed(reader, &s));
  EXPECT_EQ(s, base::span(kArray).subspan(4u, 3u));
  EXPECT_EQ(reader.remaining(), 1u);
  EXPECT_EQ(reader.num_read(), 7u);
}

}  // namespace
}  // namespace net::dns_names_util
