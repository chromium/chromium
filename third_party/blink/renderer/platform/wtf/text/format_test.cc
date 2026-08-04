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

}  // namespace blink
