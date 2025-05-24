// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_platform_api_util.h"

#include <string_view>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetPlatformApiUtilTest, CopyStringAndNulToSpan) {
  constexpr std::string_view kTestCases[] = {
      "",
      "1",
      "1234",
  };

  for (const auto& test_case : kTestCases) {
    char dest[5] = {'a', 'a', 'a', 'a', 'a'};
    CopyStringAndNulToSpan(test_case, dest);

    // A string_view that includes all of `dest`, including the terminating nul
    // and any characters after that.
    auto dest_string = base::as_string_view(base::span(dest));

    EXPECT_EQ(dest_string.substr(0u, test_case.size()), test_case);
    EXPECT_EQ(dest_string[test_case.size()], '\0');
    for (auto unmodified_char : dest_string.substr(test_case.size() + 1)) {
      EXPECT_EQ(unmodified_char, 'a');
    }
  }
}

TEST(NetPlatformApiUtilTest, SpanWithNulToStringView) {
  // Test span representation of an empty string, with and without a terminating
  // null.
  const char kTest1[] = "";
  EXPECT_EQ(kTest1, SpanMaybeWithNulToStringView(
                        base::span_with_nul_from_cstring(kTest1)));
  EXPECT_EQ(kTest1,
            SpanMaybeWithNulToStringView(base::span_from_cstring(kTest1)));

  // Test span representation of a string, with and without a terminating null.
  const char kTest2[] = "1234";
  EXPECT_EQ(kTest2, SpanMaybeWithNulToStringView(
                        base::span_with_nul_from_cstring(kTest2)));
  EXPECT_EQ(kTest2,
            SpanMaybeWithNulToStringView(base::span_from_cstring(kTest2)));

  // base::span() constructor that esctracts a length from an array refuses to
  // take string literals, due to ambiguity around the nul. In these cases, we
  // want not just the terminating null, but everything after it as well, so
  // have to use the pointer+length constructor, and use UNSAFE_BUFFERS to avoid
  // a compile error.
  const char kTest3[] = "1234\0";
  EXPECT_EQ("1234", SpanMaybeWithNulToStringView(
                        UNSAFE_BUFFERS(base::span(kTest3, sizeof(kTest3)))));

  const char kTest4[] =
      "1234"
      "\0"
      "5678";
  EXPECT_EQ("1234", SpanMaybeWithNulToStringView(
                        UNSAFE_BUFFERS(base::span(kTest4, sizeof(kTest4)))));

  const char kTest5[] =
      "1234"
      "\0"
      "5"
      "\0\0"
      "678";
  EXPECT_EQ("1234", SpanMaybeWithNulToStringView(
                        UNSAFE_BUFFERS(base::span(kTest5, sizeof(kTest5)))));
}

}  // namespace net
