// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"

#include <unicode/uscript.h>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

using testing::ElementsAre;

namespace blink {

namespace {

ShapeResultTestInfo* TestInfo(const scoped_refptr<ShapeResult>& result) {
  return static_cast<ShapeResultTestInfo*>(result.get());
}

// Test helper to compare all RunInfo with the expected array.
struct ShapeResultRunData {
  unsigned start_index;
  unsigned num_characters;
  unsigned num_glyphs;
  hb_script_t script;

  static Vector<ShapeResultRunData> Get(
      const scoped_refptr<ShapeResult>& result) {
    const ShapeResultTestInfo* test_info = TestInfo(result);
    const unsigned num_runs = test_info->NumberOfRunsForTesting();
    Vector<ShapeResultRunData> runs(num_runs);
    for (unsigned i = 0; i < num_runs; i++) {
      ShapeResultRunData& run = runs[i];
      test_info->RunInfoForTesting(i, run.start_index, run.num_characters,
                                   run.num_glyphs, run.script);
    }
    return runs;
  }
};

bool operator==(const ShapeResultRunData& x, const ShapeResultRunData& y) {
  return x.start_index == y.start_index &&
         x.num_characters == y.num_characters && x.num_glyphs == y.num_glyphs &&
         x.script == y.script;
}

void operator<<(std::ostream& output, const ShapeResultRunData& x) {
  output << "{ start_index=" << x.start_index
         << ", num_characters=" << x.num_characters
         << ", num_glyphs=" << x.num_glyphs << ", script=" << x.script << " }";
}

// Create a string of the specified length, filled with |ch|.
String CreateStringOf(UChar ch, unsigned length) {
  UChar* data;
  String string(StringImpl::CreateUninitialized(length, data));
  string.Fill(ch);
  return string;
}

}  // namespace

class HarfBuzzShaperTest : public FontTestBase {
 protected:
  void SetUp() override {
    font_description_.SetComputedSize(12.0);
    font_ = Font(font_description_);
  }

  void TearDown() override {}

  void SelectDevanagariFont() {
    FontFamily devanagari_family;
    // Windows 10
    devanagari_family.SetFamily(AtomicString("Nirmala UI"),
                                FontFamily::Type::kFamilyName);
    // Windows 7
    devanagari_family.AppendFamily(AtomicString("Mangal"),
                                   FontFamily::Type::kFamilyName);
    // Linux
    devanagari_family.AppendFamily(AtomicString("Lohit Devanagari"),
                                   FontFamily::Type::kFamilyName);
    // Mac
    devanagari_family.AppendFamily(AtomicString("ITF Devanagari"),
                                   FontFamily::Type::kFamilyName);

    font_description_.SetFamily(devanagari_family);
    font_ = Font(font_description_);
  }

  Font CreateAhem(float size) {
    FontDescription::VariantLigatures ligatures;
    return blink::test::CreateTestFont(
        AtomicString("Ahem"), blink::test::PlatformTestDataPath("Ahem.woff"),
        size, &ligatures);
  }

  Font CreateNotoColorEmoji() {
    return blink::test::CreateTestFont(
        AtomicString("NotoColorEmoji"),
        blink::test::BlinkRootDir() +
            "/web_tests/third_party/NotoColorEmoji/NotoColorEmoji.ttf",
        12);
  }

  scoped_refptr<ShapeResult> SplitRun(scoped_refptr<ShapeResult> shape_result,
                                      unsigned offset) {
    unsigned length = shape_result->NumCharacters();
    scoped_refptr<ShapeResult> run2 = shape_result->SubRange(offset, length);
    shape_result = shape_result->SubRange(0, offset);
    run2->CopyRange(offset, length, shape_result.get());
    return shape_result;
  }

  scoped_refptr<ShapeResult> CreateMissingRunResult(TextDirection direction) {
    scoped_refptr<ShapeResult> result =
        ShapeResult::Create(&font_, 2, 8, direction);
    result->InsertRunForTesting(2, 1, direction, {0});
    result->InsertRunForTesting(3, 3, direction, {0, 1});
    // The character index 6 and 7 is missing.
    result->InsertRunForTesting(8, 2, direction, {0});
    return result;
  }

  FontCachePurgePreventer font_cache_purge_preventer_;
  FontDescription font_description_;
  Font font_;
  unsigned start_index_ = 0;
  unsigned num_characters_ = 0;
  unsigned num_glyphs_ = 0;
  hb_script_t script_ = HB_SCRIPT_INVALID;
};

class ScopedSubpixelOverride {
 public:
  explicit ScopedSubpixelOverride(bool b) {
    prev_subpixel_allowed_ =
        WebTestSupport::IsTextSubpixelPositioningAllowedForTest();
    prev_antialias_ = WebTestSupport::IsFontAntialiasingEnabledForTest();
    prev_fd_subpixel_ = FontDescription::SubpixelPositioning();

    if (b) {
      // Allow subpixel positioning.
      WebTestSupport::SetTextSubpixelPositioningAllowedForTest(true);

      // Now, enable subpixel positioning in platform-specific ways.

      // Mac always enables subpixel positioning.

      // On Windows, subpixel positioning also requires antialiasing.
      WebTestSupport::SetFontAntialiasingEnabledForTest(true);

      // On platforms other than Windows and Mac this needs to be set as
      // well.
      FontDescription::SetSubpixelPositioning(true);
    } else {
      // Explicitly disallow all subpixel positioning.
      WebTestSupport::SetTextSubpixelPositioningAllowedForTest(false);
    }
  }
  ~ScopedSubpixelOverride() {
    FontDescription::SetSubpixelPositioning(prev_fd_subpixel_);
    WebTestSupport::SetFontAntialiasingEnabledForTest(prev_antialias_);
    WebTestSupport::SetTextSubpixelPositioningAllowedForTest(
        prev_subpixel_allowed_);

    // Fonts cached with a different subpixel positioning state are not
    // automatically invalidated and need to be cleared between test
    // runs.
    FontCache::Get().Invalidate();
  }

