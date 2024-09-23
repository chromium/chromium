/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

#include "base/test/task_environment.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::CreateTestFont;

namespace blink {

class FontPlatformDataTest : public FontTestBase {};

TEST_F(FontPlatformDataTest, AhemHasNoSpaceInLigaturesOrKerning) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 16);
  const FontPlatformData& platform_data = font.PrimaryFont()->PlatformData();
  TypesettingFeatures features = kKerning | kLigatures;

  EXPECT_FALSE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasSpaceInLigaturesOrKerning) {
  Font font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font.PrimaryFont()->PlatformData();
  TypesettingFeatures features = kKerning | kLigatures;

  EXPECT_TRUE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasNoSpaceWithoutFontFeatures) {
  Font font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font.PrimaryFont()->PlatformData();
  TypesettingFeatures features = 0;

  EXPECT_FALSE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemHasAliasing) {
  RuntimeEnabledFeaturesTestHelpers::ScopedDisableAhemAntialias
      scoped_feature_list_(true);
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 16);
  const FontPlatformData& platform_data = font.PrimaryFont()->PlatformData();
  SkFont sk_font = platform_data.CreateSkFont(/* FontDescription */ nullptr);
  EXPECT_EQ(sk_font.getEdging(), SkFont::Edging::kAlias);
}

// Two Font objects using the same underlying font (the "A" character extracted
// from Robot-Regular) but different sizes should have the same digest.
TEST_F(FontPlatformDataTest, TypefaceDigestForDifferentSizes_SameDigest) {
  Font size_16_font = CreateTestFont(
      AtomicString("robot-a"), test::PlatformTestDataPath("roboto-a.ttf"), 16);
  IdentifiableToken size_16_digest =
      size_16_font.PrimaryFont()->PlatformData().ComputeTypefaceDigest();
  Font size_32_font = CreateTestFont(
      AtomicString("robot-a"), test::PlatformTestDataPath("roboto-a.ttf"), 32);
  IdentifiableToken size_32_digest =
      size_32_font.PrimaryFont()->PlatformData().ComputeTypefaceDigest();
  EXPECT_EQ(size_16_digest, size_32_digest);
}

// Two Font objects using different underlying fonts should have different
// digests. The second font also has the "A" from Robot-Regular, but has the
// format 12 part of the CMAP character to glyph mapping table removed.
TEST_F(FontPlatformDataTest, TypefaceDigestForDifferentFonts_DifferentDigest) {
  Font font1 = CreateTestFont(AtomicString("robot-a"),
                              test::PlatformTestDataPath("roboto-a.ttf"), 16);
  IdentifiableToken digest1 =
      font1.PrimaryFont()->PlatformData().ComputeTypefaceDigest();
  Font font2 = CreateTestFont(
      AtomicString("robot-a"),
      test::PlatformTestDataPath("roboto-a-different-cmap.ttf"), 16);
  IdentifiableToken digest2 =
      font2.PrimaryFont()->PlatformData().ComputeTypefaceDigest();
  EXPECT_NE(digest1, digest2);
}

// A Font using the same underlying font should have the same digest on
// different platforms.
TEST_F(FontPlatformDataTest, TypefaceDigestCrossPlatform_SameDigest) {
  Font font = CreateTestFont(AtomicString("robot-a"),
                             test::PlatformTestDataPath("roboto-a.ttf"), 16);
  IdentifiableToken digest =
      font.PrimaryFont()->PlatformData().ComputeTypefaceDigest();

  // Calculated on Linux.
  IdentifiableToken expected_digest(6864445319287375520);
  EXPECT_EQ(digest, expected_digest);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(FontPlatformDataTest, GeometricPrecision) {
  const float saved_device_scale_factor = FontCache::DeviceScaleFactor();
  sk_sp<SkTypeface> typeface = skia::DefaultTypeface();
  const std::string name("name");
  const auto create_font_platform_data = [&]() {
    return MakeGarbageCollected<FontPlatformData>(
        typeface, name,
        /* text_size */ 10, /* synthetic_bold */ false,
        /* synthetic_italic */ false, kGeometricPrecision,
        ResolvedFontFeatures());
  };

  FontCache::SetDeviceScaleFactor(1.0f);
  const FontPlatformData* geometric_precision = create_font_platform_data();
  const WebFontRenderStyle& geometric_precision_style =
      geometric_precision->GetFontRenderStyle();
  EXPECT_EQ(geometric_precision_style.use_subpixel_positioning, true);
  EXPECT_EQ(geometric_precision_style.use_hinting, false);

  // DSF=1.5 means it's high resolution (use_subpixel_positioning) for both
  // Linux and ChromeOS. See |gfx GetFontRenderParams|.
  FontCache::SetDeviceScaleFactor(1.5f);
  const FontPlatformData* geometric_precision_high =
      create_font_platform_data();
  EXPECT_EQ(*geometric_precision, *geometric_precision_high);

  FontCache::SetDeviceScaleFactor(saved_device_scale_factor);
}
#endif

}  // namespace blink
