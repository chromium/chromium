/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, 2012 Google Inc. All rights reserved.
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

#include <windows.h>
#include "SkTypeface.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"

namespace blink {

// Maximum font size, in pixels, at which embedded bitmaps will be used
// if available.
const float kMaxSizeForEmbeddedBitmap = 24.0f;

void FontPlatformData::SetupSkPaint(SkPaint* font, float, const Font*) const {
  const float ts = text_size_ >= 0 ? text_size_ : 12;
  font->setTextSize(SkFloatToScalar(text_size_));
  font->setTypeface(typeface_);
  font->setFakeBoldText(synthetic_bold_);
  font->setTextSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);

  uint32_t text_flags = PaintTextFlags();
  uint32_t flags = font->getFlags();
  static const uint32_t kTextFlagsMask =
      SkPaint::kAntiAlias_Flag | SkPaint::kLCDRenderText_Flag |
      SkPaint::kEmbeddedBitmapText_Flag | SkPaint::kSubpixelText_Flag;
  flags &= ~kTextFlagsMask;

  if (ts <= kMaxSizeForEmbeddedBitmap)
    flags |= SkPaint::kEmbeddedBitmapText_Flag;

  // Only use sub-pixel positioning if anti aliasing is enabled. Otherwise,
  // without font smoothing, subpixel text positioning leads to uneven spacing
  // since subpixel test placement coordinates would be passed to Skia, which
  // only has non-antialiased glyphs to draw, so they necessarily get clamped at
  // pixel positions, which leads to uneven spacing, either too close or too far
  // away from adjacent glyphs. We avoid this by linking the two flags.
  if (text_flags & SkPaint::kAntiAlias_Flag)
    flags |= SkPaint::kSubpixelText_Flag;

  if (LayoutTestSupport::IsRunningLayoutTest() &&
      !LayoutTestSupport::IsTextSubpixelPositioningAllowedForTest())
    flags &= ~SkPaint::kSubpixelText_Flag;

  SkASSERT(!(text_flags & ~kTextFlagsMask));
  flags |= text_flags;

  font->setFlags(flags);

  font->setEmbeddedBitmapText(!avoid_embedded_bitmaps_);
}

static bool IsWebFont(const String& family_name) {
  // Web-fonts have artifical names constructed to always be:
  // 1. 24 characters, followed by a '\0'
  // 2. the last two characters are '=='
  return family_name.length() == 24 && '=' == family_name[22] &&
         '=' == family_name[23];
}

static int ComputePaintTextFlags(String font_family_name) {
  if (LayoutTestSupport::IsRunningLayoutTest())
    return LayoutTestSupport::IsFontAntialiasingEnabledForTest()
               ? SkPaint::kAntiAlias_Flag
               : 0;

  int text_flags = 0;
  if (FontCache::GetFontCache()->AntialiasedTextEnabled()) {
    int lcd_flag = FontCache::GetFontCache()->LcdTextEnabled()
                       ? SkPaint::kLCDRenderText_Flag
                       : 0;
    text_flags = SkPaint::kAntiAlias_Flag | lcd_flag;
  }

  // Many web-fonts are so poorly hinted that they are terrible to read when
  // drawn in BW.  In these cases, we have decided to FORCE these fonts to be
  // drawn with at least grayscale AA, even when the System (getSystemTextFlags)
  // tells us to draw only in BW.
  if (IsWebFont(font_family_name))
    text_flags |= SkPaint::kAntiAlias_Flag;

  return text_flags;
}

void FontPlatformData::QuerySystemForRenderStyle() {
  paint_text_flags_ = ComputePaintTextFlags(FontFamilyName());
}

}  // namespace blink