 private:
  bool prev_subpixel_allowed_;
  bool prev_antialias_;
  bool prev_fd_subpixel_;
  // Web test mode (which is enabled by default for unit tests) is required
  // for all WebTestSupport settings to have effects.
  ScopedWebTestMode web_test_mode_{true};
};

class ShapeParameterTest : public HarfBuzzShaperTest,
                           public testing::WithParamInterface<TextDirection> {
 protected:
  scoped_refptr<ShapeResult> ShapeWithParameter(HarfBuzzShaper* shaper) {
    TextDirection direction = GetParam();
    return shaper->Shape(&font_, direction);
  }
};

INSTANTIATE_TEST_SUITE_P(HarfBuzzShaperTest,
                         ShapeParameterTest,
                         testing::Values(TextDirection::kLtr,
                                         TextDirection::kRtl));

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLatin) {
  String latin_common = To16Bit("ABC DEF.", 8);
  HarfBuzzShaper shaper(latin_common);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(0, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(0u, start_index_);
  EXPECT_EQ(8u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_LATIN, script_);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLeadingCommon) {
  String leading_common = To16Bit("... test", 8);
  HarfBuzzShaper shaper(leading_common);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(0, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(0u, start_index_);
  EXPECT_EQ(8u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_LATIN, script_);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsUnicodeVariants) {
  struct {
    const char* name;
    UChar string[4];
    unsigned length;
    hb_script_t script;
  } testlist[] = {
      {"Standard Variants text style", {0x30, 0xFE0E}, 2, HB_SCRIPT_COMMON},
      {"Standard Variants emoji style", {0x203C, 0xFE0F}, 2, HB_SCRIPT_COMMON},
      {"Standard Variants of Ideograph", {0x4FAE, 0xFE00}, 2, HB_SCRIPT_HAN},
      {"Ideographic Variants", {0x3402, 0xDB40, 0xDD00}, 3, HB_SCRIPT_HAN},
      {"Not-defined Variants", {0x41, 0xDB40, 0xDDEF}, 3, HB_SCRIPT_LATIN},
  };
  for (auto& test : testlist) {
    HarfBuzzShaper shaper(test.string);
    scoped_refptr<ShapeResult> result =
        shaper.Shape(&font_, TextDirection::kLtr);

    EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting()) << test.name;
    ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(0, start_index_,
                                                    num_glyphs_, script_))
        << test.name;
    EXPECT_EQ(0u, start_index_) << test.name;
    if (num_glyphs_ == 2) {
      // If the specified VS is not in the font, it's mapped to .notdef.
      // then hb_ot_hide_default_ignorables() swaps it to a space with
      // zero-advance.
      // http://lists.freedesktop.org/archives/harfbuzz/2015-May/004888.html
      EXPECT_EQ(TestInfo(result)->FontDataForTesting(0)->SpaceGlyph(),
                TestInfo(result)->GlyphForTesting(0, 1))
          << test.name;
      EXPECT_EQ(0.f, TestInfo(result)->AdvanceForTesting(0, 1)) << test.name;
    } else {
      EXPECT_EQ(1u, num_glyphs_) << test.name;
    }
    EXPECT_EQ(test.script, script_) << test.name;
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommon) {
  SelectDevanagariFont();
  UChar devanagari_common_string[] = {0x915, 0x94d, 0x930, 0x28, 0x20, 0x29};
  String devanagari_common_latin(devanagari_common_string, 6u);
  HarfBuzzShaper shaper(devanagari_common_latin);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  // Depending on font coverage we cannot assume that all text is in one
  // run, the parenthesis U+0029 may be in a separate font.
  EXPECT_GT(TestInfo(result)->NumberOfRunsForTesting(), 0u);
  EXPECT_LE(TestInfo(result)->NumberOfRunsForTesting(), 2u);

  // Common part of the run must be resolved as Devanagari.
  for (unsigned i = 0; i < TestInfo(result)->NumberOfRunsForTesting(); ++i) {
    ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(i, start_index_,
                                                    num_glyphs_, script_));
    EXPECT_EQ(HB_SCRIPT_DEVANAGARI, script_);
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommonLatinCommon) {
  SelectDevanagariFont();
  UChar devanagari_common_latin_string[] = {0x915, 0x94d, 0x930, 0x20,
                                            0x61,  0x62,  0x2E};
  HarfBuzzShaper shaper(String(devanagari_common_latin_string, 7u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  // Ensure that there are only two scripts, Devanagari first, then Latin.
  EXPECT_GT(TestInfo(result)->NumberOfRunsForTesting(), 0u);
  EXPECT_LE(TestInfo(result)->NumberOfRunsForTesting(), 3u);

  bool finished_devanagari = false;
  for (unsigned i = 0; i < TestInfo(result)->NumberOfRunsForTesting(); ++i) {
    ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(i, start_index_,
                                                    num_glyphs_, script_));
    finished_devanagari = finished_devanagari | (script_ == HB_SCRIPT_LATIN);
    EXPECT_EQ(script_,
              finished_devanagari ? HB_SCRIPT_LATIN : HB_SCRIPT_DEVANAGARI);
  }
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatin) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(String(mixed_string, 6u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  EXPECT_EQ(4u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(0, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(0u, start_index_);
  EXPECT_EQ(3u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script_);

  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(1, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(3u, start_index_);
  EXPECT_EQ(1u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_THAI, script_);

  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(2, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(4u, start_index_);
  EXPECT_EQ(1u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_HAN, script_);

  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(3, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(5u, start_index_);
  EXPECT_EQ(1u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_LATIN, script_);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatinTwice) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(String(mixed_string, 6u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  EXPECT_EQ(4u, TestInfo(result)->NumberOfRunsForTesting());

  // Shape again on the same shape object and check the number of runs.
  // Should be equal if no state was retained between shape calls.
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, TextDirection::kLtr);
  EXPECT_EQ(4u, TestInfo(result2)->NumberOfRunsForTesting());
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  HarfBuzzShaper shaper(String(arabic_string, 3u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kRtl);

  EXPECT_EQ(1u, TestInfo(result)->NumberOfRunsForTesting());
  ASSERT_TRUE(TestInfo(result)->RunInfoForTesting(0, start_index_, num_glyphs_,
                                                  script_));
  EXPECT_EQ(0u, start_index_);
  EXPECT_EQ(3u, num_glyphs_);
  EXPECT_EQ(HB_SCRIPT_ARABIC, script_);
}

// This is a simplified test and doesn't accuratly reflect how the shape range
// is to be used. If you instead of the string you imagine the following HTML:
// <div>Hello <span>World</span>!</div>
// It better reflects the intended use where the range given to each shape call
// corresponds to the text content of a TextNode.
TEST_F(HarfBuzzShaperTest, ShapeLatinSegment) {
  String string("Hello World!", 12u);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> combined = shaper.Shape(&font_, direction);
  scoped_refptr<ShapeResult> first = shaper.Shape(&font_, direction, 0, 6);
  scoped_refptr<ShapeResult> second = shaper.Shape(&font_, direction, 6, 11);
  scoped_refptr<ShapeResult> third = shaper.Shape(&font_, direction, 11, 12);

  ASSERT_TRUE(TestInfo(first)->RunInfoForTesting(
      0, start_index_, num_characters_, num_glyphs_, script_));
  EXPECT_EQ(0u, start_index_);
  EXPECT_EQ(6u, num_characters_);
  ASSERT_TRUE(TestInfo(second)->RunInfoForTesting(
      0, start_index_, num_characters_, num_glyphs_, script_));
  EXPECT_EQ(6u, start_index_);
  EXPECT_EQ(5u, num_characters_);
  ASSERT_TRUE(TestInfo(third)->RunInfoForTesting(
      0, start_index_, num_characters_, num_glyphs_, script_));
  EXPECT_EQ(11u, start_index_);
  EXPECT_EQ(1u, num_characters_);

  HarfBuzzShaper shaper2(string.Substring(0, 6));
  scoped_refptr<ShapeResult> first_reference = shaper2.Shape(&font_, direction);

  HarfBuzzShaper shaper3(string.Substring(6, 5));
  scoped_refptr<ShapeResult> second_reference =
      shaper3.Shape(&font_, direction);

  HarfBuzzShaper shaper4(string.Substring(11, 1));
  scoped_refptr<ShapeResult> third_reference = shaper4.Shape(&font_, direction);

  // Width of each segment should be the same when shaped using start and end
  // offset as it is when shaping the three segments using separate shaper
  // instances.
  // A full pixel is needed for tolerance to account for kerning on some
  // platforms.
  ASSERT_NEAR(first_reference->Width(), first->Width(), 1);
  ASSERT_NEAR(second_reference->Width(), second->Width(), 1);
  ASSERT_NEAR(third_reference->Width(), third->Width(), 1);

  // Width of shape results for the entire string should match the combined
  // shape results from the three segments.
  float total_width = first->Width() + second->Width() + third->Width();
  ASSERT_NEAR(combined->Width(), total_width, 1);
}

// Represents the case where a part of a cluster has a different color.
// <div>0x647<span style="color: red;">0x64A</span></
// Cannot be enabled on Apple yet, compare
// https:// https://github.com/harfbuzz/harfbuzz/issues/1415
#if BUILDFLAG(IS_APPLE)
#define MAYBE_ShapeArabicWithContext DISABLED_ShapeArabicWithContext
#else
#define MAYBE_ShapeArabicWithContext ShapeArabicWithContext
#endif
TEST_F(HarfBuzzShaperTest, MAYBE_ShapeArabicWithContext) {
  UChar arabic_string[] = {0x647, 0x64A};
  HarfBuzzShaper shaper(String(arabic_string, 2u));

  scoped_refptr<ShapeResult> combined =
      shaper.Shape(&font_, TextDirection::kRtl);

  scoped_refptr<ShapeResult> first =
      shaper.Shape(&font_, TextDirection::kRtl, 0, 1);
  scoped_refptr<ShapeResult> second =
      shaper.Shape(&font_, TextDirection::kRtl, 1, 2);

  // Combined width should be the same when shaping the two characters
  // separately as when shaping them combined.
  ASSERT_NEAR(combined->Width(), first->Width() + second->Width(), 0.1);
}

TEST_F(HarfBuzzShaperTest, ShapeTabulationCharacters) {
  const unsigned length = HarfBuzzRunGlyphData::kMaxCharacters * 2 + 1;
  scoped_refptr<ShapeResult> result =
      ShapeResult::CreateForTabulationCharacters(&font_, TextDirection::kLtr,
                                                 TabSize(8), 0.f, 0, length);
  EXPECT_EQ(result->NumCharacters(), length);
  EXPECT_EQ(result->NumGlyphs(), length);
}

TEST_F(HarfBuzzShaperTest, ShapeVerticalUpright) {
  font_description_.SetOrientation(FontOrientation::kVerticalUpright);
  font_ = Font(font_description_);

  // This string should create 2 runs, ideographic and Latin, both in upright.
  String string(u"\u65E5\u65E5\u65E5lllll");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Shape each run and merge them using CopyRange. Width() should match.
  scoped_refptr<ShapeResult> result1 = shaper.Shape(&font_, direction, 0, 3);
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, direction, 3, string.length());

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, 3, composite_result.get());
  result2->CopyRange(3, string.length(), composite_result.get());

  EXPECT_EQ(result->Width(), composite_result->Width());
}

TEST_F(HarfBuzzShaperTest, ShapeVerticalUprightIdeograph) {
  font_description_.SetOrientation(FontOrientation::kVerticalUpright);
  font_ = Font(font_description_);

  // This string should create one ideograph run.
  String string(u"\u65E5\u65E6\u65E0\u65D3\u65D0");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Shape each run and merge them using CopyRange. Width() should match.
  scoped_refptr<ShapeResult> result1 = shaper.Shape(&font_, direction, 0, 3);
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, direction, 3, string.length());

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, 3, composite_result.get());
  result2->CopyRange(3, string.length(), composite_result.get());

  // Rounding of x and width may be off by ~0.1 on Mac.
  float tolerance = 0.1f;
  EXPECT_NEAR(result->Width(), composite_result->Width(), tolerance);
}

TEST_F(HarfBuzzShaperTest, RangeShapeSmallCaps) {
  // Test passes if no assertion is hit of the ones below, but also the newly
  // introduced one in HarfBuzzShaper::ShapeSegment: DCHECK_GT(shape_end,
  // shape_start) is not hit.
  FontDescription font_description;
  font_description.SetVariantCaps(FontDescription::kSmallCaps);
  font_description.SetComputedSize(12.0);
  Font font(font_description);

  // Shaping index 2 to 3 means that case splitting for small caps splits before
  // character index 2 since the initial 'a' needs to be uppercased, but the
  // space character does not need to be uppercased. This triggered
  // crbug.com/817271.
  String string(u"a aa");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result =
      shaper.Shape(&font, TextDirection::kLtr, 2, 3);
  EXPECT_EQ(1u, result->NumCharacters());

  string = u"aa a";
  HarfBuzzShaper shaper_two(string);
  result = shaper_two.Shape(&font, TextDirection::kLtr, 3, 4);
  EXPECT_EQ(1u, result->NumCharacters());

  string = u"a aa";
  HarfBuzzShaper shaper_three(string);
  result = shaper_three.Shape(&font, TextDirection::kLtr, 1, 2);
  EXPECT_EQ(1u, result->NumCharacters());

  string = u"aa aa aa aa aa aa aa aa aa aa";
  HarfBuzzShaper shaper_four(string);
  result = shaper_four.Shape(&font, TextDirection::kLtr, 21, 23);
  EXPECT_EQ(2u, result->NumCharacters());

  string = u"aa aa aa aa aa aa aa aa aa aa";
  HarfBuzzShaper shaper_five(string);
  result = shaper_five.Shape(&font, TextDirection::kLtr, 27, 29);
  EXPECT_EQ(2u, result->NumCharacters());
}

TEST_F(HarfBuzzShaperTest, ShapeVerticalMixed) {
  font_description_.SetOrientation(FontOrientation::kVerticalMixed);
  font_ = Font(font_description_);

  // This string should create 2 runs, ideographic in upright and Latin in
  // rotated horizontal.
  String string(u"\u65E5\u65E5\u65E5lllll");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Shape each run and merge them using CopyRange. Width() should match.
  scoped_refptr<ShapeResult> result1 = shaper.Shape(&font_, direction, 0, 3);
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, direction, 3, string.length());

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, 3, composite_result.get());
  result2->CopyRange(3, string.length(), composite_result.get());

  EXPECT_EQ(result->Width(), composite_result->Width());
}

class ShapeStringTest : public HarfBuzzShaperTest,
                        public testing::WithParamInterface<const char16_t*> {};

INSTANTIATE_TEST_SUITE_P(HarfBuzzShaperTest,
                         ShapeStringTest,
                         testing::Values(
                             // U+FFF0 is not assigned as of Unicode 10.0.
                             u"\uFFF0",
                             u"\uFFF0Hello",
                             // U+00AD SOFT HYPHEN often does not have glyphs.
                             u"\u00AD"));

TEST_P(ShapeStringTest, MissingGlyph) {
  String string(GetParam());
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  EXPECT_EQ(0u, result->StartIndex());
  EXPECT_EQ(string.length(), result->EndIndex());
}

// Test splitting runs by kMaxCharacterIndex using a simple string that has code
// point:glyph:cluster are all 1:1.
TEST_P(ShapeParameterTest, MaxGlyphsSimple) {
  const unsigned length = HarfBuzzRunGlyphData::kMaxCharacters + 1;
  String string = CreateStringOf('X', length);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = ShapeWithParameter(&shaper);
  EXPECT_EQ(length, result->NumCharacters());
  EXPECT_EQ(length, result->NumGlyphs());
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);
  EXPECT_THAT(
      runs,
      IsLtr(GetParam())
          ? testing::ElementsAre(
                ShapeResultRunData{0, length - 1, length - 1, HB_SCRIPT_LATIN},
                ShapeResultRunData{length - 1, 1, 1, HB_SCRIPT_LATIN})
          : testing::ElementsAre(
                ShapeResultRunData{1, length - 1, length - 1, HB_SCRIPT_LATIN},
                ShapeResultRunData{0, 1, 1, HB_SCRIPT_LATIN}));
}

// 'X' + U+0300 COMBINING GRAVE ACCENT is a cluster, but most fonts do not have
// a pre-composed glyph for it, so code points and glyphs are 1:1. Because the
// length is "+1" and the last character is combining, this string does not hit
// kMaxCharacterIndex but hits kMaxCharacters.
TEST_P(ShapeParameterTest, MaxGlyphsClusterLatin) {
  const unsigned length = HarfBuzzRunGlyphData::kMaxCharacters + 1;
  String string = CreateStringOf('X', length);
  string.replace(1, 1, u"\u0300");  // U+0300 COMBINING GRAVE ACCENT
  string.replace(length - 2, 2, u"Z\u0300");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = ShapeWithParameter(&shaper);
  EXPECT_EQ(length, result->NumCharacters());
  EXPECT_EQ(length, result->NumGlyphs());
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);
  EXPECT_THAT(
      runs,
      IsLtr(GetParam())
          ? testing::ElementsAre(
                ShapeResultRunData{0, length - 2, length - 2, HB_SCRIPT_LATIN},
                ShapeResultRunData{length - 2, 2u, 2u, HB_SCRIPT_LATIN})
          : testing::ElementsAre(
                ShapeResultRunData{2, length - 2, length - 2, HB_SCRIPT_LATIN},
                ShapeResultRunData{0, 2, 2, HB_SCRIPT_LATIN}));
}

// Same as MaxGlyphsClusterLatin, but by making the length "+2", this string
// hits kMaxCharacterIndex.
TEST_P(ShapeParameterTest, MaxGlyphsClusterLatin2) {
  const unsigned length = HarfBuzzRunGlyphData::kMaxCharacters + 2;
  String string = CreateStringOf('X', length);
  string.replace(1, 1, u"\u0300");  // U+0300 COMBINING GRAVE ACCENT
  string.replace(length - 2, 2, u"Z\u0300");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = ShapeWithParameter(&shaper);
  EXPECT_EQ(length, result->NumCharacters());
  EXPECT_EQ(length, result->NumGlyphs());
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);
  EXPECT_THAT(
      runs,
      IsLtr(GetParam())
          ? testing::ElementsAre(
                ShapeResultRunData{0, length - 2, length - 2, HB_SCRIPT_LATIN},
                ShapeResultRunData{length - 2, 2u, 2u, HB_SCRIPT_LATIN})
          : testing::ElementsAre(
                ShapeResultRunData{2, length - 2, length - 2, HB_SCRIPT_LATIN},
                ShapeResultRunData{0, 2u, 2u, HB_SCRIPT_LATIN}));
}

