// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#include <Foundation/Foundation.h>

#include <string_view>

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
  // `MatchNSFontFamily` uses AppKit's integer weights (`font_info[2]`), whereas
  // `MatchFontFamily` (when `MacFontWeightFromOS2` is enabled) reads
  // `usWeightClass` from the `OS/2` table. Skip families where the font's
  // declared `OS/2.usWeightClass` differs from AppKit's weight scale (e.g.,
  // "Helvetica Neue", "Hiragino Sans", "Avenir" where Book and Roman both
  // declare 400 and Black/Heavy declare 800/900, and "Gill Sans" where
  // UltraBold declares 1000).
  std::string_view family_name(font_name.family_name);
  if (family_name == "Helvetica Neue" || family_name == "Hiragino Sans" ||
      (RuntimeEnabledFeatures::MacFontWeightFromOS2Enabled() &&
       (family_name == "Avenir" || family_name == "Gill Sans"))) {
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
  // For some fonts, both AppKit (`availableMembersOfFontFamily:`) and CoreText
  // (`kCTFontWeightTrait`) report inconsistent weights: AppKit reports weight 3
  // for both "NotoSansMyanmar-Thin" and "NotoSansMyanmar-Light", while CoreText
  // swaps "NotoSansMyanmar-Thin" (`OS/2.usWeightClass` 100,
  // `kCTFontWeightTrait` -0.6 -> 200) and "NotoSansMyanmar-ExtraLight"
  // (`OS/2.usWeightClass` 200, `kCTFontWeightTrait` -0.8 -> 100). With
  // `MacFontWeightFromOS2` enabled, both paths read `usWeightClass` from the
  // `OS/2` table and match the expected face.
  struct TestCase {
    int requested_weight;
    const char* expected_ps_name;
  };
  constexpr TestCase kCases[] = {
      {100, "NotoSansMyanmar-Thin"},   {200, "NotoSansMyanmar-ExtraLight"},
      {300, "NotoSansMyanmar-Light"},  {400, "NotoSansMyanmar-Regular"},
      {500, "NotoSansMyanmar-Medium"}, {600, "NotoSansMyanmar-SemiBold"},
      {700, "NotoSansMyanmar-Bold"},   {800, "NotoSansMyanmar-ExtraBold"},
      {900, "NotoSansMyanmar-Black"},
  };
  AtomicString family_name = AtomicString("Noto Sans Myanmar");
  for (bool ct_migration : {false, true}) {
    ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(
        ct_migration);
    for (const auto& c : kCases) {
      ScopedCFTypeRef<CTFontRef> font =
          MatchFontFamily(family_name, FontSelectionValue(c.requested_weight),
                          kNormalSlopeValue, kNormalWidthValue, 11);
      ASSERT_TRUE(font) << "weight=" << c.requested_weight
                        << " ct_migration=" << ct_migration;
      ScopedCFTypeRef<CFStringRef> matched_postscript_name(
          CTFontCopyPostScriptName(font.get()));
      ScopedCFTypeRef<CFStringRef> expected_postscript_name(
          CFStringCreateWithCString(nullptr, c.expected_ps_name,
                                    kCFStringEncodingUTF8));
      EXPECT_EQ(CFStringCompare(matched_postscript_name.get(),
                                expected_postscript_name.get(),
                                kCFCompareCaseInsensitive),
                kCFCompareEqualTo)
          << "weight=" << c.requested_weight
          << " ct_migration=" << ct_migration;
    }
  }
}

