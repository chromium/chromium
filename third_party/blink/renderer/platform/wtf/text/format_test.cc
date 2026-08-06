// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

TEST(FormatTest, Basic) {
  String result = Format("Hello {}", StringView("world"));
  EXPECT_EQ("Hello world", result);
}

TEST(FormatTest, Integers) {
  String result = Format("{} + {} = {}", 1, 2, 3);
  EXPECT_EQ("1 + 2 = 3", result);

  String negative = Format("Negative: {}", -42);
  EXPECT_EQ("Negative: -42", negative);

  String unsigned_val = Format("Unsigned: {}", 42u);
  EXPECT_EQ("Unsigned: 42", unsigned_val);
}

TEST(FormatTest, StringsAndViews) {
  String str("blink");
  StringView view("WTF");
  String result =
      Format("{} / {} / {} / {}", str, view, AtomicString("content"), "std");
  EXPECT_EQ("blink / WTF / content / std", result);
}

TEST(FormatTest, Escapes) {
  String result = Format("{{ {} }}", 42);
  EXPECT_EQ("{ 42 }", result);

  String double_escape = Format("{{{{}}}}");
  EXPECT_EQ("{{}}", double_escape);
}

TEST(FormatTest, VFormatDirect) {
  FormatArg args[] = {FormatArg(100)};
  String result = VFormat("Value: {}", FormatArgs(args));
  EXPECT_EQ("Value: 100", result);
}

TEST(FormatTest, VFormatToDirect) {
  StringBuilder builder;
  builder.Append("Prefix: ");
  FormatArg args[] = {FormatArg(200)};
  VFormatTo(builder, "Value: {}", FormatArgs(args));
  EXPECT_EQ("Prefix: Value: 200", builder.ReleaseString());
}

TEST(FormatTest, WidthSpecifier) {
  // Empty specifier {:}
  EXPECT_EQ("42", Format("{:}", 42));
  EXPECT_EQ("abc", Format("{:}", StringView("abc")));

  // Integers (Right-aligned / padded on left)
  EXPECT_EQ("   42", Format("{:5}", 42));
  EXPECT_EQ(" -42", Format("{:4}", -42));
  EXPECT_EQ("12345", Format("{:2}", 12345));
  EXPECT_EQ("42", Format("{:0}", 42));

  // Zero-padding.
  EXPECT_EQ("00042", Format("{:05}", 42));
  EXPECT_EQ("-042", Format("{:04}", -42));
  EXPECT_EQ("12345", Format("{:02}", 12345));
  EXPECT_EQ("-12345", Format("{:04}", -12345));
  EXPECT_EQ("00042", Format("{:05}", 42u));

  // Strings (Left-aligned / padded on right)
  EXPECT_EQ("abc  ", Format("{:5}", StringView("abc")));
  EXPECT_EQ("abc  ", Format("{:05}", StringView("abc")));
  EXPECT_EQ("abc", Format("{:2}", StringView("abc")));

  // Multiple width specifiers
  EXPECT_EQ("  1 +   2 =   3", Format("{:3} + {:3} = {:3}", 1, 2, 3));
}

TEST(FormatTest, WidthDeathTest) {
  FormatArg args[] = {FormatArg(42)};
  // Missing closing brace
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:5", FormatArgs(args)), "");
  // Width out of bounds
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:4294967296}", FormatArgs(args)), "");
}

TEST(FormatTest, TypeSpecifierDecimal) {
  EXPECT_EQ("42", Format("{:d}", 42));
  EXPECT_EQ("-42", Format("{:d}", -42));
  EXPECT_EQ("   42", Format("{:5d}", 42));
  EXPECT_EQ("00042", Format("{:05d}", 42));
  EXPECT_EQ("-042", Format("{:04d}", -42));
  EXPECT_EQ("9223372036854775807",
            Format("{:d}", std::numeric_limits<int64_t>::max()));
  EXPECT_EQ("-9223372036854775808",
            Format("{:d}", std::numeric_limits<int64_t>::min()));
  EXPECT_EQ("18446744073709551615",
            Format("{:d}", std::numeric_limits<uint64_t>::max()));
}

TEST(FormatTest, TypeSpecifierHexLower) {
  EXPECT_EQ("0", Format("{:x}", 0));
  EXPECT_EQ("2a", Format("{:x}", 42));
  EXPECT_EQ("ff", Format("{:x}", 255));
  EXPECT_EQ("-2a", Format("{:x}", -42));
  EXPECT_EQ("   2a", Format("{:5x}", 42));
  EXPECT_EQ("0002a", Format("{:05x}", 42));
  EXPECT_EQ("-002a", Format("{:05x}", -42));
  EXPECT_EQ("2a", Format("{:x}", 42u));
  EXPECT_EQ("7fffffffffffffff",
            Format("{:x}", std::numeric_limits<int64_t>::max()));
  EXPECT_EQ("-8000000000000000",
            Format("{:x}", std::numeric_limits<int64_t>::min()));
  EXPECT_EQ("ffffffffffffffff",
            Format("{:x}", std::numeric_limits<uint64_t>::max()));
}

TEST(FormatTest, TypeSpecifierHexUpper) {
  EXPECT_EQ("0", Format("{:X}", 0));
  EXPECT_EQ("2A", Format("{:X}", 42));
  EXPECT_EQ("FF", Format("{:X}", 255));
  EXPECT_EQ("-2A", Format("{:X}", -42));
  EXPECT_EQ("   2A", Format("{:5X}", 42));
  EXPECT_EQ("0002A", Format("{:05X}", 42));
  EXPECT_EQ("-002A", Format("{:05X}", -42));
  EXPECT_EQ("2A", Format("{:X}", 42u));
  EXPECT_EQ("7FFFFFFFFFFFFFFF",
            Format("{:X}", std::numeric_limits<int64_t>::max()));
  EXPECT_EQ("-8000000000000000",
            Format("{:X}", std::numeric_limits<int64_t>::min()));
  EXPECT_EQ("FFFFFFFFFFFFFFFF",
            Format("{:X}", std::numeric_limits<uint64_t>::max()));
}

TEST(FormatTest, TypeSpecifierString) {
  EXPECT_EQ("abc", Format("{:s}", StringView("abc")));
  EXPECT_EQ("abc  ", Format("{:5s}", StringView("abc")));
}

TEST(FormatTest, TypeSpecifierDeathTest) {
  FormatArg int_args[] = {FormatArg(42)};
  FormatArg str_args[] = {FormatArg(StringView("abc"))};

  // String argument with integer type specifiers
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:d}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:x}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:X}", FormatArgs(str_args)), "");

  // Integer argument with string type specifier
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:s}", FormatArgs(int_args)), "");

  // Unsupported type specifier
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:z}", FormatArgs(int_args)), "");
}

}  // namespace blink
