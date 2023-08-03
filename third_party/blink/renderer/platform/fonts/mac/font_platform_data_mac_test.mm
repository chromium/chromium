// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"

#include "base/apple/bridging.h"
#include "base/mac/foundation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/mac/font_matcher_mac.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"

namespace blink {

namespace {
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

sk_sp<SkTypeface> MakeSystemFontOfSize(float size) {
  return SkMakeTypefaceFromCTFont(base::apple::NSToCFPtrCast(MatchNSFontFamily(
      font_family_names::kSystemUi, 0, FontSelectionValue(400), size)));
}
}

TEST(FontPlatformDataMacTest, VariableOpticalSizingThreshold) {
  // Before macOS 10.15, the system font did not have variable optical sizing.
  // In 10.15, the system font has two optical sizes glued together at 19.9. In
  // 11.0, the system font has a real optical size axis with range 17-96.

  // Below the 11.0 axis minimum.
  sk_sp<SkTypeface> system_font(MakeSystemFontOfSize(12));
  if (@available(macOS 11.0, *)) {
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  } else {
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_FALSE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  }

  // Just smaller than the switch-over size in 10.15, which is 19.9.
  system_font = MakeSystemFontOfSize(19);
  if (@available(macOS 11.0, *)) {
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  } else {
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_FALSE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  }

  // Just larger than the switch-over size in 10.15, which is 19.9.
  system_font = MakeSystemFontOfSize(20);
  if (@available(macOS 11.0, *)) {
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  } else {
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  }

  // Above the 11.0 axis maximum.
  system_font = MakeSystemFontOfSize(128);
  if (@available(macOS 11.0, *)) {
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  } else {
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 6));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 12));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 17));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 18));
    EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 19));
    EXPECT_TRUE(
        VariableAxisChangeEffective(system_font.get(), kOpszTag, 19.8999));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 20));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 24));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 72));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 96));
    EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kOpszTag, 97));
  }
}

TEST(FontPlatformDataMacTest, VariableWeight) {
  sk_sp<SkTypeface> system_font(MakeSystemFontOfSize(19));
  EXPECT_FALSE(VariableAxisChangeEffective(system_font.get(), kWghtTag, 400));
  EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kWghtTag, 400.5));
  EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kWghtTag, 395.5));
  EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kWghtTag, 2000));
  EXPECT_TRUE(VariableAxisChangeEffective(system_font.get(), kWghtTag, 0));
}

TEST(FontPlatformDataMacTest, VariableWeightClamped) {
  sk_sp<SkTypeface> system_font(MakeSystemFontOfSize(19));

  SkFontArguments::VariationPosition::Coordinate coordinates[1] = {
      {kWghtTag, 1000}};

  SkFontArguments::VariationPosition variation_design_position_black{
      coordinates, 1};
  sk_sp<SkTypeface> black_typeface(
      system_font->makeClone(SkFontArguments().setVariationDesignPosition(
          variation_design_position_black)));

  EXPECT_FALSE(
      VariableAxisChangeEffective(black_typeface.get(), kWghtTag, 1001));
  EXPECT_FALSE(
      VariableAxisChangeEffective(black_typeface.get(), kWghtTag, 10000));
  EXPECT_TRUE(VariableAxisChangeEffective(black_typeface.get(), kWghtTag, 999));

  coordinates[0].value = 1;
  SkFontArguments::VariationPosition variation_design_position_thin{coordinates,
                                                                    1};
  sk_sp<SkTypeface> thin_typeface(
      system_font->makeClone(SkFontArguments().setVariationDesignPosition(
          variation_design_position_thin)));

  EXPECT_FALSE(VariableAxisChangeEffective(thin_typeface.get(), kWghtTag, 0));
  EXPECT_FALSE(VariableAxisChangeEffective(thin_typeface.get(), kWghtTag, -1));
  EXPECT_TRUE(VariableAxisChangeEffective(thin_typeface.get(), kWghtTag, 2));
}
}
