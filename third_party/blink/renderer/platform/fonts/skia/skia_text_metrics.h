// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKIA_TEXT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKIA_TEXT_METRICS_H_

#include <hb.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRect.h"

class SkFont;
class SkStrikeRef;

namespace blink {

// TODO: Width functions are affected by issue
// https://bugs.chromium.org/p/skia/issues/detail?id=10123 in Skia, which
// currently does not return trak-free advances on Mac OS 10.15.

void SkFontGetGlyphWidthForHarfBuzz(const SkStrikeRef&,
                                    bool subpixel,
                                    hb_codepoint_t,
                                    hb_position_t* width);

/** Retrieves the advance widths for each glyph, handling arbitrary strides for
    both input glyphs and output advances. Strided access ensures a fast
    transfer of glyph advances in the hot text shaping code path.

    @param count              number of glyphs to measure
    @param first_glyph        pointer to the first glyph ID
    @param glyph_stride_32    stride in 32-bit words between input glyph IDs
    @param first_advance      pointer to the first output advance
    @param advance_stride_32  stride in 32-bit words between output advances
*/
void SkFontGetGlyphWidthForHarfBuzz(const SkStrikeRef&,
                                    bool subpixel,
                                    unsigned count,
                                    const hb_codepoint_t* first_glyph,
                                    unsigned glyph_stride_32,
                                    hb_position_t* first_advance,
                                    unsigned advance_stride_32);

void SkFontGetGlyphExtentsForHarfBuzz(const SkFont&,
                                      hb_codepoint_t,
                                      hb_glyph_extents_t*);

void SkFontGetBoundsForGlyph(const SkFont&, Glyph, SkRect* bounds);
void SkFontGetPreciseBoundsForGlyph(const SkFont&, Glyph, SkRect* bounds);
void SkFontGetBoundsForGlyphs(const SkFont&,
                              const Vector<Glyph, 256>&,
                              base::span<SkRect>);
float SkFontGetWidthForGlyph(const SkFont&, Glyph);

hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SKIA_SKIA_TEXT_METRICS_H_
