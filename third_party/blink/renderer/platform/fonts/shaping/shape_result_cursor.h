// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_CURSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_CURSOR_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

//
// This class keeps a pointer to a glyph in a `ShapeResult`, move it, and
// inspect or edit the glyph data.
//
// As each move involves function call overhead, this is appropriate when
// inspecting or editing only some of glyphs. Otherwise, member functions of
// `ShapeResult` such as `ForEachGlyph` are more appropriate.
//
class PLATFORM_EXPORT ShapeResultCursor {
  STACK_ALLOCATED();

 public:
  explicit ShapeResultCursor(ShapeResult* result) : result_(result) {
    MoveToStart();
  }

  explicit operator bool() const { return run_; }

  bool IsLtr() const { return result_->IsLtr(); }

  // Get the current character index.
  wtf_size_t CharacterIndex() const {
    return run_->start_index_ + GlyphData().character_index;
  }

  const SimpleFontData& FontData() const { return *run_->font_data_; }

  // Advance of the current cluster.
  TextRunLayoutUnit ClusterAdvance() const;

  // Move the current to the start. It is the first glyph if LTR, or the last
  // glyph if RTL.
  void MoveToStart();

  // Move forward to the specified character index. The current must be before
  // the specified character index.
  //
  // If the specified character index is missing, it stops at the next
  // character. Use `CharacterIndex()` to check this condition if needed.
  void MoveToCharacter(wtf_size_t character_index);

  // Add the space to the left/right side of the current cluster. On return,
  // the current is moved to the last glyph of the current cluster.
  void AddSpaceToLeft(TextRunLayoutUnit advance);
  void AddSpaceToRight(TextRunLayoutUnit advance);

  // Set the current glyph unsafe-to-break.
  void SetUnsafeToBreakBefore();

 private:
  FRIEND_TEST_ALL_PREFIXES(ShapeResultCursorTest, Ltr);
  FRIEND_TEST_ALL_PREFIXES(ShapeResultCursorTest, Rtl);
  FRIEND_TEST_ALL_PREFIXES(ShapeResultCursorTest, StartIndex);

  // Get `HarfBuzzRunGlyphData` for the current or the specified glyph index.
  const HarfBuzzRunGlyphData& GlyphData(wtf_size_t i) const {
    return run_->glyph_data_[i];
  }
  const HarfBuzzRunGlyphData& GlyphData() const {
    return GlyphData(glyph_index_);
  }
  HarfBuzzRunGlyphData& GlyphData(wtf_size_t i) { return run_->glyph_data_[i]; }
  HarfBuzzRunGlyphData& GlyphData() { return GlyphData(glyph_index_); }

  bool IsCluster(wtf_size_t i, wtf_size_t character_index) const {
    return i < run_->glyph_data_.size() &&
           GlyphData(i).character_index == character_index;
  }

  ShapeResult* result_;
  ShapeResultRun* run_;
  wtf_size_t run_index_ = 0;
  wtf_size_t glyph_index_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_CURSOR_H_