TEST_P(ShapeParameterTest, MaxGlyphsClusterDevanagari) {
  const unsigned length = HarfBuzzRunGlyphData::kMaxCharacters + 1;
  String string = CreateStringOf(0x930, length);
  string.replace(0, 3, u"\u0930\u093F\u0902");
  string.replace(length - 3, 3, u"\u0930\u093F\u0902");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = ShapeWithParameter(&shaper);
  EXPECT_EQ(length, result->NumCharacters());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  // Linux and Fuchsia use Lohit Devanagari. When using that font the shaper
  // returns 32767 glyphs instead of 32769.
  // TODO(crbug.com/933551): Add Noto Sans Devanagari to
  // //third_party/test_fonts and use it here.
  if (result->NumGlyphs() != length)
    return;
#endif
  EXPECT_EQ(length, result->NumGlyphs());
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);
  EXPECT_THAT(
      runs,
      IsLtr(GetParam())
          ? testing::ElementsAre(
                ShapeResultRunData{0, length - 3, length - 3,
                                   HB_SCRIPT_DEVANAGARI},
                ShapeResultRunData{length - 3, 3u, 3u, HB_SCRIPT_DEVANAGARI})
          : testing::ElementsAre(
                ShapeResultRunData{3, length - 3, length - 3,
                                   HB_SCRIPT_DEVANAGARI},
                ShapeResultRunData{0, 3u, 3u, HB_SCRIPT_DEVANAGARI}));
}

TEST_P(ShapeParameterTest, ZeroWidthSpace) {
  UChar string[] = {kZeroWidthSpaceCharacter,
                    kZeroWidthSpaceCharacter,
                    0x0627,
                    0x0631,
                    0x062F,
                    0x0648,
                    kZeroWidthSpaceCharacter,
                    kZeroWidthSpaceCharacter};
  const unsigned length = std::size(string);
  HarfBuzzShaper shaper(String(string, length));
  scoped_refptr<ShapeResult> result = ShapeWithParameter(&shaper);
  EXPECT_EQ(0u, result->StartIndex());
  EXPECT_EQ(length, result->EndIndex());
#if DCHECK_IS_ON()
  result->CheckConsistency();
#endif
}

TEST_F(HarfBuzzShaperTest, IdeographicSpace) {
  String string(
      u"\u3001"    // IDEOGRAPHIC COMMA
      u"\u3000");  // IDEOGRAPHIC SPACE
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  Vector<ShapeResult::RunFontData> run_font_data;
  result->GetRunFontData(&run_font_data);
  EXPECT_EQ(run_font_data.size(), 1u);
}

TEST_F(HarfBuzzShaperTest, NegativeLetterSpacing) {
  String string(u"Hello");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  float width = result->Width();

  ShapeResultSpacing<String> spacing(string);
  FontDescription font_description;
  font_description.SetLetterSpacing(-5);
  spacing.SetSpacing(font_description);
  result->ApplySpacing(spacing);

  EXPECT_EQ(5 * 5, width - result->Width());
}

TEST_F(HarfBuzzShaperTest, NegativeLetterSpacingTo0) {
  String string(u"00000");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  float char_width = result->Width() / string.length();

  ShapeResultSpacing<String> spacing(string);
  FontDescription font_description;
  font_description.SetLetterSpacing(-char_width);
  spacing.SetSpacing(font_description);
  result->ApplySpacing(spacing);

  // EXPECT_EQ(0.0f, result->Width());
}

TEST_F(HarfBuzzShaperTest, NegativeLetterSpacingToNegative) {
  String string(u"00000");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  float char_width = result->Width() / string.length();

  ShapeResultSpacing<String> spacing(string);
  FontDescription font_description;
  font_description.SetLetterSpacing(-2 * char_width);
  spacing.SetSpacing(font_description);
  result->ApplySpacing(spacing);

  // CSS does not allow negative width, it should be clampled to 0.
  // EXPECT_EQ(0.0f, result->Width());
}

static struct GlyphDataRangeTestData {
  const char16_t* text;
  TextDirection direction;
  unsigned run_index;
  unsigned start_offset;
  unsigned end_offset;
  unsigned start_glyph;
  unsigned end_glyph;
} glyph_data_range_test_data[] = {
    // Hebrew, taken from fast/text/selection/hebrew-selection.html
    // The two code points form a grapheme cluster, which produces two glyphs.
    // Character index array should be [0, 0].
    {u"\u05E9\u05B0", TextDirection::kRtl, 0, 0, 1, 0, 2},
    // ZWJ tests taken from fast/text/international/zerowidthjoiner.html
    // Character index array should be [6, 3, 3, 3, 0, 0, 0].
    {u"\u0639\u200D\u200D\u0639\u200D\u200D\u0639", TextDirection::kRtl, 0, 0,
     1, 4, 7},
    {u"\u0639\u200D\u200D\u0639\u200D\u200D\u0639", TextDirection::kRtl, 0, 2,
     5, 1, 4},
    {u"\u0639\u200D\u200D\u0639\u200D\u200D\u0639", TextDirection::kRtl, 0, 4,
     7, 0, 1},
};

