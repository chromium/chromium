// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

using base::apple::CFToNSOwnershipCast;
using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;
using base::apple::ObjCCast;
using base::apple::ScopedCFTypeRef;

namespace blink {

namespace {
struct FontName {
  const char* full_font_name;
  const char* postscript_name;
  const char* family_name;
};

// If these font names are unavailable on future Mac OS versions, please try to
// find replacements or remove individual lines.
const FontName FontNames[] = {
    {"American Typewriter Condensed Light", "AmericanTypewriter-CondensedLight",
     "American Typewriter"},
    {"Apple Braille Outline 6 Dot", "AppleBraille-Outline6Dot",
     "Apple Braille"},
    {"Arial Narrow Bold Italic", "ArialNarrow-BoldItalic", "Arial Narrow"},
    {"Baskerville SemiBold Italic", "Baskerville-SemiBoldItalic",
     "Baskerville"},
    {"Devanagari MT", "DevanagariMT", "Devanagari MT"},
    {"DIN Alternate Bold", "DINAlternate-Bold", "DIN Alternate"},
    {"Gill Sans Light Italic", "GillSans-LightItalic", "Gill Sans"},
    {"Malayalam Sangam MN", "MalayalamSangamMN", "Malayalam Sangam MN"},
    {"Hiragino Maru Gothic ProN W4", "HiraMaruProN-W4",
     "Hiragino Maru Gothic ProN"},
    {"Hiragino Sans W3", "HiraginoSans-W3", "Hiragino Sans"},
};

const FontName CommonFontNames[] = {
    {"Avenir-Roman", "Avenir-Roman", "Avenir"},
    {"CourierNewPS-BoldMT", "CourierNewPS-BoldMT", "Courier New"},
    {"Helvetica-Light", "Helvetica-Light", "Helvetica"},
    {"HelveticaNeue-CondensedBlack", "HelveticaNeue-CondensedBlack",
     "Helvetica Neue"},
    {"Menlo-Bold", "Menlo-Bold", "Menlo"},
    {"Tahoma", "Tahoma", "Tahoma"},
    {"TimesNewRomanPS-BoldItalicMT", "TimesNewRomanPS-BoldItalicMT",
     "Times New Roman"},
};

const char* FamiliesWithBoldItalicFaces[] = {"Baskerville", "Cochin", "Georgia",
                                             "GillSans"};

void TestFontWithBoldAndItalicTraits(const AtomicString& font_name) {
  ScopedCFTypeRef<CTFontRef> font_italic = MatchFontFamily(
      font_name, kNormalWeightValue, kItalicSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font_italic);

  CTFontSymbolicTraits italic_font_traits =
      CTFontGetSymbolicTraits(font_italic.get());
  EXPECT_TRUE(italic_font_traits & kCTFontTraitItalic);

  ScopedCFTypeRef<CTFontRef> font_bold_italic = MatchFontFamily(
      font_name, kBoldWeightValue, kItalicSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font_bold_italic);

  CTFontSymbolicTraits bold_italic_font_traits =
      CTFontGetSymbolicTraits(font_bold_italic.get());
  EXPECT_TRUE(bold_italic_font_traits & kCTFontTraitItalic);
  EXPECT_TRUE(bold_italic_font_traits & kCTFontTraitBold);
}

void TestFontMatchingByFamilyName(const char* font_name) {
  ScopedCFTypeRef<CTFontRef> font =
      MatchFontFamily(AtomicString(font_name), kNormalWeightValue,
                      kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font);
  ScopedCFTypeRef<CFStringRef> matched_family_name(
      CTFontCopyFamilyName(font.get()));
  ScopedCFTypeRef<CFStringRef> expected_family_name(
      CFStringCreateWithCString(nullptr, font_name, kCFStringEncodingUTF8));
  EXPECT_EQ(
      CFStringCompare(matched_family_name.get(), expected_family_name.get(),
                      kCFCompareCaseInsensitive),
      kCFCompareEqualTo);
}

void TestFontMatchingByPostscriptName(const char* font_name) {
  ScopedCFTypeRef<CTFontRef> font =
      MatchFontFamily(AtomicString(font_name), kNormalWeightValue,
                      kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font);
  ScopedCFTypeRef<CFStringRef> matched_postscript_name(
      CTFontCopyPostScriptName(font.get()));
  ScopedCFTypeRef<CFStringRef> expected_postscript_name(
      CFStringCreateWithCString(nullptr, font_name, kCFStringEncodingUTF8));
  EXPECT_EQ(CFStringCompare(matched_postscript_name.get(),
                            expected_postscript_name.get(),
                            kCFCompareCaseInsensitive),
            kCFCompareEqualTo);
}

void TestCTAndNSMatchEqual(const char* font_name,
                           float size,
                           int weight,
                           int style,
                           int stretch) {
  ScopedCFTypeRef<CTFontRef> matched_font = MatchFontFamily(
      AtomicString(font_name), FontSelectionValue(weight),
      FontSelectionValue(style), FontSelectionValue(stretch), size);

  NSFontTraitMask traits = (style != kNormalSlopeValue) ? NSFontItalicTrait : 0;
  ScopedCFTypeRef<CTFontRef> matched_ns_font(
      base::apple::NSToCFOwnershipCast(MatchNSFontFamily(
          AtomicString(font_name), traits, FontSelectionValue(weight), size)));

  if (matched_font || matched_ns_font) {
    EXPECT_TRUE(matched_font);
    EXPECT_TRUE(matched_ns_font);

    ScopedCFTypeRef<CFStringRef> matched_font_name(
        CTFontCopyPostScriptName(matched_font.get()));
    EXPECT_TRUE(matched_font_name);

    ScopedCFTypeRef<CFStringRef> matched_ns_font_name(
        CTFontCopyPostScriptName(matched_ns_font.get()));
    EXPECT_TRUE(matched_ns_font_name);

    EXPECT_TRUE(
        CFStringCompare(matched_font_name.get(), matched_ns_font_name.get(),
                        kCFCompareCaseInsensitive) == kCFCompareEqualTo);
  }
}

}  // namespace

TEST(FontMatcherMacTest, MatchSystemFont) {
  ScopedCFTypeRef<CTFontRef> font = MatchSystemUIFont(
      kNormalWeightValue, kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font);
}

TEST(FontMatcherMacTest, MatchSystemFontItalic) {
  ScopedCFTypeRef<CTFontRef> font = MatchSystemUIFont(
      kNormalWeightValue, kItalicSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font);
  NSDictionary* traits = CFToNSOwnershipCast(CTFontCopyTraits(font.get()));
  NSNumber* slant_num =
      ObjCCast<NSNumber>(traits[CFToNSPtrCast(kCTFontSlantTrait)]);
  float slant = slant_num.floatValue;
  EXPECT_NE(slant, 0.0);
}

TEST(FontMatcherMacTest, MatchSystemFontWithWeightVariations) {
  // Mac SystemUI font supports weight variations between 1 and 1000.
  int min_weight = 1;
  int max_weight = 1000;
  FourCharCode wght_tag = 'wght';
  for (int weight = min_weight - 1; weight <= max_weight + 1; weight += 50) {
    if (weight != kNormalWeightValue) {
      ScopedCFTypeRef<CTFontRef> font = MatchSystemUIFont(
          FontSelectionValue(weight), kNormalSlopeValue, kNormalWidthValue, 11);
      EXPECT_TRUE(font);

      NSDictionary* variations =
          CFToNSOwnershipCast(CTFontCopyVariation(font.get()));
      NSNumber* actual_weight_num = ObjCCast<NSNumber>(variations[@(wght_tag)]);
      EXPECT_TRUE(actual_weight_num);

      float actual_weight = actual_weight_num.floatValue;
      float expected_weight =
          std::max(min_weight, std::min(max_weight, weight));
      EXPECT_EQ(actual_weight, expected_weight);
    }
  }
}

TEST(FontMatcherMacTest, MatchSystemFontWithWidthVariations) {
  // Mac SystemUI font supports width variations between 30 and 150.
  int min_width = 30;
  int max_width = 150;
  FourCharCode wdth_tag = 'wdth';
  for (int width = min_width - 10; width <= max_width + 10; width += 10) {
    if (width != kNormalWidthValue) {
      ScopedCFTypeRef<CTFontRef> font = MatchSystemUIFont(
          kNormalWidthValue, kNormalSlopeValue, FontSelectionValue(width), 11);
      EXPECT_TRUE(font);

      NSDictionary* variations =
          CFToNSOwnershipCast(CTFontCopyVariation(font.get()));
      NSNumber* actual_width_num = ObjCCast<NSNumber>(variations[@(wdth_tag)]);
      EXPECT_TRUE(actual_width_num);

      float actual_width = actual_width_num.floatValue;
      float expected_width = std::max(min_width, std::min(max_width, width));
      EXPECT_EQ(actual_width, expected_width);
    }
  }
}

TEST(FontMatcherMacTest, FontFamilyMatchingUnavailableFont) {
  ScopedCFTypeRef<CTFontRef> font = MatchFontFamily(
      AtomicString(
          "ThisFontNameDoesNotExist07F444B9-4DDF-4A41-8F30-C80D4ED4CCA2"),
      kNormalWeightValue, kNormalSlopeValue, kNormalWidthValue, 12);
  EXPECT_FALSE(font);
}

TEST(FontMatcherMacTest, FontFamilyMatchingLastResortFont) {
  ScopedCFTypeRef<CTFontRef> last_resort_font =
      MatchFontFamily(AtomicString("lastresort"), kNormalWeightValue,
                      kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_FALSE(last_resort_font);

  ScopedCFTypeRef<CTFontRef> last_resort_font_bold =
      MatchFontFamily(AtomicString("lastresort"), kBoldWeightValue,
                      kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_FALSE(last_resort_font_bold);
}

TEST(FontMatcherMacTest, MatchUniqueUnavailableFont) {
  ScopedCFTypeRef<CTFontRef> font = MatchUniqueFont(
      AtomicString(
          "ThisFontNameDoesNotExist07F444B9-4DDF-4A41-8F30-C80D4ED4CCA2"),
      12);
  EXPECT_FALSE(font);
}

class TestFontMatchingByName : public testing::TestWithParam<FontName> {};

INSTANTIATE_TEST_SUITE_P(FontMatcherMacTest,
                         TestFontMatchingByName,
                         testing::ValuesIn(FontNames));

INSTANTIATE_TEST_SUITE_P(CommonFontMatcherMacTest,
                         TestFontMatchingByName,
                         testing::ValuesIn(CommonFontNames));

// We perform matching by PostScript name for legacy and compatibility reasons
// (Safari also does it), although CSS specs do not require that, see
// crbug.com/641861.
TEST_P(TestFontMatchingByName, MatchingByFamilyName) {
  const FontName font_name = TestFontMatchingByName::GetParam();
  TestFontMatchingByFamilyName(font_name.family_name);
}

TEST_P(TestFontMatchingByName, MatchingByPostscriptName) {
  const FontName font_name = TestFontMatchingByName::GetParam();
  TestFontMatchingByPostscriptName(font_name.postscript_name);
}

TEST_P(TestFontMatchingByName, MatchUniqueFontByFullFontName) {
  const FontName font_name = TestFontMatchingByName::GetParam();
  ScopedCFTypeRef<CTFontRef> font =
      MatchUniqueFont(AtomicString(font_name.full_font_name), 12);
  EXPECT_TRUE(font);
}

TEST_P(TestFontMatchingByName, MatchUniqueFontByPostscriptName) {
  const FontName font_name = TestFontMatchingByName::GetParam();
  ScopedCFTypeRef<CTFontRef> font =
      MatchUniqueFont(AtomicString(font_name.postscript_name), 12);
  EXPECT_TRUE(font);
}

class TestFontMatchingByNameAndWeight
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<FontName, int, bool>> {};

INSTANTIATE_TEST_SUITE_P(FontMatcherMacTest,
                         TestFontMatchingByNameAndWeight,
                         ::testing::Combine(::testing::ValuesIn(FontNames),
                                            ::testing::Range(100, 900, 100),
                                            ::testing::ValuesIn({true,
                                                                 false})));

INSTANTIATE_TEST_SUITE_P(
    CommonFontMatcherMacTest,
    TestFontMatchingByNameAndWeight,
    ::testing::Combine(::testing::ValuesIn(CommonFontNames),
                       ::testing::Range(100, 900, 100),
                       ::testing::ValuesIn({true, false})));

TEST_P(TestFontMatchingByNameAndWeight, TestCTAndNSMatchEqual) {
  struct FontName font_name;
  int weight;
  bool flag;
  std::tie(font_name, weight, flag) = GetParam();
  ScopedFontFamilyPostscriptMatchingCTMigrationForTest scoped_feature(flag);
  // AppKit computes weight values of some fonts not as discrete as CoreText.
  // This is causing matching results of CoreText approach for some weight
  // values of several font families to be more precise than AppKit's. For
  // instance, if desired weight is 300, with AppKit approach will match
  // "HelveticaNeue-Thin" font, while with CoreText we will match
  // "HelveticaNeue-Light". This fonts should be skipped in this test.
  // This issue is described in the comment under
  // "MatchFamilyWithWeightVariations" test.
  if (strcmp(font_name.family_name, "Helvetica Neue") == 0 ||
      strcmp(font_name.family_name, "Hiragino Sans") == 0) {
    return;
  }
  TestCTAndNSMatchEqual(font_name.family_name, 11, weight, kNormalSlopeValue,
                        kNormalWidthValue);
  TestCTAndNSMatchEqual(font_name.family_name, 11, weight, kItalicSlopeValue,
                        kNormalWidthValue);
  TestCTAndNSMatchEqual(font_name.postscript_name, 11, weight,
                        kNormalSlopeValue, kNormalWidthValue);
  TestCTAndNSMatchEqual(font_name.postscript_name, 11, weight,
                        kItalicSlopeValue, kNormalWidthValue);
}

class TestFontWithTraitsMatching : public testing::TestWithParam<const char*> {
};

INSTANTIATE_TEST_SUITE_P(FontMatcherMacTest,
                         TestFontWithTraitsMatching,
                         testing::ValuesIn(FamiliesWithBoldItalicFaces));

TEST_P(TestFontWithTraitsMatching, FontFamilyMatchingWithBoldItalicTraits) {
  const char* font_name = TestFontWithTraitsMatching::GetParam();
  TestFontWithBoldAndItalicTraits(AtomicString(font_name));
}

TEST(FontMatcherMacTest, FontFamilyMatchingWithBoldCondensedTraits) {
  AtomicString family_name = AtomicString("American Typewriter");
  ScopedCFTypeRef<CTFontRef> font_condensed =
      MatchFontFamily(family_name, kNormalWeightValue, kNormalSlopeValue,
                      kCondensedWidthValue, 11);
  EXPECT_TRUE(font_condensed);

  CTFontSymbolicTraits condensed_font_traits =
      CTFontGetSymbolicTraits(font_condensed.get());
  EXPECT_TRUE(condensed_font_traits & NSFontCondensedTrait);

  ScopedCFTypeRef<CTFontRef> font_bold_condensed =
      MatchFontFamily(family_name, kBoldWeightValue, kNormalSlopeValue,
                      kCondensedWidthValue, 11);
  EXPECT_TRUE(font_bold_condensed.get());

  CTFontSymbolicTraits bold_condensed_font_traits =
      CTFontGetSymbolicTraits(font_bold_condensed.get());
  EXPECT_TRUE(bold_condensed_font_traits & NSFontCondensedTrait);
  EXPECT_TRUE(bold_condensed_font_traits & NSFontCondensedTrait);
}

TEST(FontMatcherMacTest, MatchFamilyWithWeightVariations) {
  // For some fonts AppKit returns inconsistent weight values in the font
  // information, retrieved using `availableFontsForFamily`. For instance, both
  // "NotoSansMyanmar-Light" and "NotoSansMyanmar-Thin" have AppKit weight value
  // of 3, while "NotoSansMyanmar-Thin" should be thinner than
  // "NotoSansMyanmar-Light".
  // This behavior is affecting matching results. For instance, in this test, if
  // the "FontFamilyStyleMatchingCTMigration" flag is off, we are using
  // `availableFontsForFamily`, so for `weight=300` we will match
  // "NotoSansMyanmar-Thin" font instead of "NotoSansMyanmar-Light".
  // The same issue might appear with CoreText but less often. For instance, for
  // both "AppleSDGothicNeo-Heavy" and "AppleSDGothicNeo-ExtraBold" CoreText
  // returns font weight value 0.56, although weight value of
  // "AppleSDGothicNeo-Heavy" is higher than weight value of
  // "AppleSDGothicNeo-ExtraBold". However, for fonts in "Noto Sans Myanmar"
  // family CoreText returns the correct weight values.
  // Hence we only run this test with the "FontFamilyStyleMatchingCTMigration"
  // flag on.
  ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(true);
  AtomicString family_name = AtomicString("Noto Sans Myanmar");
  for (int weight = 100; weight <= 900; weight += 100) {
    ScopedCFTypeRef<CTFontRef> font =
        MatchFontFamily(family_name, FontSelectionValue(weight),
                        kNormalSlopeValue, kNormalWidthValue, 11);
    NSDictionary* traits = CFToNSOwnershipCast(CTFontCopyTraits(font.get()));
    NSNumber* actual_weight_num =
        ObjCCast<NSNumber>(traits[CFToNSPtrCast(kCTFontWeightTrait)]);

    float actual_ct_weight = actual_weight_num.floatValue;
    int actual_weight = ToCSSFontWeight(actual_ct_weight);
    EXPECT_EQ(actual_weight, weight);
  }
}

}  // namespace blink
