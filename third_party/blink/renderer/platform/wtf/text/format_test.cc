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

TEST(FormatTest, FormatTo) {
  StringBuilder builder;
  builder.Append("Prefix: ");
  FormatTo(builder, "Hello {}", StringView("world"));
  EXPECT_EQ("Prefix: Hello world", builder.ReleaseString());

  StringBuilder builder2;
  FormatTo(builder2, "No args");
  EXPECT_EQ("No args", builder2.ReleaseString());

  StringBuilder builder3;
  FormatTo(builder3, "{} + {} = {}", 1, 2, 3);
  EXPECT_EQ("1 + 2 = 3", builder3.ReleaseString());
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

TEST(FormatTest, TypeSpecifierPointer) {
  void* ptr = reinterpret_cast<void*>(0x12ab);
  const void* cptr = ptr;
  EXPECT_EQ("0x12ab", Format("{}", ptr));
  EXPECT_EQ("0x12ab", Format("{}", cptr));
  EXPECT_EQ("0x12ab", Format("{:}", ptr));
  EXPECT_EQ("0x12ab", Format("{:p}", ptr));
  EXPECT_EQ("0x12ab", Format("{:p}", cptr));
  EXPECT_EQ("0X12AB", Format("{:P}", ptr));
  EXPECT_EQ("0X12AB", Format("{:P}", cptr));
  EXPECT_EQ("0x0", Format("{}", nullptr));
  EXPECT_EQ("0x0", Format("{:}", nullptr));
  EXPECT_EQ("0x0", Format("{:p}", nullptr));
  EXPECT_EQ("0X0", Format("{:P}", nullptr));
  EXPECT_EQ("0x0", Format("{}", static_cast<void*>(nullptr)));
  EXPECT_EQ("0x0", Format("{:p}", static_cast<void*>(nullptr)));
  EXPECT_EQ("0X0", Format("{:P}", static_cast<void*>(nullptr)));

  // Width and padding
  EXPECT_EQ("  0x12ab", Format("{:8p}", ptr));
  EXPECT_EQ("  0X12AB", Format("{:8P}", ptr));
  EXPECT_EQ("0x0012ab", Format("{:08p}", ptr));
  EXPECT_EQ("0X0012AB", Format("{:08P}", ptr));
  EXPECT_EQ("0x12ab", Format("{:2p}", ptr));
  EXPECT_EQ("0X12AB", Format("{:2P}", ptr));
  EXPECT_EQ("   0x0", Format("{:6p}", nullptr));
  EXPECT_EQ("0x0000", Format("{:06p}", nullptr));
  EXPECT_EQ("   0X0", Format("{:6P}", nullptr));
  EXPECT_EQ("0X0000", Format("{:06P}", nullptr));
}

TEST(FormatTest, TypeSpecifierFloat) {
  EXPECT_EQ("3.141592653589793", Format("{}", 3.141592653589793));
  EXPECT_EQ("3.141592653589793", Format("{:g}", 3.141592653589793));
  EXPECT_EQ("3.141592653589793", Format("{:G}", 3.141592653589793));

  EXPECT_EQ("3.141592653589793", Format("{:}", 3.141592653589793));
  EXPECT_EQ("3.141590", Format("{:f}", 3.14159));
  EXPECT_EQ("3.14159e+00", Format("{:e}", 3.14159));
  EXPECT_EQ("3.14159E+00", Format("{:E}", 3.14159));

  // Width and padding
  EXPECT_EQ("  3.141590", Format("{:10f}", 3.14159));
  EXPECT_EQ("003.141590", Format("{:010f}", 3.14159));
  EXPECT_EQ("-3.141590", Format("{:09f}", -3.14159));
  EXPECT_EQ("-3.141590", Format("{:9f}", -3.14159));

  // Special values
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ("inf", Format("{:f}", kInfinity));
  EXPECT_EQ("-inf", Format("{:f}", -kInfinity));
  EXPECT_EQ("nan", Format("{:f}", kNaN));
  EXPECT_EQ("INF", Format("{:F}", kInfinity));
  EXPECT_EQ("-INF", Format("{:F}", -kInfinity));
  EXPECT_EQ("NAN", Format("{:F}", kNaN));
  EXPECT_EQ("INF", Format("{:E}", kInfinity));
  EXPECT_EQ("NAN", Format("{:G}", kNaN));

  // Precision
  EXPECT_EQ("3.14", Format("{:.2f}", 3.14159));
  EXPECT_EQ("3.1", Format("{:.2}", 3.14159));
  EXPECT_EQ("3.142e+00", Format("{:.3e}", 3.14159));
  EXPECT_EQ("3.14", Format("{:.3g}", 3.14159));
  EXPECT_EQ("  3.14", Format("{:6.2f}", 3.14159));
  EXPECT_EQ("003.14", Format("{:06.2f}", 3.14159));
  EXPECT_EQ("3e+00", Format("{:.0e}", 3.14159));
  EXPECT_EQ("3", Format("{:.0f}", 3.14159));
  EXPECT_EQ("3", Format("{:.0g}", 3.14159));
  EXPECT_EQ("3", Format("{:.1}", 3.14159));
  EXPECT_EQ("0.567", Format("{:.6g}", 0.567));
  EXPECT_EQ("150001", Format("{:.6g}", 150000.5));
}

TEST(FormatTest, TypeSpecifierDeathTest) {
  FormatArg int_args[] = {FormatArg(42)};
  FormatArg str_args[] = {FormatArg(StringView("abc"))};
  FormatArg ptr_args[] = {FormatArg(static_cast<const void*>(nullptr))};
  FormatArg double_args[] = {FormatArg(3.14)};

  // String argument with integer/pointer/float type specifiers
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:d}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:x}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:X}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:p}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:P}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:f}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:e}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:g}", FormatArgs(str_args)), "");

  // Integer argument with string/pointer/float type specifiers
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:s}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:p}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:P}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:f}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:e}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:g}", FormatArgs(int_args)), "");

  // Pointer argument with integer/string/float type specifiers
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:d}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:x}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:X}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:s}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:f}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:e}", FormatArgs(ptr_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:g}", FormatArgs(ptr_args)), "");

  // Double argument with integer/string/pointer type specifiers
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:d}", FormatArgs(double_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:x}", FormatArgs(double_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:X}", FormatArgs(double_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:s}", FormatArgs(double_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:p}", FormatArgs(double_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:P}", FormatArgs(double_args)), "");

  // Unsupported type specifier
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:z}", FormatArgs(int_args)), "");

  // Precision specified for non-floating-point types
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.2f}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.2}", FormatArgs(int_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.2s}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.2}", FormatArgs(str_args)), "");
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.2p}", FormatArgs(ptr_args)), "");

  // Invalid precision specifier
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:.}", FormatArgs(double_args)), "");
}

}  // namespace blink
