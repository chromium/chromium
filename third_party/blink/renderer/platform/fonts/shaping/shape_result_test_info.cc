// Copyright 2015 The Chromium Authors. All rights reserved.
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
  return runs_[run_index]->font_data_.get();
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

}  // namespace blink
