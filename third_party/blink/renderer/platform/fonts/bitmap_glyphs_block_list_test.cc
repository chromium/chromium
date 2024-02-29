// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/bitmap_glyphs_block_list.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"

namespace blink {

#if BUILDFLAG(IS_WIN)

class BlockListBitmapGlyphsTest : public FontTestBase {};

static void TestBitmapGlyphsBlockListed(AtomicString windows_family_name,
                                        bool block_listed_expected) {
  FontCache& font_cache = FontCache::Get();
  FontDescription font_description;
  font_description.SetFamily(FontFamily(
      windows_family_name, FontFamily::InferredTypeFor(windows_family_name)));
  const SimpleFontData* simple_font_data =
      font_cache.GetFontData(font_description, windows_family_name);
  ASSERT_TRUE(simple_font_data);
  const FontPlatformData& font_platform_data = simple_font_data->PlatformData();
  ASSERT_TRUE(font_platform_data.Typeface());
  ASSERT_EQ(block_listed_expected,
            BitmapGlyphsBlockList::ShouldAvoidEmbeddedBitmapsForTypeface(
                *font_platform_data.Typeface()));
}

TEST_F(BlockListBitmapGlyphsTest, Simsun) {
  TestBitmapGlyphsBlockListed(AtomicString("Simsun"), false);
}

TEST_F(BlockListBitmapGlyphsTest, Arial) {
  TestBitmapGlyphsBlockListed(AtomicString("Arial"), false);
}

TEST_F(BlockListBitmapGlyphsTest, Calibri) {
  TestBitmapGlyphsBlockListed(AtomicString("Calibri"), true);
}

#endif
}  // namespace blink
