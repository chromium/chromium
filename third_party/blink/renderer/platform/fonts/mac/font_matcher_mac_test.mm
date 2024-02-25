// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

using base::apple::CFCast;
using base::apple::GetValueFromDictionary;
using base::apple::NSToCFPtrCast;
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

ScopedCFTypeRef<CTFontRef> MatchCTFontFamily(const AtomicString& font_name,
                                             FontSelectionValue desired_weight,
                                             FontSelectionValue desired_slant,
                                             FontSelectionValue desired_width,
                                             float size) {
  if (RuntimeEnabledFeatures::FontMatchingCTMigrationEnabled()) {
    return MatchFontFamily(font_name, desired_weight, desired_slant,
                           desired_width, size);
  }

  NSFontTraitMask traits = 0;
  if (desired_slant != kNormalSlopeValue) {
    traits |= NSFontItalicTrait;
  }
  if (desired_width > kNormalWidthValue) {
    traits |= NSFontExpandedTrait;
  }
  if (desired_width < kNormalWidthValue) {
    traits |= NSFontCondensedTrait;
  }

  return ScopedCFTypeRef<CTFontRef>(NSToCFPtrCast(
      MatchNSFontFamily(font_name, traits, desired_weight, size)));
}

void TestFontWithBoldAndItalicTraits(const AtomicString& font_name) {
  ScopedCFTypeRef<CTFontRef> font_italic = MatchCTFontFamily(
      font_name, kNormalWeightValue, kItalicSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font_italic);

  CTFontSymbolicTraits italic_font_traits =
      CTFontGetSymbolicTraits(font_italic.get());
  EXPECT_TRUE(italic_font_traits & kCTFontTraitItalic);

  ScopedCFTypeRef<CTFontRef> font_bold_italic = MatchCTFontFamily(
      font_name, kBoldWeightValue, kItalicSlopeValue, kNormalWidthValue, 11);
  EXPECT_TRUE(font_bold_italic);

  CTFontSymbolicTraits bold_italic_font_traits =
      CTFontGetSymbolicTraits(font_bold_italic.get());
  EXPECT_TRUE(bold_italic_font_traits & kCTFontTraitItalic);
  EXPECT_TRUE(bold_italic_font_traits & kCTFontTraitBold);
}

void TestFontMatchingByFamilyName(const char* font_name) {
  ScopedCFTypeRef<CTFontRef> font =
      MatchCTFontFamily(AtomicString(font_name), kNormalWeightValue,
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
      MatchCTFontFamily(AtomicString(font_name), kNormalWeightValue,
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
  ScopedCFTypeRef<CFDictionaryRef> traits(CTFontCopyTraits(font.get()));
  CFNumberRef slant_num = base::apple::GetValueFromDictionary<CFNumberRef>(
      traits.get(), kCTFontSlantTrait);
  float slant;
  CFNumberGetValue(slant_num, kCFNumberFloatType, &slant);
  EXPECT_NE(slant, 0.0);
}

TEST(FontMatcherMacTest, MatchSystemFontWithWeightVariations) {
  // Mac SystemUI font supports weight variations between 1 and 1000.
  int min_weight = 1;
  int max_weight = 1000;
  FourCharCode wght_tag = 'wght';
  ScopedCFTypeRef<CFNumberRef> wght_tag_num(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &wght_tag));
  for (int weight = min_weight - 1; weight <= max_weight + 1; weight += 50) {
    if (weight != kNormalWeightValue) {
      ScopedCFTypeRef<CTFontRef> font = MatchSystemUIFont(
          FontSelectionValue(weight), kNormalSlopeValue, kNormalWidthValue, 11);
      EXPECT_TRUE(font);
      ScopedCFTypeRef<CFDictionaryRef> variations(
          CTFontCopyVariation(font.get()));
      CFNumberRef actual_weight_cf_num = CFCast<CFNumberRef>(
          CFDictionaryGetValue(variations.get(), wght_tag_num.get()));
      EXPECT_TRUE(actual_weight_cf_num);
      float actual_weight = 0.0;
      CFNumberGetValue(actual_weight_cf_num, kCFNumberFloatType,
                       &actual_weight);
      float expected_weight =
          std::max(min_weight, std::min(max_weight, weight));
      EXPECT_EQ(actual_weight, expected_weight);
    }
  }
}

TEST(FontMatcherMacTest, MatchSystemFontWithWidthVariations) {
  // Font width variations are supported from Mac OS 11 and later.
  if (@available(macos 11, *)) {
    // Mac SystemUI font supports width variations between 30 and 150.
    int min_width = 30;
    int max_width = 150;
    FourCharCode wdth_tag = 'wdth';
    ScopedCFTypeRef<CFNumberRef> wdth_tag_num(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &wdth_tag));
    for (int width = min_width - 10; width <= max_width + 10; width += 10) {
      if (width != kNormalWidthValue) {
        ScopedCFTypeRef<CTFontRef> font =
            MatchSystemUIFont(kNormalWidthValue, kNormalSlopeValue,
                              FontSelectionValue(width), 11);
        EXPECT_TRUE(font);

        ScopedCFTypeRef<CFDictionaryRef> variations(
            CTFontCopyVariation(font.get()));
        CFNumberRef actual_width_cf_num = CFCast<CFNumberRef>(
            CFDictionaryGetValue(variations.get(), wdth_tag_num.get()));
        EXPECT_TRUE(actual_width_cf_num);

        float actual_width = 0.0;
        CFNumberGetValue(actual_width_cf_num, kCFNumberFloatType,
                         &actual_width);
        float expected_width = std::max(min_width, std::min(max_width, width));
        EXPECT_EQ(actual_width, expected_width);
      }
    }
  }
}

