// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

struct TestData {
  String ToString() const { return str8 ? String(str8) : String(str16); }

  const char* str8;
  const UChar* str16;
  std::vector<UChar32> chars;
} g_test_data[] = {
    // Empty strings.
    {"", nullptr, {}},
    {nullptr, u"", {}},
    // 8-bits strings.
    {"Ascii", nullptr, {'A', 's', 'c', 'i', 'i'}},
    // BMP 16-bits strings.
    {nullptr, u"\u30D0\u30CA\u30CA", {0x30D0, 0x30CA, 0x30CA}},
    {nullptr, u"A\u30D0X\u30CA", {'A', 0x30D0, 'X', 0x30CA}},
    // Non-BMP 16-bits strings.
    {nullptr, u"A\xD842\xDFB7X", {'A', 0x20BB7, 'X'}},
    // An unpaired lead surrogate.
    {nullptr, u"\xD800", {0xD800}},
    {nullptr, u"\xD842\xDFB7\xD800", {0x20BB7, 0xD800}},
};
class CodePointIteratorParamTest
    : public testing::Test,
      public testing::WithParamInterface<TestData> {};
INSTANTIATE_TEST_SUITE_P(CodePointIteratorTest,
                         CodePointIteratorParamTest,
                         testing::ValuesIn(g_test_data));

TEST_P(CodePointIteratorParamTest, Chars) {
  const auto& test = GetParam();
  const String string = test.ToString();
  std::vector<UChar32> chars;
  for (const UChar32 ch : string) {
    chars.push_back(ch);
  }
  EXPECT_THAT(chars, test.chars);

  const StringView view(string);
  chars.clear();
  for (const UChar32 ch : view) {
    chars.push_back(ch);
  }
  EXPECT_THAT(chars, test.chars);
}

// Test `operator++()` without calling `operator*()`.
TEST_P(CodePointIteratorParamTest, Length) {
  const auto& test = GetParam();
  const String string = test.ToString();
  wtf_size_t count = 0;
  for (auto iterator = string.begin(); iterator != string.end(); ++iterator) {
    ++count;
  }
  EXPECT_EQ(count, test.chars.size());

  const StringView view(string);
  count = 0;
  for (auto iterator = view.begin(); iterator != view.end(); ++iterator) {
    ++count;
  }
  EXPECT_EQ(count, test.chars.size());
}

TEST(CodePointIteratorTest, Equality) {
  StringView str1{"foo"};
  EXPECT_EQ(str1.begin(), str1.begin());
  EXPECT_EQ(str1.end(), str1.end());
  EXPECT_FALSE(str1.begin() == str1.end());

  StringView str2{"bar"};
  EXPECT_NE(str1.begin(), str2.begin());
  EXPECT_NE(str1.end(), str2.end());
  EXPECT_FALSE(str1.end() != str1.end());
}

}  // namespace

}  // namespace WTF
