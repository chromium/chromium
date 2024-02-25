// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"

namespace blink {

unsigned ShapeResultTestInfo::NumberOfRunsForTesting() const {
  return runs_.size();
}

ShapeResult::RunInfo& ShapeResultTestInfo::RunInfoForTesting(
    unsigned run_index) const {
  return *runs_[run_index];
}

bool ShapeResultTestInfo::RunInfoForTesting(unsigned run_index,
                                            unsigned& start_index,
                                            unsigned& num_characters,
                                            unsigned& num_glyphs,
                                            hb_script_t& script) const {
  if (run_index < runs_.size() && runs_[run_index]) {
    start_index = runs_[run_index]->start_index_;
    num_characters = runs_[run_index]->num_characters_;
    num_glyphs = runs_[run_index]->glyph_data_.size();
    script = runs_[run_index]->script_;
    return true;
  }
  return false;
}

bool ShapeResultTestInfo::RunInfoForTesting(unsigned run_index,
                                            unsigned& start_index,
                                            unsigned& num_glyphs,
                                            hb_script_t& script) const {
  unsigned num_characters;
  return RunInfoForTesting(run_index, start_index, num_characters, num_glyphs,
                           script);
}

uint16_t ShapeResultTestInfo::GlyphForTesting(unsigned run_index,
                                              unsigned glyph_index) const {
  return runs_[run_index]->glyph_data_[glyph_index].glyph;
}

float ShapeResultTestInfo::AdvanceForTesting(unsigned run_index,
                                             unsigned glyph_index) const {
  return runs_[run_index]->glyph_data_[glyph_index].advance;
}

SimpleFontData* ShapeResultTestInfo::FontDataForTesting(
    unsigned run_index) const {
  return runs_[run_index]->font_data_.Get();
}

Vector<unsigned> ShapeResultTestInfo::CharacterIndexesForTesting() const {
  Vector<unsigned> character_indexes;
  for (const auto& run : runs_) {
    for (const auto& glyph_data : run->glyph_data_) {
      character_indexes.push_back(run->start_index_ +
                                  glyph_data.character_index);
    }
  }
  return character_indexes;
}

void AddGlyphInfo(void* context,
                  unsigned character_index,
                  Glyph glyph,
                  gfx::Vector2dF glyph_offset,
                  float advance,
                  bool is_horizontal,
                  CanvasRotationInVertical rotation,
                  const SimpleFontData* font_data) {
  auto* list = static_cast<Vector<ShapeResultTestGlyphInfo>*>(context);
  ShapeResultTestGlyphInfo glyph_info = {character_index, glyph, advance};
  list->push_back(glyph_info);
}

void ComputeGlyphResults(const ShapeResult& result,
                         Vector<ShapeResultTestGlyphInfo>* glyphs) {
  result.ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(glyphs));
}

bool CompareResultGlyphs(const Vector<ShapeResultTestGlyphInfo>& test,
                         const Vector<ShapeResultTestGlyphInfo>& reference,
                         unsigned reference_start,
                         unsigned num_glyphs) {
  float advance_offset = reference[reference_start].advance;
  bool glyphs_match = true;
  for (unsigned i = 0; i < test.size(); i++) {
    const auto& test_glyph = test[i];
    const auto& reference_glyph = reference[i + reference_start];
    if (test_glyph.character_index != reference_glyph.character_index ||
        test_glyph.glyph != reference_glyph.glyph ||
        test_glyph.advance != reference_glyph.advance - advance_offset) {
      glyphs_match = false;
      break;
    }
  }
  if (!glyphs_match) {
    fprintf(stderr, "╔══ Actual ═══════╤═══════╤═════════╗    ");
    fprintf(stderr, "╔══ Expected ═════╤═══════╤═════════╗\n");
    fprintf(stderr, "║ Character Index │ Glyph │ Advance ║    ");
    fprintf(stderr, "║ Character Index │ Glyph │ Advance ║\n");
    fprintf(stderr, "╟─────────────────┼───────┼─────────╢    ");
    fprintf(stderr, "╟─────────────────┼───────┼─────────╢\n");
    for (unsigned i = 0; i < test.size(); i++) {
      const auto& test_glyph = test[i];
      const auto& reference_glyph = reference[i + reference_start];

      if (test_glyph.character_index == reference_glyph.character_index)
        fprintf(stderr, "║      %10u │", test_glyph.character_index);
      else
        fprintf(stderr, "║▶     %10u◀│", test_glyph.character_index);

      if (test_glyph.glyph == reference_glyph.glyph)
        fprintf(stderr, "  %04X │", test_glyph.glyph);
      else
        fprintf(stderr, "▶ %04X◀│", test_glyph.glyph);

      if (test_glyph.advance == reference_glyph.advance)
        fprintf(stderr, " %7.2f ║    ", test_glyph.advance);
      else
        fprintf(stderr, "▶%7.2f◀║    ", test_glyph.advance);

      fprintf(stderr, "║      %10u │  %04X │ %7.2f ║\n",
              reference_glyph.character_index, reference_glyph.glyph,
              reference_glyph.advance - advance_offset);
    }
    fprintf(stderr, "╚═════════════════╧═══════╧═════════╝    ");
    fprintf(stderr, "╚═════════════════╧═══════╧═════════╝\n");
  }
  return glyphs_match;
}

}  // namespace blink
