// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/text_auto_space.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"

namespace blink {

namespace {

using testing::ElementsAre;

class TextAutoSpaceTest : public testing::Test {
 public:
  Vector<float> GetAdvances(const ShapeResult& shape_result) {
    Vector<CharacterRange> ranges;
    shape_result.IndividualCharacterRanges(&ranges);
    Vector<float> advances;
    for (const CharacterRange& range : ranges) {
      advances.push_back(range.Width());
    }
    return advances;
  }
};

TEST_F(TextAutoSpaceTest, Check8Bit) {
  for (UChar32 ch = 0; ch <= std::numeric_limits<uint8_t>::max(); ++ch) {
    EXPECT_NE(TextAutoSpace::GetType(ch), TextAutoSpace::kIdeograph);
  }
}

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

TEST_F(TextAutoSpaceTest, Unapply) {
  const float size = 40;
  const Font font = test::CreateAhemFont(size);
  HarfBuzzShaper shaper(u"01234");
  ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);
  EXPECT_THAT(GetAdvances(*result), ElementsAre(size, size, size, size, size));

  // Apply auto-spacing.
  const float spacing = TextAutoSpace::GetSpacingWidth(&font);
  result->ApplyTextAutoSpacing({{2, spacing}, {5, spacing}});
  const float with_spacing = size + spacing;
  EXPECT_THAT(GetAdvances(*result),
              ElementsAre(size, with_spacing, size, size, with_spacing));

  // Compute the line-end by unapplying the spacing.
  for (unsigned end_offset : {2u, 5u}) {
    const ShapeResult* line_end =
        result->UnapplyAutoSpacing(spacing, end_offset - 1, end_offset);
    DCHECK_EQ(line_end->Width(), size);

    // Check the original `result` is unchanged; i.e., still has auto-spacing.
    EXPECT_THAT(GetAdvances(*result),
                ElementsAre(size, with_spacing, size, size, with_spacing));
  }
}

}  // namespace

}  // namespace blink
