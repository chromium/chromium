// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

#include <unicode/uscript.h>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

class HarfBuzzShapingLineBreaker : public ShapingLineBreaker {
  STACK_ALLOCATED();

 public:
  HarfBuzzShapingLineBreaker(const HarfBuzzShaper* shaper,
                             const Font* font,
                             const ShapeResult* result,
                             const LazyLineBreakIterator* break_iterator,
                             const Hyphenation* hyphenation)
      : ShapingLineBreaker(result, break_iterator, hyphenation, font),
        shaper_(shaper),
        font_(font) {}

 protected:
  ShapeResult* Shape(unsigned start, unsigned end, ShapeOptions options) final {
    return shaper_->Shape(font_, GetShapeResult().Direction(), start, end);
  }

  const HarfBuzzShaper* shaper_;
  const Font* font_;
};

const ShapeResultView* ShapeLine(ShapingLineBreaker* breaker,
                                 unsigned start_offset,
                                 LayoutUnit available_space,
                                 unsigned* break_offset) {
  ShapingLineBreaker::Result result;
  const ShapeResultView* shape_result =
      breaker->ShapeLine(start_offset, available_space, &result);
  *break_offset = result.break_offset;
  return shape_result;
}

}  // namespace

class ShapingLineBreakerTest : public FontTestBase {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
  }

  void SelectLucidaFont() {
    font_description.SetFamily(
        FontFamily(AtomicString("Lucida Grande"), FontFamily::Type::kFamilyName,
                   SharedFontFamily::Create(AtomicString("Lucida Medium"),
                                            FontFamily::Type::kFamilyName)));
  }

  void TearDown() override {}

  // Compute all break positions by |NextBreakOpportunity|.
  Vector<unsigned> BreakPositionsByNext(const ShapingLineBreaker& breaker,
                                        const String& string) {
    Vector<unsigned> break_positions;
    for (unsigned i = 0; i <= string.length(); i++) {
      unsigned next =
          breaker.NextBreakOpportunity(i, 0, string.length()).offset;
      if (break_positions.empty() || break_positions.back() != next)
        break_positions.push_back(next);
    }
    return break_positions;
  }

  // Compute all break positions by |PreviousBreakOpportunity|.
  Vector<unsigned> BreakPositionsByPrevious(const ShapingLineBreaker& breaker,
                                            const String& string) {
    Vector<unsigned> break_positions;
    for (unsigned i = string.length(); i; i--) {
      unsigned previous = breaker.PreviousBreakOpportunity(i, 0).offset;
      if (previous &&
          (break_positions.empty() || break_positions.back() != previous))
        break_positions.push_back(previous);
    }
    break_positions.Reverse();
    return break_positions;
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
};

