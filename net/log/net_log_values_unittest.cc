// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_values.h"

#include <limits>

#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Calls NetLogASCIIStringValue() on |raw| and returns the resulting string
// (rather than the base::Value).
std::string GetNetLogString(std::string_view raw) {
  base::Value value = NetLogStringValue(raw);
  EXPECT_TRUE(value.is_string());
  return value.GetString();
}

TEST(NetLogValuesTest, NetLogASCIIStringValue) {
  // ASCII strings should not be transformed.
  EXPECT_EQ("ascii\nstrin\0g", GetNetLogString("ascii\nstrin\0g"));

  // Non-ASCII UTF-8 strings should be escaped.
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B utf-8 string %E2%98%83",
            GetNetLogString("utf-8 string \xE2\x98\x83"));

  // The presence of percent should not trigger escaping.
  EXPECT_EQ("%20", GetNetLogString("%20"));

  // However if the value to be escaped contains percent, it should be escaped
  // (so can unescape to restore the original string).
  EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %E2%98%83 %2520",
            GetNetLogString("\xE2\x98\x83 %20"));

  // Test that when percent escaping, no ASCII value is escaped (excluding %).
  for (uint8_t c = 0; c <= 0x7F; ++c) {
    if (c == '%')
      continue;

    std::string s;
    s.push_back(c);

    EXPECT_EQ("%ESCAPED:\xE2\x80\x8B %E2 " + s, GetNetLogString("\xE2 " + s));
  }
}

TEST(NetLogValuesTest, NetLogBinaryValue) {
  // Test the encoding for empty bytes.
  auto value1 = NetLogBinaryValue(nullptr, 0);
  ASSERT_TRUE(value1.is_string());
  EXPECT_EQ("", value1.GetString());

  // Test the encoding for a non-empty sequence (which needs padding).
  const uint8_t kBytes[] = {0x00, 0xF3, 0xF8, 0xFF};
  auto value2 = NetLogBinaryValue(kBytes, std::size(kBytes));
  ASSERT_TRUE(value2.is_string());
  EXPECT_EQ("APP4/w==", value2.GetString());
}

template <typename T>
std::string SerializedNetLogNumber(T num) {
  auto value = NetLogNumberValue(num);

  EXPECT_TRUE(value.is_string() || value.is_int() || value.is_double());

  return SerializeNetLogValueToJson(value);
}

std::string SerializedNetLogInt64(int64_t num) {
  return SerializedNetLogNumber(num);
}

std::string SerializedNetLogUint64(uint64_t num) {
  return SerializedNetLogNumber(num);
}

TEST(NetLogValuesTest, NetLogNumberValue) {
  const int64_t kMinInt = std::numeric_limits<int32_t>::min();
  const int64_t kMaxInt = std::numeric_limits<int32_t>::max();

  // Numbers which can be represented by an INTEGER base::Value().
  EXPECT_EQ("0", SerializedNetLogInt64(0));
  EXPECT_EQ("0", SerializedNetLogUint64(0));
  EXPECT_EQ("-1", SerializedNetLogInt64(-1));
  EXPECT_EQ("-2147483648", SerializedNetLogInt64(kMinInt));
  EXPECT_EQ("2147483647", SerializedNetLogInt64(kMaxInt));

  // Numbers which are outside of the INTEGER range, but fit within a DOUBLE.
  EXPECT_EQ("-2147483649", SerializedNetLogInt64(kMinInt - 1));
  EXPECT_EQ("2147483648", SerializedNetLogInt64(kMaxInt + 1));
  EXPECT_EQ("4294967294", SerializedNetLogInt64(0xFFFFFFFF - 1));

  // kMaxSafeInteger is the same as JavaScript's Numbers.MAX_SAFE_INTEGER.
  const int64_t kMaxSafeInteger = 9007199254740991;  // 2^53 - 1

  // Numbers that can be represented with full precision by a DOUBLE.
  EXPECT_EQ("-9007199254740991", SerializedNetLogInt64(-kMaxSafeInteger));
  EXPECT_EQ("9007199254740991", SerializedNetLogInt64(kMaxSafeInteger));
  EXPECT_EQ("9007199254740991", SerializedNetLogUint64(kMaxSafeInteger));

  // Numbers that are just outside of the range of a DOUBLE need to be encoded
  // as strings.
  EXPECT_EQ("\"-9007199254740992\"",
            SerializedNetLogInt64(-kMaxSafeInteger - 1));
  EXPECT_EQ("\"9007199254740992\"", SerializedNetLogInt64(kMaxSafeInteger + 1));
  EXPECT_EQ("\"9007199254740992\"",
            SerializedNetLogUint64(kMaxSafeInteger + 1));

  // Test the 64-bit maximums.
  EXPECT_EQ("\"9223372036854775807\"",
            SerializedNetLogInt64(std::numeric_limits<int64_t>::max()));
  EXPECT_EQ("\"18446744073709551615\"",
            SerializedNetLogUint64(std::numeric_limits<uint64_t>::max()));
}

}  // namespace net
