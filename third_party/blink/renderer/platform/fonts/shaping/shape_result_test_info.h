// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_TEST_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_TEST_INFO_H_

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <hb.h>

namespace blink {

class PLATFORM_EXPORT ShapeResultTestInfo : public ShapeResult {
 public:
  wtf_size_t NumberOfRunsForTesting() const;
  ShapeResultRun& RunInfoForTesting(wtf_size_t run_index) const;
  bool RunInfoForTesting(wtf_size_t run_index,
                         wtf_size_t& start_index,
                         wtf_size_t& num_glyphs,
                         hb_script_t&) const;
  bool RunInfoForTesting(wtf_size_t run_index,
                         wtf_size_t& start_index,
                         wtf_size_t& num_characters,
                         wtf_size_t& num_glyphs,
                         hb_script_t&) const;
  uint16_t GlyphForTesting(wtf_size_t run_index, wtf_size_t glyph_index) const;
  float AdvanceForTesting(wtf_size_t run_index, wtf_size_t glyph_index) const;
  SimpleFontData* FontDataForTesting(wtf_size_t run_index) const;
  Vector<wtf_size_t> CharacterIndexesForTesting() const;
};

class PLATFORM_EXPORT ShapeResultBloberizerTestInfo {
  STATIC_ONLY(ShapeResultBloberizerTestInfo);

 public:
  static void Add(ShapeResultBloberizer& bloberizer,
                  Glyph glyph,
                  const SimpleFontData* font_data,
                  CanvasRotationInVertical canvas_rotation,
                  float h_offset,
                  wtf_size_t character_index) {
    bloberizer.Add(glyph, font_data, canvas_rotation, h_offset,
                   character_index);
  }

  static void Add(ShapeResultBloberizer& bloberizer,
                  Glyph glyph,
                  const SimpleFontData* font_data,
                  CanvasRotationInVertical canvas_rotation,
                  const gfx::Vector2dF& offset,
                  wtf_size_t character_index) {
    bloberizer.Add(glyph, font_data, canvas_rotation, offset, character_index);
  }

  static const SimpleFontData* PendingRunFontData(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_font_data_;
  }

  static CanvasRotationInVertical PendingBlobRotation(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_canvas_rotation_;
  }

  static const Vector<Glyph, 1024>& PendingRunGlyphs(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_glyphs_;
  }

  static const Vector<float, 1024>& PendingRunOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_offsets_;
  }

  static bool HasPendingRunVerticalOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.HasPendingVerticalOffsets();
  }

  static size_t PendingBlobRunCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.builder_run_count_;
  }

  static size_t CommittedBlobCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.blobs_.size();
  }
};

struct PLATFORM_EXPORT ShapeResultTestGlyphInfo {
  wtf_size_t character_index;
  Glyph glyph;
  float advance;
};

void PLATFORM_EXPORT AddGlyphInfo(void* context,
                                  wtf_size_t character_index,
                                  Glyph,
                                  gfx::Vector2dF glyph_offset,
                                  float advance,
                                  bool is_horizontal,
                                  CanvasRotationInVertical,
                                  const SimpleFontData*);

void PLATFORM_EXPORT ComputeGlyphResults(const ShapeResult&,
                                         Vector<ShapeResultTestGlyphInfo>*);

bool PLATFORM_EXPORT
CompareResultGlyphs(const Vector<ShapeResultTestGlyphInfo>& test,
                    const Vector<ShapeResultTestGlyphInfo>& reference,
                    wtf_size_t reference_start,
                    wtf_size_t num_glyphs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_TEST_INFO_H_