std::ostream& operator<<(std::ostream& ostream,
                         const GlyphDataRangeTestData& data) {
  return ostream << data.text;
}

class GlyphDataRangeTest
    : public HarfBuzzShaperTest,
      public testing::WithParamInterface<GlyphDataRangeTestData> {};

INSTANTIATE_TEST_SUITE_P(HarfBuzzShaperTest,
                         GlyphDataRangeTest,
                         testing::ValuesIn(glyph_data_range_test_data));

TEST_P(GlyphDataRangeTest, Data) {
  auto data = GetParam();
  String string(data.text);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, data.direction);

  const auto& run = TestInfo(result)->RunInfoForTesting(data.run_index);
  auto glyphs = run.FindGlyphDataRange(data.start_offset, data.end_offset);
  unsigned start_glyph = std::distance(run.glyph_data_.begin(), glyphs.begin);
  EXPECT_EQ(data.start_glyph, start_glyph);
  unsigned end_glyph = std::distance(run.glyph_data_.begin(), glyphs.end);
  EXPECT_EQ(data.end_glyph, end_glyph);
}

static struct OffsetForPositionTestData {
  float position;
  unsigned offset_ltr;
  unsigned offset_rtl;
  unsigned hit_test_ltr;
  unsigned hit_test_rtl;
  unsigned fit_ltr_ltr;
  unsigned fit_ltr_rtl;
  unsigned fit_rtl_ltr;
  unsigned fit_rtl_rtl;
} offset_for_position_fixed_pitch_test_data[] = {
    // The left edge.
    {-1, 0, 5, 0, 5, 0, 0, 5, 5},
    {0, 0, 5, 0, 5, 0, 0, 5, 5},
    // Hit test should round to the nearest glyph at the middle of a glyph.
    {4, 0, 4, 0, 5, 0, 1, 5, 4},
    {6, 0, 4, 1, 4, 0, 1, 5, 4},
    // Glyph boundary between the 1st and the 2nd glyph.
    // Avoid testing "10.0" to avoid rounding differences on Windows.
    {9.9, 0, 4, 1, 4, 0, 1, 5, 4},
    {10.1, 1, 3, 1, 4, 1, 2, 4, 3},
    // Run boundary is at position 20. The 1st run has 2 characters.
    {14, 1, 3, 1, 4, 1, 2, 4, 3},
    {16, 1, 3, 2, 3, 1, 2, 4, 3},
    {20.1, 2, 2, 2, 3, 2, 3, 3, 2},
    {24, 2, 2, 2, 3, 2, 3, 3, 2},
    {26, 2, 2, 3, 2, 2, 3, 3, 2},
    // The end of the ShapeResult. The result has 5 characters.
    {44, 4, 0, 4, 1, 4, 5, 1, 0},
    {46, 4, 0, 5, 0, 4, 5, 1, 0},
    {50, 5, 0, 5, 0, 5, 5, 0, 0},
    // Beyond the right edge of the ShapeResult.
    {51, 5, 0, 5, 0, 5, 5, 0, 0},
};

std::ostream& operator<<(std::ostream& ostream,
                         const OffsetForPositionTestData& data) {
  return ostream << data.position;
}

class OffsetForPositionTest
    : public HarfBuzzShaperTest,
      public testing::WithParamInterface<OffsetForPositionTestData> {};

INSTANTIATE_TEST_SUITE_P(
    HarfBuzzShaperTest,
    OffsetForPositionTest,
    testing::ValuesIn(offset_for_position_fixed_pitch_test_data));

TEST_P(OffsetForPositionTest, Data) {
  auto data = GetParam();
  String string(u"01234");
  HarfBuzzShaper shaper(string);
  Font ahem = CreateAhem(10);
  scoped_refptr<ShapeResult> result =
      SplitRun(shaper.Shape(&ahem, TextDirection::kLtr), 2);
  EXPECT_EQ(data.offset_ltr,
            result->OffsetForPosition(data.position, BreakGlyphsOption(false)));
  EXPECT_EQ(data.hit_test_ltr,
            result->CaretOffsetForHitTest(data.position, string,
                                          BreakGlyphsOption(false)));
  EXPECT_EQ(data.fit_ltr_ltr,
            result->OffsetToFit(data.position, TextDirection::kLtr));
  EXPECT_EQ(data.fit_ltr_rtl,
            result->OffsetToFit(data.position, TextDirection::kRtl));

  result = SplitRun(shaper.Shape(&ahem, TextDirection::kRtl), 3);
  EXPECT_EQ(data.offset_rtl,
            result->OffsetForPosition(data.position, BreakGlyphsOption(false)));
  EXPECT_EQ(data.hit_test_rtl,
            result->CaretOffsetForHitTest(data.position, string,
                                          BreakGlyphsOption(false)));
  EXPECT_EQ(data.fit_rtl_ltr,
            result->OffsetToFit(data.position, TextDirection::kLtr));
  EXPECT_EQ(data.fit_rtl_rtl,
            result->OffsetToFit(data.position, TextDirection::kRtl));
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetLatin) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);
  scoped_refptr<ShapeResult> first =
      shaper.Shape(&font_, direction, 0, 5);  // Hello
  scoped_refptr<ShapeResult> second =
      shaper.Shape(&font_, direction, 6, 11);  // World

  EXPECT_EQ(0.0f, result->PositionForOffset(0));
  ASSERT_NEAR(first->Width(), result->PositionForOffset(5), 1);
  ASSERT_NEAR(second->Width(),
              result->PositionForOffset(11) - result->PositionForOffset(6), 1);
  ASSERT_NEAR(result->Width(), result->PositionForOffset(12), 0.1);
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(String(arabic_string, 3u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  EXPECT_EQ(0.0f, result->PositionForOffset(3));
  ASSERT_NEAR(result->Width(), result->PositionForOffset(0), 0.1);
}

TEST_F(HarfBuzzShaperTest, EmojiZWJSequence) {
  UChar emoji_zwj_sequence[] = {0x270C, 0x200D, 0xD83C, 0xDFFF,
                                0x270C, 0x200D, 0xD83C, 0xDFFC};
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(
      String(emoji_zwj_sequence, std::size(emoji_zwj_sequence)));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);
}

// A Value-Parameterized Test class to test OffsetForPosition() with
// |include_partial_glyphs| parameter.
class IncludePartialGlyphsTest
    : public HarfBuzzShaperTest,
      public ::testing::WithParamInterface<IncludePartialGlyphsOption> {};

INSTANTIATE_TEST_SUITE_P(
    HarfBuzzShaperTest,
    IncludePartialGlyphsTest,
    ::testing::Values(IncludePartialGlyphsOption::kOnlyFullGlyphs,
                      IncludePartialGlyphsOption::kIncludePartialGlyphs));

TEST_P(IncludePartialGlyphsTest,
       OffsetForPositionMatchesPositionForOffsetLatin) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  IncludePartialGlyphsOption partial = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(4u, result->OffsetForPosition(result->PositionForOffset(4), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(5u, result->OffsetForPosition(result->PositionForOffset(5), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(6u, result->OffsetForPosition(result->PositionForOffset(6), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(7u, result->OffsetForPosition(result->PositionForOffset(7), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(8u, result->OffsetForPosition(result->PositionForOffset(8), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(9u, result->OffsetForPosition(result->PositionForOffset(9), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(10u,
            result->OffsetForPosition(result->PositionForOffset(10), string,
                                      partial, BreakGlyphsOption(false)));
  EXPECT_EQ(11u,
            result->OffsetForPosition(result->PositionForOffset(11), string,
                                      partial, BreakGlyphsOption(false)));
  EXPECT_EQ(12u,
            result->OffsetForPosition(result->PositionForOffset(12), string,
                                      partial, BreakGlyphsOption(false)));
}

TEST_P(IncludePartialGlyphsTest,
       OffsetForPositionMatchesPositionForOffsetArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  String string(arabic_string, 3u);
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  IncludePartialGlyphsOption partial = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3), string,
                                          partial, BreakGlyphsOption(false)));
}

TEST_P(IncludePartialGlyphsTest,
       OffsetForPositionMatchesPositionForOffsetMixed) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  String string(mixed_string, 6u);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);

  IncludePartialGlyphsOption partial = GetParam();
  EXPECT_EQ(0u, result->OffsetForPosition(result->PositionForOffset(0), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(1u, result->OffsetForPosition(result->PositionForOffset(1), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(2u, result->OffsetForPosition(result->PositionForOffset(2), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(3u, result->OffsetForPosition(result->PositionForOffset(3), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(4u, result->OffsetForPosition(result->PositionForOffset(4), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(5u, result->OffsetForPosition(result->PositionForOffset(5), string,
                                          partial, BreakGlyphsOption(false)));
  EXPECT_EQ(6u, result->OffsetForPosition(result->PositionForOffset(6), string,
                                          partial, BreakGlyphsOption(false)));
}

TEST_F(HarfBuzzShaperTest, CachedOffsetPositionMappingForOffsetLatin) {
  String string = To16Bit("Hello World!", 12);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> sr = shaper.Shape(&font_, direction);
  sr->EnsurePositionData();

  EXPECT_EQ(0u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(0)));
  EXPECT_EQ(1u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(1)));
  EXPECT_EQ(2u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(2)));
  EXPECT_EQ(3u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(3)));
  EXPECT_EQ(4u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(4)));
  EXPECT_EQ(5u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(5)));
  EXPECT_EQ(6u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(6)));
  EXPECT_EQ(7u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(7)));
  EXPECT_EQ(8u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(8)));
  EXPECT_EQ(9u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(9)));
  EXPECT_EQ(10u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(10)));
  EXPECT_EQ(11u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(11)));
  EXPECT_EQ(12u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(12)));
}

TEST_F(HarfBuzzShaperTest, CachedOffsetPositionMappingArabic) {
  UChar arabic_string[] = {0x628, 0x64A, 0x629};
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(String(arabic_string, 3u));
  scoped_refptr<ShapeResult> sr = shaper.Shape(&font_, direction);
  sr->EnsurePositionData();

  EXPECT_EQ(0u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(0)));
  EXPECT_EQ(1u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(1)));
  EXPECT_EQ(2u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(2)));
  EXPECT_EQ(3u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(3)));
}