TEST(FontMatcherMacTest, HiraginoSansWeightMatching) {
  struct TestCase {
    int requested_weight;
    const char* expected_ps_name;
  };
  constexpr TestCase kCases[] = {
      {100, "HiraginoSans-W0"}, {200, "HiraginoSans-W1"},
      {250, "HiraginoSans-W2"}, {300, "HiraginoSans-W3"},
      {400, "HiraginoSans-W4"}, {500, "HiraginoSans-W5"},
      {600, "HiraginoSans-W6"}, {700, "HiraginoSans-W7"},
      {800, "HiraginoSans-W8"}, {900, "HiraginoSans-W9"},
  };
  for (bool ct_migration : {false, true}) {
    ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(
        ct_migration);
    for (const auto& c : kCases) {
      ScopedCFTypeRef<CTFontRef> font = MatchFontFamily(
          AtomicString("Hiragino Sans"), FontSelectionValue(c.requested_weight),
          kNormalSlopeValue, kNormalWidthValue, 11);
      ASSERT_TRUE(font) << "weight=" << c.requested_weight
                        << " ct_migration=" << ct_migration;
      ScopedCFTypeRef<CFStringRef> matched_postscript_name(
          CTFontCopyPostScriptName(font.get()));
      ScopedCFTypeRef<CFStringRef> expected_postscript_name(
          CFStringCreateWithCString(nullptr, c.expected_ps_name,
                                    kCFStringEncodingUTF8));
      EXPECT_EQ(CFStringCompare(matched_postscript_name.get(),
                                expected_postscript_name.get(),
                                kCFCompareCaseInsensitive),
                kCFCompareEqualTo)
          << "weight=" << c.requested_weight
          << " ct_migration=" << ct_migration;
    }
  }
}

// Verify that font weight matching follows the CSS Fonts 4 section 5.2
// directional search rules. Helvetica on macOS has Light (300), Regular (400),
// and Bold (700) faces. Specifically:
// - Weight 500 should match Regular (400), not Bold (700): for desired in
//   [400, 500], lighter weights are checked before heavier weights.
// - Weight 600 should match Bold (700), not Regular (400): for desired > 500,
//   heavier weights are checked first.
// - Weights 100-300 should match Light (300): for desired < 400, lighter
//   weights are checked first, then heavier.
TEST(FontMatcherMacTest, FontWeightSearchDirection) {
  struct WeightExpectation {
    int weight;
    const char* expected_ps_name;
  };
  constexpr WeightExpectation kExpectations[] = {
      {100, "Helvetica-Light"}, {200, "Helvetica-Light"},
      {300, "Helvetica-Light"}, {400, "Helvetica"},
      {500, "Helvetica"},       {600, "Helvetica-Bold"},
      {700, "Helvetica-Bold"},  {800, "Helvetica-Bold"},
      {900, "Helvetica-Bold"},
  };

  for (bool ct_migration : {false, true}) {
    ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(
        ct_migration);
    for (const auto& [weight, expected_ps] : kExpectations) {
      ScopedCFTypeRef<CTFontRef> font =
          MatchFontFamily(AtomicString("Helvetica"), FontSelectionValue(weight),
                          kNormalSlopeValue, kNormalWidthValue, 11);
      ASSERT_TRUE(font) << "Failed to match Helvetica at weight " << weight;

      ScopedCFTypeRef<CFStringRef> actual_ps(
          CTFontCopyPostScriptName(font.get()));
      ScopedCFTypeRef<CFStringRef> expected_cf(CFStringCreateWithCString(
          nullptr, expected_ps, kCFStringEncodingUTF8));
      EXPECT_EQ(CFStringCompare(actual_ps.get(), expected_cf.get(),
                                kCFCompareCaseInsensitive),
                kCFCompareEqualTo)
          << "At weight " << weight << " (ct_migration=" << ct_migration
          << "): expected " << expected_ps << " but got "
          << base::apple::CFToNSPtrCast(actual_ps.get()).UTF8String;
    }
  }
}