TEST(FontMatcherMacTest, FontFamilyMatchingUnavailableFont) {
  ScopedCFTypeRef<CTFontRef> font = MatchCTFontFamily(
      AtomicString(
          "ThisFontNameDoesNotExist07F444B9-4DDF-4A41-8F30-C80D4ED4CCA2"),
      kNormalWeightValue, kNormalSlopeValue, kNormalWidthValue, 12);
  EXPECT_FALSE(font);
}

TEST(FontMatcherMacTest, FontFamilyMatchingLastResortFont) {
  ScopedCFTypeRef<CTFontRef> last_resort_font =
      MatchCTFontFamily(AtomicString("lastresort"), kNormalWeightValue,
                        kNormalSlopeValue, kNormalWidthValue, 11);
  EXPECT_FALSE(last_resort_font);

  ScopedCFTypeRef<CTFontRef> last_resort_font_bold =
      MatchCTFontFamily(AtomicString("lastresort"), kBoldWeightValue,
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
      MatchCTFontFamily(family_name, kNormalWeightValue, kNormalSlopeValue,
                        kCondensedWidthValue, 11);
  EXPECT_TRUE(font_condensed);

  CTFontSymbolicTraits condensed_font_traits =
      CTFontGetSymbolicTraits(font_condensed.get());
  EXPECT_TRUE(condensed_font_traits & NSFontCondensedTrait);

  ScopedCFTypeRef<CTFontRef> font_bold_condensed =
      MatchCTFontFamily(family_name, kBoldWeightValue, kNormalSlopeValue,
                        kCondensedWidthValue, 11);
  EXPECT_TRUE(font_bold_condensed.get());

  CTFontSymbolicTraits bold_condensed_font_traits =
      CTFontGetSymbolicTraits(font_bold_condensed.get());
  EXPECT_TRUE(bold_condensed_font_traits & NSFontCondensedTrait);
  EXPECT_TRUE(bold_condensed_font_traits & NSFontCondensedTrait);
}

TEST(FontMatcherMacTest, MatchFamilyWithWeightVariations) {
  AtomicString family_name = AtomicString("Noto Sans Myanmar");
  for (int weight = 100; weight < 900; weight += 100) {
    ScopedCFTypeRef<CTFontRef> font =
        MatchCTFontFamily(family_name, FontSelectionValue(weight),
                          kNormalSlopeValue, kCondensedWidthValue, 11);
    ScopedCFTypeRef<CFDictionaryRef> traits_dict(CTFontCopyTraits(font.get()));
    CFNumberRef actual_weight_num = GetValueFromDictionary<CFNumberRef>(
        traits_dict.get(), kCTFontWeightTrait);
    float actual_ct_weight;
    CFNumberGetValue(actual_weight_num, kCFNumberFloatType, &actual_ct_weight);
    int actual_weight = ToCSSFontWeight(actual_ct_weight);
    EXPECT_EQ(actual_weight, weight);
  }
}

}  // namespace blink