TEST_F(HarfBuzzShaperTest, CachedOffsetPositionMappingMixed) {
  UChar mixed_string[] = {0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62};
  HarfBuzzShaper shaper(String(mixed_string, 6u));
  scoped_refptr<ShapeResult> sr = shaper.Shape(&font_, TextDirection::kLtr);
  sr->EnsurePositionData();

  EXPECT_EQ(0u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(0)));
  EXPECT_EQ(1u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(1)));
  EXPECT_EQ(2u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(2)));
  EXPECT_EQ(3u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(3)));
  EXPECT_EQ(4u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(4)));
  EXPECT_EQ(5u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(5)));
  EXPECT_EQ(6u, sr->CachedOffsetForPosition(sr->CachedPositionForOffset(6)));
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetMultiGlyphClusterLtr) {
  // In this Hindi text, each code unit produces a glyph, and the first 3 glyphs
  // form a grapheme cluster, and the last 2 glyphs form another.
  String string(u"\u0930\u093F\u0902\u0926\u0940");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> sr = shaper.Shape(&font_, direction);
  sr->EnsurePositionData();

  // The first 3 code units should be at position 0.
  EXPECT_EQ(0, sr->CachedPositionForOffset(0));
  EXPECT_EQ(0, sr->CachedPositionForOffset(1));
  EXPECT_EQ(0, sr->CachedPositionForOffset(2));
  // The last 2 code units should be > 0, and the same position.
  EXPECT_GT(sr->CachedPositionForOffset(3), 0);
  EXPECT_EQ(sr->CachedPositionForOffset(3), sr->CachedPositionForOffset(4));
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetMultiGlyphClusterRtl) {
  // In this Hindi text, each code unit produces a glyph, and the first 3 glyphs
  // form a grapheme cluster, and the last 2 glyphs form another.
  String string(u"\u0930\u093F\u0902\u0926\u0940");
  TextDirection direction = TextDirection::kRtl;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> sr = shaper.Shape(&font_, direction);
  sr->EnsurePositionData();

  // The first 3 code units should be at position 0, but since this is RTL, the
  // position is the right edgef of the character, and thus > 0.
  float pos0 = sr->CachedPositionForOffset(0);
  EXPECT_GT(pos0, 0);
  EXPECT_EQ(pos0, sr->CachedPositionForOffset(1));
  EXPECT_EQ(pos0, sr->CachedPositionForOffset(2));
  // The last 2 code units should be > 0, and the same position.
  float pos3 = sr->CachedPositionForOffset(3);
  EXPECT_GT(pos3, 0);
  EXPECT_LT(pos3, pos0);
  EXPECT_EQ(pos3, sr->CachedPositionForOffset(4));
}

TEST_F(HarfBuzzShaperTest, PositionForOffsetMissingGlyph) {
  String string(u"\u0633\u0644\u0627\u0645");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kRtl);
  // Because the offset 1 and 2 should form a ligature, SubRange(2, 4) creates a
  // ShapeResult that does not have its first glyph.
  result = result->SubRange(2, 4);
  result->PositionForOffset(0);
  // Pass if |PositionForOffset| does not crash.
}

static struct ShapeResultCopyRangeTestData {
  const char16_t* string;
  TextDirection direction;
  unsigned break_point;
} shape_result_copy_range_test_data[] = {
    {u"ABC", TextDirection::kLtr, 1},
    {u"\u0648\u0644\u064A", TextDirection::kRtl, 1},
    // These strings creates 3 runs. Split it in the middle of 2nd run.
    {u"\u65E5Hello\u65E5\u65E5", TextDirection::kLtr, 3},
    {u"\u0648\u0644\u064A AB \u0628\u062A", TextDirection::kRtl, 5}};

std::ostream& operator<<(std::ostream& ostream,
                         const ShapeResultCopyRangeTestData& data) {
  return ostream << String(data.string) << " @ " << data.break_point << ", "
                 << data.direction;
}

class ShapeResultCopyRangeTest
    : public HarfBuzzShaperTest,
      public testing::WithParamInterface<ShapeResultCopyRangeTestData> {};

INSTANTIATE_TEST_SUITE_P(HarfBuzzShaperTest,
                         ShapeResultCopyRangeTest,
                         testing::ValuesIn(shape_result_copy_range_test_data));

// Split a ShapeResult and combine them should match to the original result.
TEST_P(ShapeResultCopyRangeTest, Split) {
  const auto& test_data = GetParam();
  String string(test_data.string);
  TextDirection direction = test_data.direction;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Split the result.
  scoped_refptr<ShapeResult> result1 =
      ShapeResult::Create(&font_, 0, 0, direction);
  result->CopyRange(0, test_data.break_point, result1.get());
  EXPECT_EQ(test_data.break_point, result1->NumCharacters());
  EXPECT_EQ(0u, result1->StartIndex());
  EXPECT_EQ(test_data.break_point, result1->EndIndex());

  scoped_refptr<ShapeResult> result2 =
      ShapeResult::Create(&font_, 0, 0, direction);
  result->CopyRange(test_data.break_point, string.length(), result2.get());
  EXPECT_EQ(string.length() - test_data.break_point, result2->NumCharacters());
  EXPECT_EQ(test_data.break_point, result2->StartIndex());
  EXPECT_EQ(string.length(), result2->EndIndex());

  // Combine them.
  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, test_data.break_point, composite_result.get());
  result2->CopyRange(0, string.length(), composite_result.get());
  EXPECT_EQ(string.length(), composite_result->NumCharacters());

  // Test character indexes match.
  Vector<unsigned> expected_character_indexes =
      TestInfo(result)->CharacterIndexesForTesting();
  Vector<unsigned> composite_character_indexes =
      TestInfo(result)->CharacterIndexesForTesting();
  EXPECT_EQ(expected_character_indexes, composite_character_indexes);
}

// Shape ranges and combine them shold match to the result of shaping the whole
// string.
TEST_P(ShapeResultCopyRangeTest, ShapeRange) {
  const auto& test_data = GetParam();
  String string(test_data.string);
  TextDirection direction = test_data.direction;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Shape each range.
  scoped_refptr<ShapeResult> result1 =
      shaper.Shape(&font_, direction, 0, test_data.break_point);
  EXPECT_EQ(test_data.break_point, result1->NumCharacters());
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, direction, test_data.break_point, string.length());
  EXPECT_EQ(string.length() - test_data.break_point, result2->NumCharacters());

  // Combine them.
  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, test_data.break_point, composite_result.get());
  result2->CopyRange(0, string.length(), composite_result.get());
  EXPECT_EQ(string.length(), composite_result->NumCharacters());

  // Test character indexes match.
  Vector<unsigned> expected_character_indexes =
      TestInfo(result)->CharacterIndexesForTesting();
  Vector<unsigned> composite_character_indexes =
      TestInfo(result)->CharacterIndexesForTesting();
  EXPECT_EQ(expected_character_indexes, composite_character_indexes);
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeIntoLatin) {
  String string = To16Bit("Testing ShapeResult::createSubRun", 33);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result->CopyRange(0, 10, composite_result.get());
  result->CopyRange(10, 20, composite_result.get());
  result->CopyRange(20, 30, composite_result.get());
  result->CopyRange(30, 33, composite_result.get());

  EXPECT_EQ(result->NumCharacters(), composite_result->NumCharacters());
  EXPECT_EQ(result->SnappedWidth(), composite_result->SnappedWidth());

  // Rounding of width may be off by ~0.1 on Mac.
  float tolerance = 0.1f;
  EXPECT_NEAR(result->Width(), composite_result->Width(), tolerance);

  EXPECT_EQ(result->SnappedStartPositionForOffset(0),
            composite_result->SnappedStartPositionForOffset(0));
  EXPECT_EQ(result->SnappedStartPositionForOffset(15),
            composite_result->SnappedStartPositionForOffset(15));
  EXPECT_EQ(result->SnappedStartPositionForOffset(30),
            composite_result->SnappedStartPositionForOffset(30));
  EXPECT_EQ(result->SnappedStartPositionForOffset(33),
            composite_result->SnappedStartPositionForOffset(33));
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeIntoArabicThaiHanLatin) {
  UChar mixed_string[] = {0x628, 0x20, 0x64A, 0x629, 0x20, 0xE20, 0x65E5, 0x62};
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(String(mixed_string, 8u));
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result->CopyRange(0, 4, composite_result.get());
  result->CopyRange(4, 6, composite_result.get());
  result->CopyRange(6, 8, composite_result.get());

  EXPECT_EQ(result->NumCharacters(), composite_result->NumCharacters());
  EXPECT_EQ(result->SnappedWidth(), composite_result->SnappedWidth());
  EXPECT_EQ(result->SnappedStartPositionForOffset(0),
            composite_result->SnappedStartPositionForOffset(0));
  EXPECT_EQ(result->SnappedStartPositionForOffset(1),
            composite_result->SnappedStartPositionForOffset(1));
  EXPECT_EQ(result->SnappedStartPositionForOffset(2),
            composite_result->SnappedStartPositionForOffset(2));
  EXPECT_EQ(result->SnappedStartPositionForOffset(3),
            composite_result->SnappedStartPositionForOffset(3));
  EXPECT_EQ(result->SnappedStartPositionForOffset(4),
            composite_result->SnappedStartPositionForOffset(4));
  EXPECT_EQ(result->SnappedStartPositionForOffset(5),
            composite_result->SnappedStartPositionForOffset(5));
  EXPECT_EQ(result->SnappedStartPositionForOffset(6),
            composite_result->SnappedStartPositionForOffset(6));
  EXPECT_EQ(result->SnappedStartPositionForOffset(7),
            composite_result->SnappedStartPositionForOffset(7));
  EXPECT_EQ(result->SnappedStartPositionForOffset(8),
            composite_result->SnappedStartPositionForOffset(8));
}

