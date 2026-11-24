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
#include "build/build_config.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"

using blink::test::CreateTestFont;

namespace blink {

class FontPlatformDataTest : public FontTestBase {};

TEST_F(FontPlatformDataTest, AhemHasNoSpaceInLigaturesOrKerning) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features = {TypesettingFeature::kKerning,
                                  TypesettingFeature::kLigatures};

  EXPECT_FALSE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasSpaceInLigaturesOrKerning) {
  Font* font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features = {TypesettingFeature::kKerning,
                                  TypesettingFeature::kLigatures};

  EXPECT_TRUE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasNoSpaceWithoutFontFeatures) {
  Font* font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features;

  EXPECT_FALSE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemHasAliasing) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  SkFont sk_font = platform_data.CreateSkFont(/* FontDescription */ nullptr);
  EXPECT_EQ(sk_font.getEdging(), SkFont::Edging::kAlias);
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
        /* synthetic_italic */ false, TextRenderingMode::kGeometricPrecision,
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

#if !BUILDFLAG(IS_MAC)
// Ensure full hinting maps to SkFontHinting::kNormal while preserving
// whole-pixel glyph positioning at 1x scale, so LCD surface rasterization
// matches Skia's canonicalized shaping metrics (crbug.com/472679086).
TEST_F(FontPlatformDataTest,
       WebFontRenderStyleFullHintingUsesNormalSkiaHinting) {
  ScopedWebTestMode web_test_mode(false);

  WebFontRenderStyle full_style;
  full_style.use_bitmaps = 0;
  full_style.use_auto_hint = 1;
  full_style.use_hinting = 1;
  full_style.hint_style = static_cast<char>(SkFontHinting::kFull);
  full_style.use_anti_alias = 1;
  full_style.use_subpixel_rendering = 1;
  full_style.use_subpixel_positioning = 0;

  SkFont full_font;
  full_style.ApplyToSkFont(&full_font);
  EXPECT_EQ(full_font.getHinting(), SkFontHinting::kNormal);
  EXPECT_EQ(full_font.getEdging(), SkFont::Edging::kSubpixelAntiAlias);
  EXPECT_FALSE(full_font.isSubpixel());
  EXPECT_FALSE(full_font.isLinearMetrics());

  // Contrast with kNormal input, which force-enables subpixel positioning
  // outside web tests even when use_subpixel_positioning is 0.
  WebFontRenderStyle normal_style = full_style;
  normal_style.hint_style = static_cast<char>(SkFontHinting::kNormal);
  SkFont normal_font;
  normal_style.ApplyToSkFont(&normal_font);
  EXPECT_EQ(normal_font.getHinting(), SkFontHinting::kNormal);
  EXPECT_TRUE(normal_font.isSubpixel());

  Font* roboto = CreateTestFont(
      AtomicString("Roboto"),
      test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"),
      16);
  sk_sp<SkTypeface> typeface =
      sk_ref_sp(roboto->PrimaryFont()->PlatformData().Typeface());
  ASSERT_TRUE(typeface);

  SkFont roboto_full(typeface, 16);
  full_style.ApplyToSkFont(&roboto_full);

  SkFont roboto_expected(typeface, 16);
  roboto_expected.setHinting(SkFontHinting::kNormal);
  roboto_expected.setEmbeddedBitmaps(false);
  roboto_expected.setForceAutoHinting(true);
  roboto_expected.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  roboto_expected.setSubpixel(false);
  roboto_expected.setLinearMetrics(false);

  const SkGlyphID glyph_t = typeface->unicharToGlyph('t');
  ASSERT_NE(glyph_t, 0u);
  EXPECT_FLOAT_EQ(roboto_full.getWidth(glyph_t),
                  roboto_expected.getWidth(glyph_t));

  const SkSurfaceProps lcd_props(0, kRGB_H_SkPixelGeometry);
  const SkImageInfo info = SkImageInfo::MakeN32Premul(32, 32);
  sk_sp<SkSurface> full_surface = SkSurfaces::Raster(info, &lcd_props);
  sk_sp<SkSurface> expected_surface = SkSurfaces::Raster(info, &lcd_props);
  ASSERT_TRUE(full_surface);
  ASSERT_TRUE(expected_surface);

  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  const SkPoint pos = SkPoint::Make(8, 24);

  full_surface->getCanvas()->clear(SK_ColorWHITE);
  full_surface->getCanvas()->drawGlyphs({&glyph_t, 1}, {&pos, 1}, {0, 0},
                                        roboto_full, paint);

  expected_surface->getCanvas()->clear(SK_ColorWHITE);
  expected_surface->getCanvas()->drawGlyphs({&glyph_t, 1}, {&pos, 1}, {0, 0},
                                            roboto_expected, paint);

  SkPixmap full_pixmap;
  SkPixmap expected_pixmap;
  ASSERT_TRUE(full_surface->peekPixels(&full_pixmap));
  ASSERT_TRUE(expected_surface->peekPixels(&expected_pixmap));
  for (int y = 0; y < info.height(); ++y) {
    for (int x = 0; x < info.width(); ++x) {
      EXPECT_EQ(full_pixmap.getColor(x, y), expected_pixmap.getColor(x, y))
          << "LCD pixel mismatch at (" << x << ", " << y << ")";
    }
  }
}
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace blink