// Verify sub-400 directional search with Helvetica Neue, which has UltraLight
// (100), Thin (200), Light (300), and Regular (400). Per CSS Fonts 4 §5.2,
// when desired weight < 400, weights <= desired are searched in descending
// order first. For example, weight 350 should match Light (300), not Regular
// (400), and weight 250 should match Thin (200), not Light (300).
TEST(FontMatcherMacTest, FontWeightSearchDirectionSub400) {
  struct WeightExpectation {
    int weight;
    const char* expected_ps_name;
  };
  constexpr WeightExpectation kExpectations[] = {
      {100, "HelveticaNeue-UltraLight"}, {150, "HelveticaNeue-UltraLight"},
      {190, "HelveticaNeue-UltraLight"}, {200, "HelveticaNeue-Thin"},
      {250, "HelveticaNeue-Thin"},       {290, "HelveticaNeue-Thin"},
      {300, "HelveticaNeue-Light"},      {350, "HelveticaNeue-Light"},
      {390, "HelveticaNeue-Light"},      {400, "HelveticaNeue"},
  };

  for (bool ct_migration : {false, true}) {
    ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(
        ct_migration);
    for (const auto& [weight, expected_ps] : kExpectations) {
      ScopedCFTypeRef<CTFontRef> font = MatchFontFamily(
          AtomicString("Helvetica Neue"), FontSelectionValue(weight),
          kNormalSlopeValue, kNormalWidthValue, 11);
      ASSERT_TRUE(font) << "Failed to match Helvetica Neue at weight "
                        << weight;

      ScopedCFTypeRef<CFStringRef> actual_ps(
          CTFontCopyPostScriptName(font.get()));
      ScopedCFTypeRef<CFStringRef> expected_cf(CFStringCreateWithCString(
          nullptr, expected_ps, kCFStringEncodingUTF8));
      EXPECT_EQ(CFStringCompare(actual_ps.get(), expected_cf.get(),
                                kCFCompareCaseInsensitive),
                kCFCompareEqualTo)
          << "At weight " << weight << " (ct_migration=" << ct_migration
          << "): expected " << expected_ps << " but got "
          << base::apple::CFToNSPtrCast(actual_ps.get()).UTF8String;
    }
  }
}

// Verify that CJK system fonts with multiple sub-400 weights resolve to the
// Light (300) face at font-weight 360. Per CSS Fonts 4 §5.2, desired weight
// 360 (< 400) searches weights <= 360 in descending order first, selecting
// HiraginoSans-W3 (OS/2 weight 300), PingFangTC-Light (300), and
// PingFangSC-Light (300). See https://crbug.com/516316384 and
// https://crbug.com/543243014.
TEST(FontMatcherMacTest, ConsistentLightMatchAcrossCJKFamilies) {
  struct FamilyExpectation {
    const char* family;
    const char* expected_ps_name;
  };
  constexpr FamilyExpectation kFamilies[] = {
      {"Hiragino Sans", "HiraginoSans-W3"},
      {"PingFang TC", "PingFangTC-Light"},
      {"PingFang SC", "PingFangSC-Light"},
  };

  for (bool ct_migration : {false, true}) {
    ScopedFontFamilyStyleMatchingCTMigrationForTest scoped_feature(
        ct_migration);
    for (const auto& [family, expected_ps] : kFamilies) {
      ScopedCFTypeRef<CTFontRef> font =
          MatchFontFamily(AtomicString(family), FontSelectionValue(360),
                          kNormalSlopeValue, kNormalWidthValue, 11);
      ASSERT_TRUE(font) << "Failed to match " << family << " at weight 360";

      ScopedCFTypeRef<CFStringRef> actual_ps(
          CTFontCopyPostScriptName(font.get()));
      ScopedCFTypeRef<CFStringRef> expected_cf(CFStringCreateWithCString(
          nullptr, expected_ps, kCFStringEncodingUTF8));
      EXPECT_EQ(CFStringCompare(actual_ps.get(), expected_cf.get(),
                                kCFCompareCaseInsensitive),
                kCFCompareEqualTo)
          << "For " << family << " at weight 360 (ct_migration=" << ct_migration
          << "): expected " << expected_ps << " but got "
          << base::apple::CFToNSPtrCast(actual_ps.get()).UTF8String;
    }
  }
}

}  // namespace blink