TEST_P(ShapeParameterTest, ShapeResultCopyRangeAcrossRuns) {
  // Create 3 runs:
  // [0]: 1 character.
  // [1]: 5 characters.
  // [2]: 2 character.
  String mixed_string(u"\u65E5Hello\u65E5\u65E5");
  TextDirection direction = GetParam();
  HarfBuzzShaper shaper(mixed_string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // CopyRange(5, 7) should copy 1 character from [1] and 1 from [2].
  scoped_refptr<ShapeResult> target =
      ShapeResult::Create(&font_, 0, 0, direction);
  result->CopyRange(5, 7, target.get());
  EXPECT_EQ(2u, target->NumCharacters());
}

TEST_P(ShapeParameterTest, ShapeResultCopyRangeContextMultiRuns) {
  // Create 2 runs:
  // [0]: 5 characters.
  // [1]: 4 character.
  String mixed_string(u"Hello\u65E5\u65E5\u65E5\u65E5");
  TextDirection direction = GetParam();
  HarfBuzzShaper shaper(mixed_string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  scoped_refptr<ShapeResult> sub2to4 = result->SubRange(2, 4);
  EXPECT_EQ(2u, sub2to4->NumCharacters());
  scoped_refptr<ShapeResult> sub5to9 = result->SubRange(5, 9);
  EXPECT_EQ(4u, sub5to9->NumCharacters());
}

TEST_F(HarfBuzzShaperTest, ShapeResultCopyRangeSegmentGlyphBoundingBox) {
  String string(u"THello worldL");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result1 = shaper.Shape(&font_, direction, 0, 6);
  scoped_refptr<ShapeResult> result2 =
      shaper.Shape(&font_, direction, 6, string.length());

  scoped_refptr<ShapeResult> composite_result =
      ShapeResult::Create(&font_, 0, 0, direction);
  result1->CopyRange(0, 6, composite_result.get());
  result2->CopyRange(6, string.length(), composite_result.get());

  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);
  EXPECT_EQ(result->Width(), composite_result->Width());
}

TEST_F(HarfBuzzShaperTest, SubRange) {
  String string(u"Hello world");
  TextDirection direction = TextDirection::kRtl;
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  scoped_refptr<ShapeResult> sub_range = result->SubRange(4, 7);
  DCHECK_EQ(4u, sub_range->StartIndex());
  DCHECK_EQ(7u, sub_range->EndIndex());
  DCHECK_EQ(3u, sub_range->NumCharacters());
  DCHECK_EQ(result->Direction(), sub_range->Direction());
}

TEST_F(HarfBuzzShaperTest, SafeToBreakLatinCommonLigatures) {
  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;

  // MEgalopolis Extra has a lot of ligatures which this test relies on.
  Font testFont = blink::test::CreateTestFont(
      AtomicString("MEgalopolis"),
      blink::test::PlatformTestDataPath(
          "third_party/MEgalopolis/MEgalopolisExtra.woff"),
      16, &ligatures);

  String string = To16Bit("ffi ff", 6);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result =
      shaper.Shape(&testFont, TextDirection::kLtr);

  EXPECT_EQ(0u, result->NextSafeToBreakOffset(0));  // At start of string.
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(1));  // At end of "ffi" ligature.
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(2));  // At end of "ffi" ligature.
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(3));  // At end of "ffi" ligature.
  EXPECT_EQ(4u, result->NextSafeToBreakOffset(4));  // After space.
  EXPECT_EQ(6u, result->NextSafeToBreakOffset(5));  // At end of "ff" ligature.
  EXPECT_EQ(6u, result->NextSafeToBreakOffset(6));  // At end of "ff" ligature.

  // Verify safe to break information in copied results to ensure that both
  // copying and multi-run break information works.
  scoped_refptr<ShapeResult> copied_result =
      ShapeResult::Create(&testFont, 0, 0, TextDirection::kLtr);
  result->CopyRange(0, 3, copied_result.get());
  result->CopyRange(3, string.length(), copied_result.get());

  EXPECT_EQ(0u, copied_result->NextSafeToBreakOffset(0));
  EXPECT_EQ(3u, copied_result->NextSafeToBreakOffset(1));
  EXPECT_EQ(3u, copied_result->NextSafeToBreakOffset(2));
  EXPECT_EQ(3u, copied_result->NextSafeToBreakOffset(3));
  EXPECT_EQ(4u, copied_result->NextSafeToBreakOffset(4));
  EXPECT_EQ(6u, copied_result->NextSafeToBreakOffset(5));
  EXPECT_EQ(6u, copied_result->NextSafeToBreakOffset(6));
}

TEST_F(HarfBuzzShaperTest, SafeToBreakPreviousLatinCommonLigatures) {
  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;

  // MEgalopolis Extra has a lot of ligatures which this test relies on.
  Font testFont = blink::test::CreateTestFont(
      AtomicString("MEgalopolis"),
      blink::test::PlatformTestDataPath(
          "third_party/MEgalopolis/MEgalopolisExtra.woff"),
      16, &ligatures);

  String string = To16Bit("ffi ff", 6);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result =
      shaper.Shape(&testFont, TextDirection::kLtr);

  EXPECT_EQ(6u, result->PreviousSafeToBreakOffset(6));  // At end of "ff" liga.
  EXPECT_EQ(4u, result->PreviousSafeToBreakOffset(5));  // At end of "ff" liga.
  EXPECT_EQ(4u, result->PreviousSafeToBreakOffset(4));  // After space.
  EXPECT_EQ(3u, result->PreviousSafeToBreakOffset(3));  // At end of "ffi" liga.
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(2));  // At start of string.
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(1));  // At start of string.
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(0));  // At start of string.

  // Verify safe to break information in copied results to ensure that both
  // copying and multi-run break information works.
  scoped_refptr<ShapeResult> copied_result =
      ShapeResult::Create(&testFont, 0, 0, TextDirection::kLtr);
  result->CopyRange(0, 3, copied_result.get());
  result->CopyRange(3, string.length(), copied_result.get());

  EXPECT_EQ(6u, copied_result->PreviousSafeToBreakOffset(6));
  EXPECT_EQ(4u, copied_result->PreviousSafeToBreakOffset(5));
  EXPECT_EQ(4u, copied_result->PreviousSafeToBreakOffset(4));
  EXPECT_EQ(3u, copied_result->PreviousSafeToBreakOffset(3));
  EXPECT_EQ(0u, copied_result->PreviousSafeToBreakOffset(2));
  EXPECT_EQ(0u, copied_result->PreviousSafeToBreakOffset(1));
  EXPECT_EQ(0u, copied_result->PreviousSafeToBreakOffset(0));
}

TEST_F(HarfBuzzShaperTest, SafeToBreakLatinDiscretionaryLigatures) {
  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;
  ligatures.discretionary = FontDescription::kEnabledLigaturesState;

  // MEgalopolis Extra has a lot of ligatures which this test relies on.
  Font testFont = blink::test::CreateTestFont(
      AtomicString("MEgalopolis"),
      blink::test::PlatformTestDataPath(
          "third_party/MEgalopolis/MEgalopolisExtra.woff"),
      16, &ligatures);

  // $ ./hb-shape   --shaper=ot --features="dlig=1,kern" --show-flags
  // MEgalopolisExtra.ttf  "RADDAYoVaDD"
  // [R_A=0+1150|D=2+729|D=3+699|A=4+608#1|Y=5+608#1|o=6+696#1|V=7+652#1|a=8+657#1|D=9+729|D=10+729]
  // RA Ligature, unkerned D D, D A kerns, A Y kerns, Y o kerns, o V kerns, V a
  // kerns, no kerning with D.
  String test_word(u"RADDAYoVaDD");
  unsigned safe_to_break_positions[] = {2, 3, 9, 10};
  HarfBuzzShaper shaper(test_word);
  scoped_refptr<ShapeResult> result =
      shaper.Shape(&testFont, TextDirection::kLtr);

  unsigned compare_safe_to_break_position = 0;
  for (unsigned i = 1; i < test_word.length() - 1; ++i) {
    EXPECT_EQ(safe_to_break_positions[compare_safe_to_break_position],
              result->NextSafeToBreakOffset(i));
    if (i == safe_to_break_positions[compare_safe_to_break_position])
      compare_safe_to_break_position++;
  }

  // Add zero-width spaces at some of the safe to break offsets.
  String inserted_zero_width_spaces(u"RA\u200BD\u200BDAYoVa\u200BD\u200BD");
  HarfBuzzShaper refShaper(inserted_zero_width_spaces);
  scoped_refptr<ShapeResult> referenceResult =
      refShaper.Shape(&testFont, TextDirection::kLtr);

  // Results should be identical if it truly is safe to break at the designated
  // safe-to-break offsets because otherwise, the zero-width spaces would have
  // altered the text spacing, for example by breaking apart ligatures or
  // kerning pairs.
  EXPECT_EQ(result->SnappedWidth(), referenceResult->SnappedWidth());

  // Zero-width spaces were inserted, so we need to account for that by
  // offseting the index that we compare against.
  unsigned inserts_offset = 0;
  for (unsigned i = 0; i < test_word.length(); ++i) {
    if (i == safe_to_break_positions[inserts_offset])
      inserts_offset++;
    EXPECT_EQ(
        result->SnappedStartPositionForOffset(i),
        referenceResult->SnappedStartPositionForOffset(i + inserts_offset));
  }
}

