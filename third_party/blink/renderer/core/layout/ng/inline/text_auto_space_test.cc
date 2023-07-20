// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/text_auto_space.h"

#include <gtest/gtest.h>

namespace blink {

namespace {

struct TypeData {
  UChar32 ch;
  TextAutoSpace::CharType type;
} g_type_data[] = {
    {' ', TextAutoSpace::kOther},
    {'0', TextAutoSpace::kLetterOrNumeral},
    {'A', TextAutoSpace::kLetterOrNumeral},
    {u'\u05D0', TextAutoSpace::kLetterOrNumeral},  // Hebrew Letter Alef
    {u'\u0E50', TextAutoSpace::kLetterOrNumeral},  // Thai Digit Zero
    {u'\u3041', TextAutoSpace::kIdeograph},        // Hiragana Letter Small A
    {u'\u30FB', TextAutoSpace::kOther},            // Katakana Middle Dot
    {u'\uFF21', TextAutoSpace::kOther},  // Fullwidth Latin Capital Letter A
    {U'\U00017000', TextAutoSpace::kLetterOrNumeral},  // Tangut Ideograph
    {U'\U00031350', TextAutoSpace::kIdeograph},  // CJK Unified Ideographs H
};

std::ostream& operator<<(std::ostream& ostream, const TypeData& type_data) {
  return ostream << "U+" << std::hex << type_data.ch;
}

class TextAutoSpaceTypeTest : public testing::Test,
                              public testing::WithParamInterface<TypeData> {};

INSTANTIATE_TEST_SUITE_P(TextAutoSpaceTest,
                         TextAutoSpaceTypeTest,
                         testing::ValuesIn(g_type_data));

TEST_P(TextAutoSpaceTypeTest, Char) {
  const auto& data = GetParam();
  EXPECT_EQ(TextAutoSpace::GetType(data.ch), data.type);
}

}  // namespace

}  // namespace blink
