/*
 * Copyright (C) 2005, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/font_metrics.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/vdmx_parser.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMetrics.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
// This is the largest VDMX table which we'll try to load and parse.
static const size_t kMaxVDMXTableSize = 1024 * 1024;  // 1 MB
#endif

void FontMetrics::AscentDescentWithHacks(
    float& ascent,
    float& descent,
    unsigned& visual_overflow_inflation_for_ascent,
    unsigned& visual_overflow_inflation_for_descent,
    const FontPlatformData& platform_data,
    const SkFont& font,
    bool subpixel_ascent_descent) {
  SkTypeface* face = font.getTypeface();
  DCHECK(face);

  SkFontMetrics metrics;
  font.getMetrics(&metrics);

  int vdmx_ascent = 0, vdmx_descent = 0;
  bool is_vdmx_valid = false;

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
  // Manually digging up VDMX metrics is only applicable when bytecode hinting
  // using FreeType.  With DirectWrite or CoreText, no bytecode hinting is ever
  // done.  This code should be pushed into FreeType (hinted font metrics).
  static const uint32_t kVdmxTag = SkSetFourByteTag('V', 'D', 'M', 'X');
  int pixel_size = platform_data.size() + 0.5;
  if (!font.isForceAutoHinting() &&
      (font.getHinting() == SkFontHinting::kFull ||
       font.getHinting() == SkFontHinting::kNormal)) {
    size_t vdmx_size = face->getTableSize(kVdmxTag);
    if (vdmx_size && vdmx_size < kMaxVDMXTableSize) {
      uint8_t* vdmx_table = (uint8_t*)WTF::Partitions::FastMalloc(
          vdmx_size, WTF_HEAP_PROFILER_TYPE_NAME(FontMetrics));
      if (vdmx_table &&
          face->getTableData(kVdmxTag, 0, vdmx_size, vdmx_table) == vdmx_size &&
          ParseVDMX(&vdmx_ascent, &vdmx_descent, vdmx_table, vdmx_size,
                    pixel_size))
        is_vdmx_valid = true;
      WTF::Partitions::FastFree(vdmx_table);
    }
  }
#endif

  // Beware those who step here: This code is designed to match Win32 font
  // metrics *exactly* except:
  // - the adjustment of ascent/descent on Linux/Android
  // - metrics.fAscent and .fDesscent are not rounded to int for tiny fonts
  if (is_vdmx_valid) {
    ascent = vdmx_ascent;
    descent = -vdmx_descent;
  } else if (subpixel_ascent_descent &&
             (-metrics.fAscent < 3 ||
              -metrics.fAscent + metrics.fDescent < 2)) {
    // For tiny fonts, the rounding of fAscent and fDescent results in equal
    // baseline for different types of text baselines (crbug.com/338908).
    // Please see CanvasRenderingContext2D::getFontBaseline for the heuristic.
    ascent = -metrics.fAscent;
    descent = metrics.fDescent;
  } else {
    ascent = SkScalarRoundToScalar(-metrics.fAscent);
    descent = SkScalarRoundToScalar(metrics.fDescent);

    if (ascent < -metrics.fAscent)
      visual_overflow_inflation_for_ascent = 1;
    if (descent < metrics.fDescent) {
      visual_overflow_inflation_for_descent = 1;
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_FUCHSIA)
      // When subpixel positioning is enabled, if the descent is rounded down,
      // the descent part of the glyph may be truncated when displayed in a
      // 'overflow: hidden' container.  To avoid that, borrow 1 unit from the
      // ascent when possible.
      if (platform_data.GetFontRenderStyle().use_subpixel_positioning &&
          ascent >= 1) {
        ++descent;
        --ascent;
        // We should inflate overflow 1 more pixel for ascent instead.
        visual_overflow_inflation_for_descent = 0;
        ++(visual_overflow_inflation_for_ascent);
      }
#endif
    }
  }

#if defined(OS_MACOSX)
  // We are preserving this ascent hack to match Safari's ascent adjustment
  // in their SimpleFontDataMac.mm, for details see crbug.com/445830.
  // We need to adjust Times, Helvetica, and Courier to closely match the
  // vertical metrics of their Microsoft counterparts that are the de facto
  // web standard. The AppKit adjustment of 20% is too big and is
  // incorrectly added to line spacing, so we use a 15% adjustment instead
  // and add it to the ascent.
  String family_name = platform_data.FontFamilyName();
  if (family_name == font_family_names::kTimes ||
      family_name == font_family_names::kHelvetica ||
      family_name == font_family_names::kCourier)
    ascent += floorf(((ascent + descent) * 0.15f) + 0.5f);
#endif
}
}
