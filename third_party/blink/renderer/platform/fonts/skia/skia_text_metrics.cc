// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/skia/skia_text_metrics.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPath.h"

namespace blink {

namespace {

template <class T>
T* advance_by_byte_size(T* p, unsigned byte_size) {
  return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(p) + byte_size);
}

template <class T>
const T* advance_by_byte_size(const T* p, unsigned byte_size) {
  return reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(p) +
                                    byte_size);
}

}  // namespace

void SkFontGetGlyphWidthForHarfBuzz(const SkFont& font,
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

  SkScalar sk_width;
  uint16_t glyph = codepoint;

  font.getWidths(&glyph, 1, &sk_width);
  if (!font.isSubpixel())
    sk_width = SkScalarRoundToInt(sk_width);
  *width = SkiaScalarToHarfBuzzPosition(sk_width);
}

void SkFontGetGlyphWidthForHarfBuzz(const SkFont& font,
                                    unsigned count,
                                    const hb_codepoint_t* glyphs,
                                    const unsigned glyph_stride,
                                    hb_position_t* advances,
                                    unsigned advance_stride) {
  // Batch the call to getWidths because its function entry cost is not
  // cheap. getWidths accepts multiple glyphd ID, but not from a sparse
  // array that copy them to a regular array.
  Vector<Glyph, 256> glyph_array(count);
  for (unsigned i = 0; i < count;
       i++, glyphs = advance_by_byte_size(glyphs, glyph_stride)) {
    glyph_array[i] = *glyphs;
  }
  Vector<SkScalar, 256> sk_width_array(count);
  font.getWidths(glyph_array.data(), count, sk_width_array.data());

  if (!font.isSubpixel()) {
    for (unsigned i = 0; i < count; i++)
      sk_width_array[i] = SkScalarRoundToInt(sk_width_array[i]);
  }

  // Copy the results back to the sparse array.
  for (unsigned i = 0; i < count;
       i++, advances = advance_by_byte_size(advances, advance_stride)) {
    *advances = SkiaScalarToHarfBuzzPosition(sk_width_array[i]);
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
  SkPath path;
  if (font.getPath(glyph, &path)) {
    sk_bounds = path.getBounds();
  } else {
    font.getBounds(&glyph, 1, &sk_bounds, nullptr);
  }
#else
  font.getBounds(&glyph, 1, &sk_bounds, nullptr);
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

void SkFontGetBoundsForGlyph(const SkFont& font, Glyph glyph, SkRect* bounds) {
#if BUILDFLAG(IS_APPLE)
  // TODO(drott): Remove this once we have better metrics bounds
  // on Mac, https://bugs.chromium.org/p/skia/issues/detail?id=5328
  SkPath path;
  if (font.getPath(glyph, &path)) {
    *bounds = path.getBounds();
  } else {
    // Fonts like Apple Color Emoji have no paths, fall back to bounds here.
    font.getBounds(&glyph, 1, bounds, nullptr);
  }
#else
  font.getBounds(&glyph, 1, bounds, nullptr);
#endif

  if (!font.isSubpixel()) {
    SkIRect ir;
    bounds->roundOut(&ir);
    bounds->set(ir);
  }
}

void SkFontGetBoundsForGlyphs(const SkFont& font,
                              const Vector<Glyph, 256>& glyphs,
                              SkRect* bounds) {
#if BUILDFLAG(IS_APPLE)
  for (unsigned i = 0; i < glyphs.size(); i++) {
    SkFontGetBoundsForGlyph(font, glyphs[i], &bounds[i]);
  }
#else
  static_assert(sizeof(Glyph) == 2, "Skia expects 2 bytes glyph id.");
  font.getBounds(glyphs.data(), glyphs.size(), bounds, nullptr);

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
  SkScalar sk_width;
  font.getWidths(&glyph, 1, &sk_width);

  if (!font.isSubpixel())
    sk_width = SkScalarRoundToInt(sk_width);

  return SkScalarToFloat(sk_width);
}

hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value) {
  // We treat HarfBuzz hb_position_t as 16.16 fixed-point.
  static const int kHbPosition1 = 1 << 16;
  return ClampTo<int>(value * kHbPosition1);
}

}  // namespace blink