TEST_F(ShapingLineBreakerTest, ShapeLineLatin) {
  Font font(font_description);

  String string = To16Bit(
      "Test run with multiple words and breaking "
      "opportunities.");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction);

  // "Test run with multiple"
  const ShapeResult* first4 = shaper.Shape(&font, direction, 0, 22);
  ASSERT_LT(first4->SnappedWidth(), result->SnappedWidth());

  // "Test run with"
  const ShapeResult* first3 = shaper.Shape(&font, direction, 0, 13);
  ASSERT_LT(first3->SnappedWidth(), first4->SnappedWidth());

  // "Test run"
  const ShapeResult* first2 = shaper.Shape(&font, direction, 0, 8);
  ASSERT_LT(first2->SnappedWidth(), first3->SnappedWidth());

  // "Test"
  const ShapeResult* first1 = shaper.Shape(&font, direction, 0, 4);
  ASSERT_LT(first1->SnappedWidth(), first2->SnappedWidth());

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  const ShapeResultView* line = nullptr;
  unsigned break_offset = 0;

  // Test the case where the entire string fits.
  line = ShapeLine(&breaker, 0, result->SnappedWidth(), &break_offset);
  EXPECT_EQ(56u, break_offset);  // After the end of the string.
  EXPECT_EQ(result->SnappedWidth(), line->SnappedWidth());

  // Test cases where we break between words.
  line = ShapeLine(&breaker, 0, first4->SnappedWidth(), &break_offset);
  EXPECT_EQ(22u, break_offset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first4->SnappedWidth() + 10, &break_offset);
  EXPECT_EQ(22u, break_offset);  // Between "multiple" and " words"
  EXPECT_EQ(first4->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first4->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(13u, break_offset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first3->SnappedWidth(), &break_offset);
  EXPECT_EQ(13u, break_offset);  // Between "width" and "multiple"
  EXPECT_EQ(first3->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first3->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(8u, break_offset);  // Between "run" and "width"
  EXPECT_EQ(first2->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first2->SnappedWidth(), &break_offset);
  EXPECT_EQ(8u, break_offset);  // Between "run" and "width"
  EXPECT_EQ(first2->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first2->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 0, first1->SnappedWidth(), &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());

  // Test the case where we cannot break earlier.
  line = ShapeLine(&breaker, 0, first1->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(4u, break_offset);  // Between "Test" and "run"
  EXPECT_EQ(first1->SnappedWidth(), line->SnappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinMultiLine) {
  Font font(font_description);

  String string = To16Bit("Line breaking test case.");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction);
  const ShapeResult* first = shaper.Shape(&font, direction, 0, 4);
  const ShapeResult* mid_third = shaper.Shape(&font, direction, 0, 16);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  unsigned break_offset = 0;

  ShapeLine(&breaker, 0, result->SnappedWidth() - 1, &break_offset);
  EXPECT_EQ(18u, break_offset);

  ShapeLine(&breaker, 0, first->SnappedWidth(), &break_offset);
  EXPECT_EQ(4u, break_offset);

  ShapeLine(&breaker, 0, mid_third->SnappedWidth(), &break_offset);
  EXPECT_EQ(13u, break_offset);

  ShapeLine(&breaker, 13u, mid_third->SnappedWidth(), &break_offset);
  EXPECT_EQ(24u, break_offset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineLatinBreakAll) {
  Font font(font_description);

  String string = To16Bit("Testing break type-break all.");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kBreakAll);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction);
  const ShapeResult* midpoint = shaper.Shape(&font, direction, 0, 16);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  const ShapeResultView* line;
  unsigned break_offset = 0;

  line = ShapeLine(&breaker, 0, midpoint->SnappedWidth(), &break_offset);
  EXPECT_EQ(16u, break_offset);
  EXPECT_EQ(midpoint->SnappedWidth(), line->SnappedWidth());

  line = ShapeLine(&breaker, 16u, result->SnappedWidth(), &break_offset);
  EXPECT_EQ(29u, break_offset);
  EXPECT_GE(midpoint->SnappedWidth(), line->SnappedWidth());
}

TEST_F(ShapingLineBreakerTest, ShapeLineZeroAvailableWidth) {
  Font font(font_description);

  String string(u"Testing overflow line break.");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  unsigned break_offset = 0;
  LayoutUnit zero(0);

  ShapeLine(&breaker, 0, zero, &break_offset);
  EXPECT_EQ(7u, break_offset);

  ShapeLine(&breaker, 8, zero, &break_offset);
  EXPECT_EQ(16u, break_offset);

  ShapeLine(&breaker, 17, zero, &break_offset);
  EXPECT_EQ(21u, break_offset);

  ShapeLine(&breaker, 22, zero, &break_offset);
  EXPECT_EQ(28u, break_offset);
}

TEST_F(ShapingLineBreakerTest, ShapeLineRangeEndMidWord) {
  Font font(font_description);

  String string(u"Mid word");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kNormal);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction, 0, 2);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  const ShapeResultView* line;
  unsigned break_offset = 0;

  line = ShapeLine(&breaker, 0, LayoutUnit::Max(), &break_offset);
  EXPECT_EQ(2u, break_offset);
  EXPECT_EQ(result->Width(), line->Width());
}

TEST_F(ShapingLineBreakerTest, ShapeLineWithLucidaFont) {
  SelectLucidaFont();
  Font font(font_description);

  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;

  //              012345678901234567890123456789012345
  String string(u"Lorem ipsum, consexx porttitxx. xxx");
  LazyLineBreakIterator break_iterator(string, AtomicString("en-US"),
                                       LineBreakType::kNormal);
  // In LayoutNG we use kAfterSpaceRun as TextBreakIterator`s default behavior.
  break_iterator.SetBreakSpace(BreakSpaceType::kAfterSpaceRun);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, direction, 0, 35);
  const ShapeResult* segment1 = shaper.Shape(&font, direction, 13, 31);
  const ShapeResult* segment2 = shaper.Shape(&font, direction, 13, 32);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  const ShapeResultView* line;
  unsigned break_offset = 0;

  line = ShapeLine(&breaker, 13, segment1->SnappedWidth(), &break_offset);
  EXPECT_EQ(31u, break_offset);
  EXPECT_EQ(segment1->Width(), line->Width());

  line = ShapeLine(&breaker, 13, segment2->SnappedWidth(), &break_offset);
  EXPECT_EQ(31u, break_offset);
  EXPECT_EQ(segment1->Width(), line->Width());
}