// TODO(crbug.com/870712): This test fails due to font fallback differences on
// Android and Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SafeToBreakArabicCommonLigatures \
  DISABLED_SafeToBreakArabicCommonLigatures
#else
#define MAYBE_SafeToBreakArabicCommonLigatures SafeToBreakArabicCommonLigatures
#endif
TEST_F(HarfBuzzShaperTest, MAYBE_SafeToBreakArabicCommonLigatures) {
  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;

  // كسر الاختبار
  String string(
      u"\u0643\u0633\u0631\u0020\u0627\u0644\u0627\u062E\u062A\u0628\u0627"
      u"\u0631");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kRtl);

  EXPECT_EQ(0u, result->NextSafeToBreakOffset(0));
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(1));
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(2));
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(3));
  EXPECT_EQ(4u, result->NextSafeToBreakOffset(4));
#if BUILDFLAG(IS_APPLE)
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(5));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(6));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(7));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(8));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(9));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(10));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(11));
#else
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(5));
  EXPECT_EQ(7u, result->NextSafeToBreakOffset(6));
  EXPECT_EQ(7u, result->NextSafeToBreakOffset(7));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(8));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(9));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(10));
  EXPECT_EQ(11u, result->NextSafeToBreakOffset(11));
#endif
  EXPECT_EQ(12u, result->NextSafeToBreakOffset(12));

  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(0));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(1));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(2));
  EXPECT_EQ(3u, result->PreviousSafeToBreakOffset(3));
  EXPECT_EQ(4u, result->PreviousSafeToBreakOffset(4));
#if BUILDFLAG(IS_APPLE)
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(5));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(6));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(7));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(8));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(9));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(10));
  EXPECT_EQ(11u, result->PreviousSafeToBreakOffset(11));
#else
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(5));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(6));
  EXPECT_EQ(7u, result->PreviousSafeToBreakOffset(7));
  EXPECT_EQ(7u, result->PreviousSafeToBreakOffset(8));
  EXPECT_EQ(7u, result->PreviousSafeToBreakOffset(9));
  EXPECT_EQ(7u, result->PreviousSafeToBreakOffset(10));
  EXPECT_EQ(11u, result->PreviousSafeToBreakOffset(11));
#endif
  EXPECT_EQ(12u, result->PreviousSafeToBreakOffset(12));
}

// http://crbug.com/1170334
TEST_F(HarfBuzzShaperTest, SafeToBreakU0635) {
  FontDescription::VariantLigatures ligatures;
  ligatures.common = FontDescription::kEnabledLigaturesState;

  // Five U+0635. This sequence should be rendered once.
  String string(u"\u0635\u0635\u0635\u0635\u0635");
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kRtl);

  EXPECT_EQ(0u, result->NextSafeToBreakOffset(0));
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(1));
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(2));
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(3));
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(4));
  EXPECT_EQ(5u, result->NextSafeToBreakOffset(5));

  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(0));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(1));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(2));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(3));
  EXPECT_EQ(0u, result->PreviousSafeToBreakOffset(4));
  EXPECT_EQ(5u, result->PreviousSafeToBreakOffset(5));
}

// TODO(layout-dev): Expand RTL test coverage and add tests for mixed
// directionality strings.

// Test when some characters are missing in |runs_|.
TEST_P(ShapeParameterTest, SafeToBreakMissingRun) {
  TextDirection direction = GetParam();
  scoped_refptr<ShapeResult> result = CreateMissingRunResult(direction);
#if DCHECK_IS_ON()
  result->CheckConsistency();
#endif

  EXPECT_EQ(2u, result->StartIndex());
  EXPECT_EQ(10u, result->EndIndex());

  EXPECT_EQ(2u, result->NextSafeToBreakOffset(2));
  EXPECT_EQ(3u, result->NextSafeToBreakOffset(3));
  EXPECT_EQ(4u, result->NextSafeToBreakOffset(4));
  EXPECT_EQ(6u, result->NextSafeToBreakOffset(5));
  EXPECT_EQ(6u, result->NextSafeToBreakOffset(6));
  EXPECT_EQ(8u, result->NextSafeToBreakOffset(7));
  EXPECT_EQ(8u, result->NextSafeToBreakOffset(8));
  EXPECT_EQ(10u, result->NextSafeToBreakOffset(9));

  EXPECT_EQ(2u, result->PreviousSafeToBreakOffset(2));
  EXPECT_EQ(3u, result->PreviousSafeToBreakOffset(3));
  EXPECT_EQ(4u, result->PreviousSafeToBreakOffset(4));
  EXPECT_EQ(4u, result->PreviousSafeToBreakOffset(5));
  EXPECT_EQ(6u, result->PreviousSafeToBreakOffset(6));
  EXPECT_EQ(6u, result->PreviousSafeToBreakOffset(7));
  EXPECT_EQ(8u, result->PreviousSafeToBreakOffset(8));
  EXPECT_EQ(8u, result->PreviousSafeToBreakOffset(9));
}

TEST_P(ShapeParameterTest, CopyRangeMissingRun) {
  TextDirection direction = GetParam();
  scoped_refptr<ShapeResult> result = CreateMissingRunResult(direction);

  // 6 and 7 are missing but NumCharacters() should be 4.
  scoped_refptr<ShapeResult> sub = result->SubRange(5, 9);
  EXPECT_EQ(sub->StartIndex(), 5u);
  EXPECT_EQ(sub->EndIndex(), 9u);
  EXPECT_EQ(sub->NumCharacters(), 4u);

  // The end is missing.
  sub = result->SubRange(5, 7);
  EXPECT_EQ(sub->StartIndex(), 5u);
  EXPECT_EQ(sub->EndIndex(), 7u);
  EXPECT_EQ(sub->NumCharacters(), 2u);

  // The start is missing.
  sub = result->SubRange(7, 9);
  EXPECT_EQ(sub->StartIndex(), 7u);
  EXPECT_EQ(sub->EndIndex(), 9u);
  EXPECT_EQ(sub->NumCharacters(), 2u);
}

TEST_P(ShapeParameterTest, CopyRangeNoRuns) {
  TextDirection direction = GetParam();
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(&font_, 0, 2, direction);

  scoped_refptr<ShapeResult> sub0 = result->SubRange(0, 1);
  EXPECT_EQ(sub0->StartIndex(), 0u);
  EXPECT_EQ(sub0->EndIndex(), 1u);
  EXPECT_EQ(sub0->NumCharacters(), 1u);

  scoped_refptr<ShapeResult> sub1 = result->SubRange(1, 2);
  EXPECT_EQ(sub1->StartIndex(), 1u);
  EXPECT_EQ(sub1->EndIndex(), 2u);
  EXPECT_EQ(sub1->NumCharacters(), 1u);

  Vector<scoped_refptr<ShapeResult>> range_results;
  Vector<ShapeResult::ShapeRange> ranges;
  range_results.push_back(ShapeResult::CreateEmpty(*result));
  ranges.push_back(ShapeResult::ShapeRange{0, 1, range_results[0].get()});
  result->CopyRanges(ranges.data(), ranges.size());
  for (unsigned i = 0; i < ranges.size(); i++) {
    const ShapeResult::ShapeRange& range = ranges[i];
    const ShapeResult& range_result = *range_results[i];
    EXPECT_EQ(range_result.StartIndex(), range.start);
    EXPECT_EQ(range_result.EndIndex(), range.end);
    EXPECT_EQ(range_result.NumCharacters(), range.end - range.start);
  }
}

TEST_P(ShapeParameterTest, ShapeResultViewMissingRun) {
  TextDirection direction = GetParam();
  scoped_refptr<ShapeResult> result = CreateMissingRunResult(direction);

  // 6 and 7 are missing but NumCharacters() should be 4.
  scoped_refptr<ShapeResultView> view =
      ShapeResultView::Create(result.get(), 5, 9);
  EXPECT_EQ(view->StartIndex(), 5u);
  EXPECT_EQ(view->EndIndex(), 9u);
  EXPECT_EQ(view->NumCharacters(), 4u);

  // The end is missing.
  view = ShapeResultView::Create(result.get(), 5, 7);
  EXPECT_EQ(view->StartIndex(), 5u);
  EXPECT_EQ(view->EndIndex(), 7u);
  EXPECT_EQ(view->NumCharacters(), 2u);

  // The start is missing.
  view = ShapeResultView::Create(result.get(), 7, 9);
  EXPECT_EQ(view->StartIndex(), 7u);
  EXPECT_EQ(view->EndIndex(), 9u);
  EXPECT_EQ(view->NumCharacters(), 2u);
}

// Call this to ensure your test string has some kerning going on.
static bool KerningIsHappening(const FontDescription& font_description,
                               TextDirection direction,
                               const String& str) {
  FontDescription no_kern = font_description;
  no_kern.SetKerning(FontDescription::kNoneKerning);

  FontDescription kern = font_description;
  kern.SetKerning(FontDescription::kAutoKerning);

  Font font_no_kern(no_kern);
  Font font_kern(kern);

  HarfBuzzShaper shaper(str);

  scoped_refptr<ShapeResult> result_no_kern =
      shaper.Shape(&font_no_kern, direction);
  scoped_refptr<ShapeResult> result_kern = shaper.Shape(&font_kern, direction);

  for (unsigned i = 0; i < str.length(); i++) {
    if (result_no_kern->PositionForOffset(i) !=
        result_kern->PositionForOffset(i))
      return true;
  }
  return false;
}

