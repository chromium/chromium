// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/line_ending.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(LineEndingTest, NormalizeLineEndingsToCRLF) {
  EXPECT_EQ(String(""), NormalizeLineEndingsToCRLF(""));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\n"));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\r\n"));
  EXPECT_EQ(String("\r\n"), NormalizeLineEndingsToCRLF("\r"));

  EXPECT_EQ(String("abc\r\ndef\r\n"), NormalizeLineEndingsToCRLF("abc\rdef\n"));
}

TEST(LineEndingTest, NormalizeLineEndingsToLF) {
  const struct {
    const char* const test;
    const char* const expected;
  } kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\r\n", "\n"},
      {"\r", "\n"},
      {"abc\rdef\nghi\r\n", "abc\ndef\nghi\n"},
  };

  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.expected, NormalizeLineEndingsToLF(test.test));
  }

  for (const auto& test : kTestCases) {
    Vector<char> out;
    NormalizeLineEndingsToLF(test.test, out);
    EXPECT_EQ(std::string_view(test.expected), base::as_string_view(out));
  }

  // If no modification is needed, we should get the same StringImpl back.
  String no_change("foo\nbar\nbaz\n");
  EXPECT_EQ(no_change.Impl(), NormalizeLineEndingsToLF(no_change).Impl());
}

}  // namespace blink