TEST_F(ShapingLineBreakerTest, HanKerningCloseUnsafe) {
  // Create a condition where all of the following are true:
  // 1. `ShouldTrimEnd(text_spacing_trim_)` (default).
  // 2. The candidate break is `Character::MaybeHanKerningClose`; e.g., U+FF09.
  // 3. After the candidate break is breakable.
  Font font(font_description);
  String string{u"x\uFF09\u3042"};
  HarfBuzzShaper shaper(string);
  ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);
  // 4. `ShapeResult::StartIndex` isn't 0.
  const unsigned start_offset = 1;
  ShapeResult* sub_result = result->SubRange(start_offset, result->EndIndex());
  // 5. The candidate break isn't safe to break.
  const unsigned unsafe_offsets[]{1};
  sub_result->AddUnsafeToBreak(unsafe_offsets);
  const LayoutUnit available_width =
      LayoutUnit::FromFloatFloor(sub_result->PositionForOffset(1)) - 1;

  LazyLineBreakIterator break_iterator(string);
  HarfBuzzShapingLineBreaker breaker(&shaper, &font, sub_result,
                                     &break_iterator, nullptr);
  unsigned break_offset = 0;
  ShapeLine(&breaker, start_offset, available_width, &break_offset);
  EXPECT_EQ(break_offset, 2u);
}

struct BreakOpportunityTestData {
  const char16_t* string;
  Vector<unsigned> break_positions;
  Vector<unsigned> break_positions_with_soft_hyphen_disabled;
};

class BreakOpportunityTest
    : public ShapingLineBreakerTest,
      public testing::WithParamInterface<BreakOpportunityTestData> {};

INSTANTIATE_TEST_SUITE_P(
    ShapingLineBreakerTest,
    BreakOpportunityTest,
    testing::Values(BreakOpportunityTestData{u"x y z", {2, 4, 5}},
                    BreakOpportunityTestData{u"y\xADz", {2, 3}, {3}},
                    BreakOpportunityTestData{u"\xADz", {1, 2}, {2}},
                    BreakOpportunityTestData{u"y\xAD", {2}, {2}},
                    BreakOpportunityTestData{u"\xAD\xADz", {2, 3}, {3}}));

TEST_P(BreakOpportunityTest, Next) {
  Font font(font_description);

  const BreakOpportunityTestData& data = GetParam();
  String string(data.string);
  LazyLineBreakIterator break_iterator(string);

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  EXPECT_THAT(BreakPositionsByNext(breaker, string),
              testing::ElementsAreArray(data.break_positions));

  if (!data.break_positions_with_soft_hyphen_disabled.empty()) {
    break_iterator.EnableSoftHyphen(false);
    EXPECT_THAT(BreakPositionsByNext(breaker, string),
                testing::ElementsAreArray(
                    data.break_positions_with_soft_hyphen_disabled));
  }
}

TEST_P(BreakOpportunityTest, Previous) {
  Font font(font_description);

  const BreakOpportunityTestData& data = GetParam();
  String string(data.string);
  LazyLineBreakIterator break_iterator(string);
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(&font, TextDirection::kLtr);

  HarfBuzzShapingLineBreaker breaker(&shaper, &font, result, &break_iterator,
                                     nullptr);
  EXPECT_THAT(BreakPositionsByPrevious(breaker, string),
              testing::ElementsAreArray(data.break_positions));

  if (!data.break_positions_with_soft_hyphen_disabled.empty()) {
    break_iterator.EnableSoftHyphen(false);
    EXPECT_THAT(BreakPositionsByPrevious(breaker, string),
                testing::ElementsAreArray(
                    data.break_positions_with_soft_hyphen_disabled));
  }
}

}  // namespace blink
