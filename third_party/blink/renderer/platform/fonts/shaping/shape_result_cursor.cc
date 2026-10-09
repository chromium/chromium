// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_cursor.h"

#include <algorithm>

namespace blink {

void ShapeResultCursor::MoveToStart() {
  if (IsLtr()) {
    for (run_index_ = 0; run_index_ < result_->runs_.size(); ++run_index_) {
      run_ = result_->runs_[run_index_];
      if (run_) {
        glyph_index_ = 0;
        return;
      }
    }
  } else {  // RTL
    for (run_index_ = result_->runs_.size(); run_index_;) {
      run_ = result_->runs_[--run_index_];
      if (run_) {
        glyph_index_ = run_->NumGlyphs() - 1;
        return;
      }
    }
  }
  run_ = nullptr;
}

void ShapeResultCursor::MoveToCharacter(wtf_size_t character_index) {
  DCHECK(*this);
  if (IsLtr()) {
    for (;;) {
      DCHECK_GE(character_index, run_->start_index_);
      wtf_size_t character_index_in_run = character_index - run_->start_index_;
      if (character_index_in_run < run_->NumCharacters()) {
        if (run_->glyph_data_.IsCompact()) [[unlikely]] {
          // Identity indices collapse the scan below to an index.
          const wtf_size_t num_glyphs = run_->NumGlyphs();
          glyph_index_ = std::max(glyph_index_, character_index_in_run);
          if (glyph_index_ < num_glyphs) {
            return;
          }
          glyph_index_ = num_glyphs;
        } else {
          const auto& glyphs = run_->glyph_data_.NonCompactGlyphs();
          for (; glyph_index_ < glyphs.size(); ++glyph_index_) {
            if (glyphs[glyph_index_].character_index >=
                character_index_in_run) {
              return;
            }
          }
        }
      }
      if (++run_index_ >= result_->runs_.size()) {
        break;
      }
      run_ = result_->runs_[run_index_];
      glyph_index_ = 0;
    }
  } else {  // RTL
    for (;;) {
      DCHECK_GE(character_index, run_->start_index_);
      wtf_size_t character_index_in_run = character_index - run_->start_index_;
      if (character_index_in_run < run_->NumCharacters()) {
        const auto& glyphs = run_->glyph_data_.NonCompactGlyphs();
        for (;; --glyph_index_) {
          if (glyphs[glyph_index_].character_index >= character_index_in_run) {
            return;
          }
          if (!glyph_index_) {
            break;
          }
        }
      }
      if (!run_index_) {
        break;
      }
      run_ = result_->runs_[--run_index_];
      glyph_index_ = run_->NumGlyphs() - 1;
    }
  }
  run_ = nullptr;
}

TextRunLayoutUnit ShapeResultCursor::ClusterAdvance() const {
  DCHECK(*this);
  if (run_->glyph_data_.IsCompact()) [[unlikely]] {
    return run_->glyph_data_.CompactAdvance();
  }
  const auto& glyphs = run_->glyph_data_.NonCompactGlyphs();
  const HarfBuzzRunGlyphData& glyph_data = glyphs[glyph_index_];
  const wtf_size_t character_index = glyph_data.character_index;
  TextRunLayoutUnit advance = glyph_data.advance;
  for (wtf_size_t i = glyph_index_ + 1; i < glyphs.size(); ++i) {
    const HarfBuzzRunGlyphData& next_glyph_data = glyphs[i];
    if (next_glyph_data.character_index != character_index) {
      break;
    }
    advance += next_glyph_data.advance;
  }
  return advance;
}

void ShapeResultCursor::AddSpaceToRight(TextRunLayoutUnit advance) {
  DCHECK(*this);

  // Space of a cluster should be added to the last glyph of the cluster, so
  // that positions of glyphs in the cluster do not change.
  const wtf_size_t character_index = CharacterIndex();
  while (IsCluster(glyph_index_ + 1, character_index)) [[unlikely]] {
    ++glyph_index_;
  }
  MutableGlyphData().advance += advance;
  const float advance_float = advance.ToFloat();
  run_->width_ += advance_float;
  result_->width_ += advance_float;
}

void ShapeResultCursor::AddSpaceToLeft(TextRunLayoutUnit advance) {
  DCHECK(*this);

  // Adding to the left side of the cluster means all glyphs in the cluster need
  // to be moved, in addition to what `AddSpaceToRight` does.
  const float advance_float = advance.ToFloat();
  const wtf_size_t character_index = CharacterIndex();
  const bool is_horizontal = run_->IsHorizontal();
  for (;;) {
    if (is_horizontal) {
      run_->glyph_data_.AddOffsetWidthAt(glyph_index_, advance_float);
    } else {
      run_->glyph_data_.AddOffsetHeightAt(glyph_index_, advance_float);
      result_->has_vertical_offsets_ = true;
    }
    if (!IsCluster(glyph_index_ + 1, character_index)) {
      break;
    }
    ++glyph_index_;
  }
  MutableGlyphData().advance += advance;
  run_->width_ += advance_float;
  result_->width_ += advance_float;
}

void ShapeResultCursor::SetUnsafeToBreakBefore() {
  MutableGlyphData().SetSafeToBreakBefore(SafeToBreak::kUnsafe);
}

}  // namespace blink
