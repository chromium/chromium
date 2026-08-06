// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

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

  // Zero-padding is not supported yet.
  EXPECT_NE("00042", Format("{:05}", 42));
  EXPECT_NE("-042", Format("{:04}", -42));
  EXPECT_EQ("12345", Format("{:02}", 12345));

  // Strings (Left-aligned / padded on right)
  EXPECT_EQ("abc  ", Format("{:5}", StringView("abc")));
  EXPECT_EQ("abc", Format("{:2}", StringView("abc")));

  // Multiple width specifiers
  EXPECT_EQ("  1 +   2 =   3", Format("{:3} + {:3} = {:3}", 1, 2, 3));
}

TEST(FormatTest, WidthDeathTest) {
  FormatArg args[] = {FormatArg(42)};
  // Non-digit in width
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:a}", FormatArgs(args)), "");
  // Missing closing brace
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:5", FormatArgs(args)), "");
  // Width out of bounds
  EXPECT_DEATH_IF_SUPPORTED(VFormat("{:4294967296}", FormatArgs(args)), "");
}

}  // namespace blink
