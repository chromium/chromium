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
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::CreateTestFont;

namespace blink {

class FontPlatformDataTest : public FontTestBase {};

TEST_F(FontPlatformDataTest, AhemHasNoSpaceInLigaturesOrKerning) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features = kKerning | kLigatures;

  EXPECT_FALSE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasSpaceInLigaturesOrKerning) {
  Font* font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features = kKerning | kLigatures;

  EXPECT_TRUE(platform_data.HasSpaceInLigaturesOrKerning(features));
}

TEST_F(FontPlatformDataTest, AhemSpaceLigatureHasNoSpaceWithoutFontFeatures) {
  Font* font =
      CreateTestFont(AtomicString("AhemSpaceLigature"),
                     test::PlatformTestDataPath("AhemSpaceLigature.woff"), 16);
  const FontPlatformData& platform_data = font->PrimaryFont()->PlatformData();
  TypesettingFeatures features = 0;

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
