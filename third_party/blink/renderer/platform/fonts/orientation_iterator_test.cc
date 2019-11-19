// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct OrientationTestRun {
  const char* const text;
  OrientationIterator::RenderOrientation code;
};

struct OrientationExpectedRun {
  unsigned limit;
  OrientationIterator::RenderOrientation render_orientation;

  OrientationExpectedRun(
      unsigned the_limit,
      OrientationIterator::RenderOrientation the_render_orientation)
      : limit(the_limit), render_orientation(the_render_orientation) {}
};

class OrientationIteratorTest : public testing::Test {
 protected:
  void CheckRuns(const Vector<OrientationTestRun>& runs) {
    StringBuilder text;
    text.Ensure16Bit();
    Vector<OrientationExpectedRun> expect;
    for (auto& run : runs) {
      text.Append(String::FromUTF8(run.text));
      expect.push_back(OrientationExpectedRun(text.length(), run.code));
    }
    OrientationIterator orientation_iterator(text.Characters16(), text.length(),
                                             FontOrientation::kVerticalMixed);
    VerifyRuns(&orientation_iterator, expect);
  }

  void VerifyRuns(OrientationIterator* orientation_iterator,
                  const Vector<OrientationExpectedRun>& expect) {
    unsigned limit;
    OrientationIterator::RenderOrientation render_orientation;
    size_t run_count = 0;
    while (orientation_iterator->Consume(&limit, &render_orientation)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].limit, limit);
      ASSERT_EQ(expect[run_count].render_orientation, render_orientation);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

// TODO(esprehn): WTF::Vector should allow initialization from a literal.
#define CHECK_ORIENTATION(...)                                       \
  static const OrientationTestRun kRunsArray[] = __VA_ARGS__;        \
  Vector<OrientationTestRun> runs;                                   \
  runs.Append(kRunsArray, sizeof(kRunsArray) / sizeof(*kRunsArray)); \
  CheckRuns(runs);

TEST_F(OrientationIteratorTest, Empty) {
  String empty(g_empty_string16_bit);
  OrientationIterator orientation_iterator(empty.Characters16(), empty.length(),
                                           FontOrientation::kVerticalMixed);
  unsigned limit = 0;
  OrientationIterator::RenderOrientation orientation =
      OrientationIterator::kOrientationInvalid;
  DCHECK(!orientation_iterator.Consume(&limit, &orientation));
  ASSERT_EQ(limit, 0u);
  ASSERT_EQ(orientation, OrientationIterator::kOrientationInvalid);
}

TEST_F(OrientationIteratorTest, OneCharLatin) {
  CHECK_ORIENTATION({{"A", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, OneAceOfSpades) {
  CHECK_ORIENTATION({{"🂡", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, CombiningCircle) {
  CHECK_ORIENTATION({{"◌́◌̀◌̈◌̂◌̄◌̊", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, OneEthiopicSyllable) {
  CHECK_ORIENTATION({{"ጀ", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, JapaneseLetterlikeEnd) {
  CHECK_ORIENTATION(
      {{"いろは", OrientationIterator::kOrientationKeep},
       {"ℐℒℐℒℐℒℐℒℐℒℐℒℐℒ", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, LetterlikeJapaneseEnd) {
  CHECK_ORIENTATION({{"ℐ", OrientationIterator::kOrientationRotateSideways},
                     {"いろは", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, OneCharJapanese) {
  CHECK_ORIENTATION({{"い", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, Japanese) {
  CHECK_ORIENTATION(
      {{"いろはにほへと", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, IVS) {
  CHECK_ORIENTATION(
      {{"愉\xF3\xA0\x84\x81", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, MarkAtFirstCharRotated) {
  // Unicode General Category M should be combined with the previous base
  // character, but they have their own orientation if they appear at the
  // beginning of a run.
  // http://www.unicode.org/reports/tr50/#grapheme_clusters
  // https://drafts.csswg.org/css-writing-modes-3/#vertical-orientations
  // U+0300 COMBINING GRAVE ACCENT is Mn (Mark, Nonspacing) with Rotated.
  CHECK_ORIENTATION(
      {{"\xCC\x80", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, MarkAtFirstCharUpright) {
  // U+20DD COMBINING ENCLOSING CIRCLE is Me (Mark, Enclosing) with Upright.
  CHECK_ORIENTATION({{"\xE2\x83\x9D", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, MarksAtFirstCharUpright) {
  // U+20DD COMBINING ENCLOSING CIRCLE is Me (Mark, Enclosing) with Upright.
  // U+0300 COMBINING GRAVE ACCENT is Mn (Mark, Nonspacing) with Rotated.
  CHECK_ORIENTATION(
      {{"\xE2\x83\x9D\xCC\x80", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, MarksAtFirstCharUprightThenBase) {
  // U+20DD COMBINING ENCLOSING CIRCLE is Me (Mark, Enclosing) with Upright.
  // U+0300 COMBINING GRAVE ACCENT is Mn (Mark, Nonspacing) with Rotated.
  CHECK_ORIENTATION(
      {{"\xE2\x83\x9D\xCC\x80", OrientationIterator::kOrientationKeep},
       {"ABC\xE2\x83\x9D", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, JapaneseLatinMixedInside) {
  CHECK_ORIENTATION({{"いろはに", OrientationIterator::kOrientationKeep},
                     {"Abc", OrientationIterator::kOrientationRotateSideways},
                     {"ほへと", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, PunctuationJapanese) {
  CHECK_ORIENTATION({{".…¡", OrientationIterator::kOrientationRotateSideways},
                     {"ほへと", OrientationIterator::kOrientationKeep}});
}

TEST_F(OrientationIteratorTest, JapaneseLatinMixedOutside) {
  CHECK_ORIENTATION({{"Abc", OrientationIterator::kOrientationRotateSideways},
                     {"ほへと", OrientationIterator::kOrientationKeep},
                     {"Xyz", OrientationIterator::kOrientationRotateSideways}});
}

TEST_F(OrientationIteratorTest, JapaneseMahjonggMixed) {
  CHECK_ORIENTATION(
      {{"いろはに🀤ほへと", OrientationIterator::kOrientationKeep}});
}

}  // namespace blink