TEST_F(HarfBuzzShaperTest, KerningIsHappeningWorks) {
  EXPECT_TRUE(
      KerningIsHappening(font_description_, TextDirection::kLtr, u"AVOID"));
  EXPECT_FALSE(
      KerningIsHappening(font_description_, TextDirection::kLtr, u"NOID"));

  // We won't kern vertically with the default font.
  font_description_.SetOrientation(FontOrientation::kVerticalUpright);

  EXPECT_FALSE(
      KerningIsHappening(font_description_, TextDirection::kLtr, u"AVOID"));
  EXPECT_FALSE(
      KerningIsHappening(font_description_, TextDirection::kLtr, u"NOID"));
}

TEST_F(HarfBuzzShaperTest,
       ShapeHorizontalWithoutSubpixelPositionWithoutKerningIsRounded) {
  ScopedSubpixelOverride subpixel_override(false);

  String string(u"NOID");
  TextDirection direction = TextDirection::kLtr;
  ASSERT_FALSE(KerningIsHappening(font_description_, direction, string));

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    EXPECT_EQ(round(position), position)
        << "Position not rounded at offset " << i;
  }
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShapeHorizontalWithSubpixelPositionWithoutKerningIsNotRounded \
  DISABLED_ShapeHorizontalWithSubpixelPositionWithoutKerningIsNotRounded
#else
#define MAYBE_ShapeHorizontalWithSubpixelPositionWithoutKerningIsNotRounded \
  ShapeHorizontalWithSubpixelPositionWithoutKerningIsNotRounded
#endif
TEST_F(HarfBuzzShaperTest,
       MAYBE_ShapeHorizontalWithSubpixelPositionWithoutKerningIsNotRounded) {
  ScopedSubpixelOverride subpixel_override(true);

  String string(u"NOID");
  TextDirection direction = TextDirection::kLtr;
  ASSERT_FALSE(KerningIsHappening(font_description_, direction, string));

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    if (round(position) != position)
      return;
  }

  EXPECT_TRUE(false) << "No unrounded positions found";
}

TEST_F(HarfBuzzShaperTest,
       ShapeHorizontalWithoutSubpixelPositionWithKerningIsRounded) {
  ScopedSubpixelOverride subpixel_override(false);

  // The verification whether a particular shaping routine is performing
  // kerning can be flaky when subpixel if OFF - see KerningIsHappening().
  // For instance, if the position of a character is say `7.55` with subpixel
  // ON, it gets rounded to `8` with subpixel position OFF, and the comparison
  // in KerningIsHappening() fails, although kerning is effectively happening.
  //
  // Hence, this test leverages the uses of a particular font (Arial) where the
  // result is reliable cross platform (linux, mac, ios, etc).
  //
  // [1] RoundHarfBuzzPosition() @harfbuzz_shaper.cc
  FontDescription font_description(font_description_);
  FontFamily family;
  family.SetFamily(font_family_names::kArial, FontFamily::Type::kFamilyName);
  font_description.SetFamily(family);
  Font font = Font(font_description);

  String string(u"AVOID");
  TextDirection direction = TextDirection::kLtr;
  ASSERT_TRUE(KerningIsHappening(font_description, direction, string));

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font, direction);

  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    EXPECT_EQ(round(position), position)
        << "Position not rounded at offset " << i;
  }
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ShapeHorizontalWithSubpixelPositionWithKerningIsNotRounded \
  DISABLED_ShapeHorizontalWithSubpixelPositionWithKerningIsNotRounded
#else
#define MAYBE_ShapeHorizontalWithSubpixelPositionWithKerningIsNotRounded \
  ShapeHorizontalWithSubpixelPositionWithKerningIsNotRounded
#endif
TEST_F(HarfBuzzShaperTest,
       MAYBE_ShapeHorizontalWithSubpixelPositionWithKerningIsNotRounded) {
  ScopedSubpixelOverride subpixel_override(true);

  String string(u"AVOID");
  TextDirection direction = TextDirection::kLtr;
  ASSERT_TRUE(KerningIsHappening(font_description_, direction, string));

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    if (round(position) != position)
      return;
  }

  EXPECT_TRUE(false) << "No unrounded positions found";
}

TEST_F(HarfBuzzShaperTest, ShapeVerticalWithoutSubpixelPositionIsRounded) {
  ScopedSubpixelOverride subpixel_override(false);

  font_description_.SetOrientation(FontOrientation::kVerticalUpright);
  font_ = Font(font_description_);

  String string(u"\u65E5\u65E5\u65E5");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    EXPECT_EQ(round(position), position)
        << "Position not rounded at offset " << i;
  }
}

TEST_F(HarfBuzzShaperTest, ShapeVerticalWithSubpixelPositionIsRounded) {
  ScopedSubpixelOverride subpixel_override(true);

  font_description_.SetOrientation(FontOrientation::kVerticalUpright);
  font_ = Font(font_description_);

  String string(u"\u65E5\u65E5\u65E5");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, direction);

  // Vertical text is never subpixel positioned.
  for (unsigned i = 0; i < string.length(); i++) {
    float position = result->PositionForOffset(i);
    EXPECT_EQ(round(position), position)
        << "Position not rounded at offset " << i;
  }
}

// Broken on iOS: https://crbug.com/1194323
#if BUILDFLAG(IS_IOS)
#define MAYBE_EmojiPercentage DISABLED_EmojiPercentage
#else
#define MAYBE_EmojiPercentage EmojiPercentage
#endif
TEST_F(HarfBuzzShaperTest, MAYBE_EmojiPercentage) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::IsAtLeastOS11())
    GTEST_SKIP() << "Broken on macOS >= 11: https://crbug.com/1194323";
#endif
#if BUILDFLAG(IS_WIN)
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN11) {
    GTEST_SKIP() << "Broken on WIN11 and greater: https://crbug.com/1286133";
  }
#endif
  // This test relies on Noto Color Emoji from the third_party directory to not
  // contain sequences and single codepoint emoji from Unicode 13 and 13.1 such
  // as:
  // * Couple with Heart: Woman, Man, Medium-Light Skin Tone, Medium-Dark Skin
  // Tone
  // * Disguised Face U+1F978
  // * Anatomical Heart U+1FAC0
  String string(
      u"aa👩🏼‍❤️‍👨🏾😶👩🏼‍❤️‍👨🏾aa👩🏼‍❤️‍👨🏾😶"
      u"👩🏼‍❤️‍👨🏾aa🫀🫀🥸🥸😶😶");

  struct Expectation {
    unsigned expected_clusters;
    unsigned expected_broken_clusters;
  };

  Expectation expectations[] = {{3, 2}, {3, 2}, {6, 4}};
#if BUILDFLAG(IS_ANDROID)
  // On Android 11, SDK level 30, fallback occurs to an emoji
  // font that has coverage for the last segment. Adjust the expectation.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SdkVersion::SDK_VERSION_R) {
    expectations[2].expected_broken_clusters = 0;
  }
#endif
  unsigned num_calls = 0;
  HarfBuzzShaper::EmojiMetricsCallback metrics_callback =
      base::BindLambdaForTesting(
          [&](unsigned num_clusters, unsigned num_broken_clusters) {
            CHECK_EQ(num_clusters, expectations[num_calls].expected_clusters);
            CHECK_EQ(num_broken_clusters,
                     expectations[num_calls].expected_broken_clusters);

            num_calls++;
          });
  HarfBuzzShaper shaper(string, metrics_callback);
  Font emoji_font = CreateNotoColorEmoji();
  scoped_refptr<ShapeResult> result =
      shaper.Shape(&emoji_font, TextDirection::kLtr);
  CHECK_EQ(num_calls, std::size(expectations));
}

// https://crbug.com/1255482
TEST_F(HarfBuzzShaperTest, OverlyLongGraphemeCluster) {
  // Letter 'e' with 35000 diacritics, followed by letter 'X'
  StringBuilder builder;
  builder.Append('e');
  for (unsigned i = 0; i < 35000; ++i)
    builder.Append(kCombiningAcuteAccentCharacter);
  builder.Append('X');
  String string = builder.ToString();

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);

  ASSERT_EQ(2u, runs.size());

  // The first run contains a glyph 'é' with 32767 diacritic glyphs, reaching
  // the maximum allowed number of glyphs per run. The remaining 2232
  // diacritics are abandoned.
  EXPECT_EQ(0u, runs[0].start_index);
  EXPECT_EQ(35001u, runs[0].num_characters);
  EXPECT_EQ(32768u, runs[0].num_glyphs);

  // The second run consists of a single glyph 'X'.
  EXPECT_EQ(35001u, runs[1].start_index);
  EXPECT_EQ(1u, runs[1].num_characters);
  EXPECT_EQ(1u, runs[1].num_glyphs);
}

// HarfBuzz should not swap the ordering for some fonts.
//
// In general, for cluster levels 0 and 1, if clusters are not in ascending
// order (either LTR or RTL based on buffer direction), then it is a bug that
// needs to be fixed.
// https://github.com/harfbuzz/harfbuzz/issues/3553 crbug.com/1319078
TEST_F(HarfBuzzShaperTest, UnorderedClusterIndex) {
  // The first two characters may be swapped, producing [1, 0].
  // U+1DDE COMBINING LATIN LETTER SMALL CAPITAL L
  // U+A74A LATIN CAPITAL LETTER O WITH LONG STROKE OVERLAY
  String string(u"\u1DDE\uA74A");

  // The symptom was found on Mac, but it may occur on other platforms.
  // Setting the font family is not strictly necessary as fonts automatically
  // fallback, but it helps keeping the whole string in a run (i.e., shapes
  // surrounding characters with the same font.)
  FontFamily family;
  family.SetFamily(AtomicString("Geneva"), FontFamily::Type::kFamilyName);
  font_description_.SetFamily(family);
  font_ = Font(font_description_);

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font_, TextDirection::kLtr);
#if DCHECK_IS_ON()
  result->CheckConsistency();
#endif
  Vector<ShapeResultRunData> runs = ShapeResultRunData::Get(result);
  EXPECT_GE(runs.size(), 1u);
}

}  // namespace blink
