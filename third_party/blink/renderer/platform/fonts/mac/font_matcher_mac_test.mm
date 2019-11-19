// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"

#include <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"

namespace blink {

void TestSystemFontContainsString(FontSelectionValue desired_weight,
                                  NSString* substring) {
  NSFont* font =
      MatchNSFontFamily(font_family_names::kSystemUi, 0, desired_weight, 11);
  EXPECT_TRUE([font.description containsString:substring]);
}

TEST(FontMatcherMacTest, YosemiteFontWeights) {
  if (!base::mac::IsOS10_10())
    return;

  TestSystemFontContainsString(FontSelectionValue(100), @"-UltraLight");
  TestSystemFontContainsString(FontSelectionValue(200), @"-Thin");
  TestSystemFontContainsString(FontSelectionValue(300), @"-Light");
  TestSystemFontContainsString(FontSelectionValue(400), @"-Regular");
  TestSystemFontContainsString(FontSelectionValue(500), @"-Medium");
  TestSystemFontContainsString(FontSelectionValue(600), @"-Bold");
  TestSystemFontContainsString(FontSelectionValue(700), @"-Bold");
  TestSystemFontContainsString(FontSelectionValue(800), @"-Heavy");
  TestSystemFontContainsString(FontSelectionValue(900), @"-Heavy");
}

TEST(FontMatcherMacTest, NoUniqueFontMatchOnUnavailableFont) {
  NSFont* font = MatchUniqueFont(
      "ThisFontNameDoesNotExist07F444B9-4DDF-4A41-8F30-C80D4ED4CCA2", 12);
  EXPECT_FALSE(font);
}

// If these font names are unavaiable on future Mac OS versions, please try to
// find replacements or remove individual lines.
TEST(FontMatcherMacTest, MatchFullFontName) {
  const char* font_names[] = {"American Typewriter Condensed Light",
                              "Arial Narrow Bold Italic",
                              "Baskerville SemiBold Italic",
                              "Devanagari MT",
                              "DIN Alternate Bold",
                              "Gill Sans Light Italic",
                              "Iowan Old Style Titling",
                              "Malayalam Sangam MN",
                              "Hiragino Maru Gothic Pro W4",
                              "Hiragino Kaku Gothic StdN W8"};

  for (const char* font_name : font_names) {
    @autoreleasepool {
      NSFont* font = MatchUniqueFont(font_name, 12);
      EXPECT_TRUE(font);
    }
  }
}

// If these font names are unavaiable on future Mac OS versions, please try to
// find replacements or remove individual lines.
TEST(FontMatcherMacTest, MatchPostscriptName) {
  const char* font_names[] = {
      "AmericanTypewriter-CondensedLight",
      "ArialNarrow-BoldItalic",
      "Baskerville-SemiBoldItalic",
      "DevanagariMT",
      "DINAlternate-Bold",
      "GillSans-LightItalic",
      "IowanOldStyle-Titling",
      "MalayalamSangamMN",
      "HiraMaruPro-W4",
      "HiraKakuStdN-W8",
  };

  for (const char* font_name : font_names) {
    @autoreleasepool {
      NSFont* font = MatchUniqueFont(font_name, 12);
      EXPECT_TRUE(font);
    }
  }
}

}  // namespace blink
