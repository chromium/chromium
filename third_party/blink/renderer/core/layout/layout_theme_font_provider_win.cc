/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

constexpr float kDefaultFontSize = 16.0;

// Converts |points| to pixels. One point is 1/72 of an inch.
static float PointsToPixels(float points) {
  const float kPixelsPerInch = 96.0f;
  const float kPointsPerInch = 72.0f;
  return points / kPointsPerInch * kPixelsPerInch;
}

// static
void LayoutThemeFontProvider::SystemFont(CSSValueID system_font_id,
                                         FontSelectionValue& font_style,
                                         FontSelectionValue& font_weight,
                                         float& font_size,
                                         AtomicString& font_family) {
  font_style = NormalSlopeValue();
  font_weight = NormalWeightValue();

  switch (system_font_id) {
    case CSSValueID::kSmallCaption:
      font_size = FontCache::SmallCaptionFontHeight();
      font_family = FontCache::SmallCaptionFontFamily();
      break;
    case CSSValueID::kMenu:
      font_size = FontCache::MenuFontHeight();
      font_family = FontCache::MenuFontFamily();
      break;
    case CSSValueID::kStatusBar:
      font_size = FontCache::StatusFontHeight();
      font_family = FontCache::StatusFontFamily();
      break;
    case CSSValueID::kWebkitMiniControl:
    case CSSValueID::kWebkitSmallControl:
    case CSSValueID::kWebkitControl:
      // Why 2 points smaller? Because that's what Gecko does.
      font_size = kDefaultFontSize - PointsToPixels(2);
      font_family = DefaultGUIFont();
      break;
    default:
      font_size = kDefaultFontSize;
      font_family = DefaultGUIFont();
      break;
  }
}

}  // namespace blink
