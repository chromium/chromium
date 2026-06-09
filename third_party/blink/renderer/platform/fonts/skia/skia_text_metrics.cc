// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/skia/skia_text_metrics.h"

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkStrikeRef.h"

namespace blink {

void SkFontGetGlyphWidthForHarfBuzz(const SkStrikeRef& strike_ref,
                                    bool subpixel,
                                    hb_codepoint_t codepoint,
                                    hb_position_t* width) {
  // We don't want to compute glyph extents for kUnmatchedVSGlyphId
  // cases yet. Since we will do that during the second shaping pass,
  // when VariationSelectorMode is set to kIgnoreVariationSelector.
  if (codepoint == kUnmatchedVSGlyphId) {
    return;
  }
  DCHECK_LE(codepoint, 0xFFFFu);
  CHECK(width);

  SkScalar sk_width = strike_ref.getWidth(static_cast<SkGlyphID>(codepoint));

  if (!subpixel) {
    sk_width = SkScalarRoundToInt(sk_width);
  }
  *width = SkiaScalarToHarfBuzzPosition(sk_width);
}

void SkFontGetGlyphWidthForHarfBuzz(const SkStrikeRef& strike_ref,
                                    bool subpixel,
                                    unsigned count,
                                    const hb_codepoint_t* glyphs,
                                    unsigned glyph_stride_32,
                                    hb_position_t* advances,
                                    unsigned advance_stride_32) {
  if (count == 0) {
    return;
  }
  CHECK(glyphs);
  CHECK(advances);

  static_assert(sizeof(SkScalar) == sizeof(hb_position_t),
                "SkScalar and hb_position_t must have the same size");
  SkScalar* advance = reinterpret_cast<float*>(advances);
  strike_ref.getWidthsStrided(count, glyphs, glyph_stride_32, advance,
                              advance_stride_32);

  SkScalar (*round_if_subpixel)(SkScalar) =
      subpixel ? [](SkScalar f) { return f; }
               : [](SkScalar f) { return SkScalarRoundToScalar(f); };

  // Perform in-place rounding and fixed-point conversion.
  // SAFETY: See HarfBuzzGetGlyphHorizontalAdvances in harfbuzz_face.cc:
  // We are interfacing with HarfBuzz and optimize this hot function in
  // text shaping. HarfBuzz provides the allocated output buffer and
  // stride information. The incoming count argument is provided by
  // HarfBuzz and ensure that enough output write buffer space is available
  // for the given stride.
  for (unsigned i = 0; i < count;
       i++, UNSAFE_BUFFERS(advance += advance_stride_32)) {
    *reinterpret_cast<hb_position_t*>(advance) =
        SkiaScalarToHarfBuzzPosition(round_if_subpixel(*advance));
  }
}

// HarfBuzz callback to retrieve glyph extents, mainly used by HarfBuzz for
// fallback mark positioning, i.e. the situation when the font does not have
// mark anchors or other mark positioning rules, but instead HarfBuzz is
// supposed to heuristically place combining marks around base glyphs. HarfBuzz
// does this by measuring "ink boxes" of glyphs, and placing them according to
// Unicode mark classes. Above, below, centered or left or right, etc.
void SkFontGetGlyphExtentsForHarfBuzz(const SkFont& font,
                                      hb_codepoint_t codepoint,
                                      hb_glyph_extents_t* extents) {
  // We don't want to compute glyph extents for kUnmatchedVSGlyphId
  // cases yet. Since we will do that during the second shaping pass,
  // when VariationSelectorMode is set to kIgnoreVariationSelector.
  if (codepoint == kUnmatchedVSGlyphId) {
    return;
  }
  DCHECK_LE(codepoint, 0xFFFFu);
  CHECK(extents);

  SkRect sk_bounds;
  uint16_t glyph = codepoint;

#if BUILDFLAG(IS_APPLE)
  // TODO(drott): Remove this once we have better metrics bounds
  // on Mac, https://bugs.chromium.org/p/skia/issues/detail?id=5328
  if (const auto path = font.getPath(glyph)) {
    sk_bounds = path->getBounds();
  } else {
    sk_bounds = font.getBounds(glyph, nullptr);
  }
#else
  sk_bounds = font.getBounds(glyph, nullptr);
#endif
  if (!font.isSubpixel()) {
    // Use roundOut() rather than round() to avoid rendering glyphs
    // outside the visual overflow rect. crbug.com/452914.
    sk_bounds.set(sk_bounds.roundOut());
  }

  // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be
  // y-grows-up.
  extents->x_bearing = SkiaScalarToHarfBuzzPosition(sk_bounds.fLeft);
  extents->y_bearing = SkiaScalarToHarfBuzzPosition(-sk_bounds.fTop);
  extents->width = SkiaScalarToHarfBuzzPosition(sk_bounds.width());
  extents->height = SkiaScalarToHarfBuzzPosition(-sk_bounds.height());
}

// Returns path-based bounds for float precision, falling back to
// font.getBounds() for bitmap-only glyphs (e.g. color emoji) that have no
// paths.
#if BUILDFLAG(IS_APPLE)
static void GetPathBoundsForGlyph(const SkFont& font,
                                  Glyph glyph,
                                  SkRect* bounds) {
  if (const auto path = font.getPath(glyph)) {
    *bounds = path->getBounds();
  } else {
    *bounds = font.getBounds(glyph, nullptr);
  }

  if (!font.isSubpixel()) {
    SkIRect ir;
    bounds->roundOut(&ir);
    bounds->set(ir);
  }
}
#endif

void SkFontGetBoundsForGlyph(const SkFont& font, Glyph glyph, SkRect* bounds) {
#if BUILDFLAG(IS_APPLE)
  // TODO(drott): Remove this once we have better metrics bounds
  // on Mac, https://bugs.chromium.org/p/skia/issues/detail?id=5328
  GetPathBoundsForGlyph(font, glyph, bounds);
#else
  *bounds = font.getBounds(glyph, nullptr);

  if (!font.isSubpixel()) {
    SkIRect ir;
    bounds->roundOut(&ir);
    bounds->set(ir);
  }
#endif
}

void SkFontGetPreciseBoundsForGlyph(const SkFont& font,
                                    Glyph glyph,
                                    SkRect* bounds) {
  // Use path-based bounds for float precision. SkGlyph stores bounds as
  // integers (int16_t/uint16_t) which causes large relative errors for small
  // font sizes (e.g. 1.5px). Path bounds give exact floating-point values
  // from the glyph outlines.
  //
  // Unlike GetPathBoundsForGlyph / SkFontGetBoundsForGlyph, we intentionally
  // do NOT round to integers for non-subpixel fonts, because the caller
  // (canvas TextMetrics) needs float-precision results.
  if (const auto path = font.getPath(glyph)) {
    *bounds = path->getBounds();
  } else {
    // Bitmap-only glyphs (e.g. color emoji) have no outlines.
    *bounds = font.getBounds(glyph, nullptr);
  }
}

void SkFontGetBoundsForGlyphs(const SkFont& font,
                              const Vector<Glyph, 256>& glyphs,
                              base::span<SkRect> bounds) {
#if BUILDFLAG(IS_APPLE)
  for (unsigned i = 0; i < glyphs.size(); i++) {
    SkFontGetBoundsForGlyph(font, glyphs[i], &bounds[i]);
  }
#else
  static_assert(sizeof(Glyph) == 2, "Skia expects 2 bytes glyph id.");
  font.getBounds(glyphs, {bounds.data(), glyphs.size()}, nullptr);

  if (!font.isSubpixel()) {
    for (unsigned i = 0; i < glyphs.size(); i++) {
      SkIRect ir;
      bounds[i].roundOut(&ir);
      bounds[i].set(ir);
    }
  }
#endif
}

float SkFontGetWidthForGlyph(const SkFont& font, Glyph glyph) {
  SkScalar sk_width = font.getWidth(glyph);

  if (!font.isSubpixel())
    sk_width = SkScalarRoundToInt(sk_width);

  return sk_width;
}

hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value) {
  // We treat HarfBuzz hb_position_t as 16.16 fixed-point.
  static const int kHbPosition1 = 1 << 16;
  return ClampTo<int>(value * kHbPosition1);
}

}  // namespace blink
