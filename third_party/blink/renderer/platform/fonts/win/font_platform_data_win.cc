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

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

SkFont FontPlatformData::CreateSkFont(const FontDescription*) const {
  SkFont font(typeface_);
  font.setSize(SkFloatToScalar(text_size_));
  font.setEmbolden(synthetic_bold_);
  font.setSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);

  bool use_subpixel_rendering = style_.use_subpixel_rendering;
  bool use_anti_alias = style_.use_anti_alias;

  if (RuntimeEnabledFeatures::DisableAhemAntialiasEnabled() && IsAhem()) {
    use_subpixel_rendering = false;
    use_anti_alias = false;
  }

  if (use_subpixel_rendering) {
    font.setEdging(SkFont::Edging::kSubpixelAntiAlias);
  } else if (use_anti_alias) {
    font.setEdging(SkFont::Edging::kAntiAlias);
  } else {
    font.setEdging(SkFont::Edging::kAlias);
  }

  // Only use sub-pixel positioning if anti aliasing is enabled. Otherwise,
  // without font smoothing, subpixel text positioning leads to uneven spacing
  // since subpixel test placement coordinates would be passed to Skia, which
  // only has non-antialiased glyphs to draw, so they necessarily get clamped at
  // pixel positions, which leads to uneven spacing, either too close or too far
  // away from adjacent glyphs. We avoid this by linking the two flags.
  if (use_anti_alias) {
    font.setSubpixel(true);
  }

  if (WebTestSupport::IsRunningWebTest() &&
      !WebTestSupport::IsTextSubpixelPositioningAllowedForTest()) {
    font.setSubpixel(false);
  }

  font.setEmbeddedBitmaps(!avoid_embedded_bitmaps_);
  return font;
}

WebFontRenderStyle FontPlatformData::QuerySystemForRenderStyle() {
  WebFontRenderStyle style;
  style.use_anti_alias = 0;
  style.use_subpixel_rendering = 0;

  if (WebTestSupport::IsRunningWebTest()) {
    if (WebTestSupport::IsFontAntialiasingEnabledForTest()) {
      style.use_anti_alias = 1;
    }
    return style;
  }

  if (FontCache::Get().AntialiasedTextEnabled()) {
    style.use_anti_alias = 1;
    if (FontCache::Get().LcdTextEnabled()) {
      style.use_subpixel_rendering = 1;
    }
  }

  return style;
}

}  // namespace blink
