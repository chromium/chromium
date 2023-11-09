// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

void TestFontWithWeights(const AtomicString& font_name) {
  float ns_weights[] = {
      -0.80,  // NSFontWeightUltraLight
      -0.60,  // NSFontWeightThin
      -0.40,  // NSFontWeightLight
      0.0,    // NSFontWeightRegular
      0.23,   // NSFontWeightMedium
      0.30,   // NSFontWeightSemibold
      0.40,   // NSFontWeightBold
      0.56,   // NSFontWeightHeavy
      0.62,   // NSFontWeightBlack
  };
  for (size_t i = 0; i < 9; i++) {
    @autoreleasepool {
      int weight = (i + 1) * 100;
      NSFont* font =
          MatchNSFontFamily(font_name, 0, FontSelectionValue(weight), 11);
      EXPECT_TRUE(font);

      ScopedCFTypeRef<CFDictionaryRef> traits(
          CTFontCopyTraits(NSToCFPtrCast(font)));
      CFNumberRef actual_weight_cf_num =
          base::apple::GetValueFromDictionary<CFNumberRef>(traits.get(),
                                                           kCTFontWeightTrait);
      float actual_weight = 0.0;
      CFNumberGetValue(actual_weight_cf_num, kCFNumberFloatType,
                       &actual_weight);
      EXPECT_EQ(actual_weight, ns_weights[i]);
    }
  }
}

void TestFontWithBoldAndItalicTraits(const AtomicString& font_name) {
  NSFont* font_italic =
      MatchNSFontFamily(font_name, NSFontItalicTrait, kNormalWeightValue, 11);
  EXPECT_TRUE(font_italic);

  CTFontSymbolicTraits italic_font_traits =
      CTFontGetSymbolicTraits(NSToCFPtrCast(font_italic));
  EXPECT_TRUE(italic_font_traits & NSFontItalicTrait);

  NSFont* font_bold_italic =
      MatchNSFontFamily(font_name, NSFontItalicTrait, kBoldWeightValue, 11);
  EXPECT_TRUE(font_bold_italic);

  CTFontSymbolicTraits bold_italic_font_traits =
      CTFontGetSymbolicTraits(NSToCFPtrCast(font_bold_italic));
  EXPECT_TRUE(bold_italic_font_traits & NSFontItalicTrait);
  EXPECT_TRUE(bold_italic_font_traits & NSFontBoldTrait);
}

void TestFontMatchingByNameByFamilyName(const char* font_name) {
  NSFont* font =
      MatchNSFontFamily(AtomicString(font_name), 0, kNormalWeightValue, 11);
  EXPECT_TRUE(font);
  ScopedCFTypeRef<CFStringRef> matched_family_name(
      CTFontCopyFamilyName(base::apple::NSToCFPtrCast(font)));
  ScopedCFTypeRef<CFStringRef> expected_family_name(
      CFStringCreateWithCString(nullptr, font_name, kCFStringEncodingUTF8));
  EXPECT_EQ(
      CFStringCompare(matched_family_name.get(), expected_family_name.get(),
                      kCFCompareCaseInsensitive),
      kCFCompareEqualTo);
}

void TestFontMatchingByNameByPostscriptName(const char* font_name) {
  NSFont* font =
      MatchNSFontFamily(AtomicString(font_name), 0, kNormalWeightValue, 11);
  EXPECT_TRUE(font);
  ScopedCFTypeRef<CFStringRef> matched_postscript_name(
      CTFontCopyPostScriptName(base::apple::NSToCFPtrCast(font)));
  ScopedCFTypeRef<CFStringRef> expected_postscript_name(
      CFStringCreateWithCString(nullptr, font_name, kCFStringEncodingUTF8));
  EXPECT_EQ(CFStringCompare(matched_postscript_name.get(),
                            expected_postscript_name.get(),
                            kCFCompareCaseInsensitive),
            kCFCompareEqualTo);
}

}  // namespace

TEST(FontMatcherMacTest, MatchSystemFont) {
  TestFontWithWeights(font_family_names::kSystemUi);
  TestFontWithBoldAndItalicTraits(font_family_names::kSystemUi);
}

TEST(FontMatcherMacTest, FontFamilyMatchingUnavailableFont) {
  NSFont* font = MatchNSFontFamily(
      AtomicString(
          "ThisFontNameDoesNotExist07F444B9-4DDF-4A41-8F30-C80D4ED4CCA2"),
      0, kNormalWeightValue, 12);
  EXPECT_FALSE(font);
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
  TestFontMatchingByNameByFamilyName(font_name.family_name);
}

TEST_P(TestFontMatchingByName, MatchingByPostscriptName) {
  const FontName font_name = TestFontMatchingByName::GetParam();
  TestFontMatchingByNameByPostscriptName(font_name.postscript_name);
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

}  // namespace blink
