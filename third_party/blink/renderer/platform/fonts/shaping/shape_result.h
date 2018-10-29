/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_

#include <memory>
#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

struct hb_buffer_t;

namespace blink {

struct CharacterRange;
class Font;
template <typename TextContainerType>
class PLATFORM_EXPORT ShapeResultSpacing;
class SimpleFontData;
class TextRun;

enum class AdjustMidCluster {
  // Adjust the middle of a grapheme cluster to the logical end boundary.
  kToEnd,
  // Adjust the middle of a grapheme cluster to the logical start boundary.
  kToStart
};

struct ShapeResultCharacterData {
  DISALLOW_NEW();
  float x_position;
  // Set for the logical first character of a cluster.
  unsigned is_cluster_base : 1;
  unsigned safe_to_break_before : 1;
};

// There are two options for how OffsetForPosition behaves:
// IncludePartialGlyphs - decides what to do when the position hits more than
// 50% of the glyph. If enabled, we count that glyph, if disable we don't.
enum IncludePartialGlyphsOption {
  OnlyFullGlyphs,
  IncludePartialGlyphs,
};

// BreakGlyphs - allows OffsetForPosition to consider graphemes separations
// inside a glyph. It allows the function to return a point inside a glyph when
// multiple graphemes share a glyph (for example, in a ligature)
enum BreakGlyphsOption {
  DontBreakGlyphs,
  BreakGlyphs,
};

// std::function is forbidden in Chromium and base::Callback is way too
// expensive so we resort to a good old function pointer instead.
typedef void (*GlyphCallback)(void* context,
                              unsigned character_index,
                              Glyph,
                              FloatSize glyph_offset,
                              float total_advance,
                              bool is_horizontal,
                              CanvasRotationInVertical,
                              const SimpleFontData*);

typedef void (*GraphemeClusterCallback)(void* context,
                                        unsigned character_index,
                                        float total_advance,
                                        unsigned graphemes_in_cluster,
                                        float cluster_advance,
                                        CanvasRotationInVertical);

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
 public:
  static scoped_refptr<ShapeResult> Create(const Font* font,
                                    unsigned num_characters,
                                    TextDirection direction) {
    return base::AdoptRef(new ShapeResult(font, num_characters, direction));
  }
  static scoped_refptr<ShapeResult> Create(const ShapeResult& other) {
    return base::AdoptRef(new ShapeResult(other));
  }
  static scoped_refptr<ShapeResult> CreateForTabulationCharacters(
      const Font*,
      const TextRun&,
      float position_offset,
      unsigned count);
  ~ShapeResult();

  // Returns a mutable unique instance. If |this| has more than 1 ref count,
  // a clone is created.
  scoped_refptr<ShapeResult> MutableUnique() const;

  // The logical width of this result.
  float Width() const { return width_; }
  LayoutUnit SnappedWidth() const { return LayoutUnit::FromFloatCeil(width_); }
  // The glyph bounding box, in logical coordinates, using alphabetic baseline
  // even when the result is in vertical flow.
  const FloatRect& Bounds() const { return glyph_bounding_box_; }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return num_glyphs_; }
  CharacterRange GetCharacterRange(const StringView& text,
                                   unsigned from,
                                   unsigned to) const;
  // TODO(eae): Remove start_x and return value once ShapeResultBuffer has been
  // removed.
  float IndividualCharacterRanges(Vector<CharacterRange>* ranges,
                                  float start_x = 0) const;

  // The character start/end index of a range shape result.
  unsigned StartIndexForResult() const { return start_index_; }
  unsigned EndIndexForResult() const { return start_index_ + num_characters_; }
  void FallbackFonts(HashSet<const SimpleFontData*>*) const;
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool Rtl() const { return Direction() == TextDirection::kRtl; }

  // True if at least one glyph in this result has vertical offsets.
  //
  // Vertical result always has vertical offsets, but horizontal result may also
  // have vertical offsets.
  bool HasVerticalOffsets() const { return has_vertical_offsets_; }

  // For memory reporting.
  size_t ByteSize() const;

  // Returns the next or previous offsets respectively at which it is safe to
  // break without reshaping.
  // The |offset| given and the return value is for the original string, between
  // |StartIndexForResult| and |EndIndexForResult|.
  // TODO(eae): Remove these ones the cached versions are used everywhere.
  unsigned NextSafeToBreakOffset(unsigned offset) const;
  unsigned PreviousSafeToBreakOffset(unsigned offset) const;

  // Returns the offset, relative to StartIndexForResult, whose (origin,
  // origin+advance) contains |x|.
  unsigned OffsetForPosition(float x, BreakGlyphsOption) const;
  // Returns the offset whose glyph boundary is nearest to |x|. Depends on
  // whether |x| is on the left-half or the right-half of the glyph, it
  // determines the left-boundary or the right-boundary, then computes the
  // offset from the bidi direction.
  unsigned CaretOffsetForHitTest(float x,
                                 const StringView& text,
                                 BreakGlyphsOption) const;
  // Returns the offset that can fit to between |x| and the left or the right
  // edge. The side of the edge is determined by |line_direction|.
  unsigned OffsetToFit(float x, TextDirection line_direction) const;
  unsigned OffsetForPosition(float x,
                             const StringView& text,
                             IncludePartialGlyphsOption include_partial_glyphs,
                             BreakGlyphsOption break_glyphs_option) const {
    if (include_partial_glyphs == OnlyFullGlyphs) {
      // TODO(kojii): Consider prohibiting OnlyFullGlyphs+BreakGlyphs, used only
      // in tests.
      if (break_glyphs_option == BreakGlyphs)
        EnsureGraphemes(text);
      return OffsetForPosition(x, break_glyphs_option);
    }
    return CaretOffsetForHitTest(x, text, break_glyphs_option);
  }

  // Returns the position for a given offset, relative to StartIndexForResult.
  float PositionForOffset(unsigned offset,
                          AdjustMidCluster = AdjustMidCluster::kToEnd) const;
  // Similar to |PositionForOffset| with mid-glyph (mid-ligature) support.
  float CaretPositionForOffset(
      unsigned offset,
      const StringView& text,
      AdjustMidCluster = AdjustMidCluster::kToEnd) const;
  LayoutUnit SnappedStartPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatFloor(PositionForOffset(offset));
  }
  LayoutUnit SnappedEndPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatCeil(PositionForOffset(offset));
  }

  // Computes and caches a position data object as needed.
  void EnsurePositionData() const;

  // Fast versions of OffsetForPosition and PositionForOffset that operates on
  // a cache (that needs to be pre-computed using EnsurePositionData) and that
  // does not take partial glyphs into account.
  unsigned CachedOffsetForPosition(float x) const;
  float CachedPositionForOffset(unsigned offset) const;

  // Returns the next or previous offsets respectively at which it is safe to
  // break without reshaping. Operates on a cache (that needs to be pre-computed
  // using EnsurePositionData) and does not take partial glyphs into account.
  // The |offset| given and the return value is for the original string, between
  // |StartIndexForResult| and |EndIndexForResult|.
  unsigned CachedNextSafeToBreakOffset(unsigned offset) const;
  unsigned CachedPreviousSafeToBreakOffset(unsigned offset) const;

  // Apply spacings (letter-spacing, word-spacing, and justification) as
  // configured to |ShapeResultSpacing|.
  // |text_start_offset| adjusts the character index in the ShapeResult before
  // giving it to |ShapeResultSpacing|. It can be negative if
  // |StartIndexForResult()| is larger than the text in |ShapeResultSpacing|.
  void ApplySpacing(ShapeResultSpacing<String>&, int text_start_offset = 0);
  scoped_refptr<ShapeResult> ApplySpacingToCopy(ShapeResultSpacing<TextRun>&,
                                         const TextRun&) const;

  // Append a copy of a range within an existing result to another result.
  void CopyRange(unsigned start, unsigned end, ShapeResult*) const;

  // Create a new ShapeResult instance from a range within an existing result.
  scoped_refptr<ShapeResult> SubRange(unsigned start_offset,
                                      unsigned end_offset) const;

  // Create a new ShapeResult instance with the start offset adjusted.
  scoped_refptr<ShapeResult> CopyAdjustedOffset(unsigned start_offset) const;

  // Computes the list of fonts along with the number of glyphs for each font.
  struct RunFontData {
    SimpleFontData* font_data_;
    wtf_size_t glyph_count_;
  };
  void GetRunFontData(Vector<RunFontData>* font_data) const;

  // Iterates over, and calls the specified callback function, for all the
  // glyphs. Also tracks (and returns) a seeded total advance.
  // The second version of the method only invokes the callback for glyphs in
  // the specified range and stops after the range.
  // The context parameter will be given as the first parameter for the callback
  // function.
  //
  // TODO(eae): Remove the initial_advance and index_offset parameters once
  // ShapeResultBuffer has been removed as they're only used in cases where
  // multiple ShapeResult are combined in a ShapeResultBuffer.
  float ForEachGlyph(float initial_advance, GlyphCallback, void* context) const;
  float ForEachGlyph(float initial_advance,
                     unsigned from,
                     unsigned to,
                     unsigned index_offset,
                     GlyphCallback,
                     void* context) const;

  // Iterates over, and calls the specified callback function, for all the
  // grapheme clusters. As ShapeResuls do not contain the original text content
  // a StringView with the text must be supplied and must match the text that
  // was used generate the ShapeResult.
  // Also tracks (and returns) a seeded total advance.
  // The context parameter will be given as the first parameter for the callback
  // function.
  float ForEachGraphemeClusters(const StringView& text,
                                float initial_advance,
                                unsigned from,
                                unsigned to,
                                unsigned index_offset,
                                GraphemeClusterCallback,
                                void* context) const;

  String ToString() const;
  void ToString(StringBuilder*) const;

  struct RunInfo;
  RunInfo* InsertRunForTesting(unsigned start_index,
                               unsigned num_characters,
                               TextDirection,
                               Vector<uint16_t> safe_break_offsets = {});
#if DCHECK_IS_ON()
  void CheckConsistency() const;
#endif

 protected:
  ShapeResult(const SimpleFontData*, unsigned num_characters, TextDirection);
  ShapeResult(const Font*, unsigned num_characters, TextDirection);
  ShapeResult(const ShapeResult&);

  static scoped_refptr<ShapeResult> Create(const SimpleFontData* font_data,
                                           unsigned num_characters,
                                           TextDirection direction) {
    return base::AdoptRef(
        new ShapeResult(font_data, num_characters, direction));
  }

  // Ensure |grapheme_| is computed. |BreakGlyphs| is valid only when
  // |grapheme_| is computed.
  void EnsureGraphemes(const StringView& text) const;

  struct GlyphIndexResult {
    STACK_ALLOCATED();

   public:
    unsigned run_index = 0;
    // The total number of characters of runs_[0..run_index - 1].
    unsigned characters_on_left_runs = 0;

    // Those are the left and right character indexes of the group of glyphs
    // that were selected by OffsetForPosition.
    unsigned left_character_index = 0;
    unsigned right_character_index = 0;

    // The glyph origin of the glyph.
    float origin_x = 0;
    // The advance of the glyph.
    float advance = 0;
  };

  void OffsetForPosition(float target_x,
                         BreakGlyphsOption,
                         GlyphIndexResult*) const;

  // Helper class storing a map between offsets and x-positions.
  // Unlike the RunInfo and GlyphData structures in ShapeResult, which operates
  // in glyph order, this class stores a map between character index and the
  // total accumulated advance for each character. Allowing constant time
  // mapping from character index to x-position and O(log n) time, using binary
  // search, from x-position to character index.
  class CharacterPositionData {
    USING_FAST_MALLOC(CharacterPositionData);

   public:
    CharacterPositionData(unsigned num_characters, float width)
        : data_(num_characters), width_(width) {}

    // Returns the next or previous offsets respectively at which it is safe to
    // break without reshaping.
    unsigned NextSafeToBreakOffset(unsigned offset) const;
    unsigned PreviousSafeToBreakOffset(unsigned offset) const;

    // Returns the offset of the last character that fully fits before the given
    // x-position.
    unsigned OffsetForPosition(float x, bool rtl) const;

    // Returns the x-position for a given offset.
    float PositionForOffset(unsigned offset, bool rtl) const;

   private:
    // This vector is indexed by visual-offset; the character offset from the
    // left edge regardless of the TextDirection.
    Vector<ShapeResultCharacterData> data_;
    unsigned start_offset_;
    float width_;

    friend class ShapeResult;
  };

  template <bool>
  void ComputePositionData() const;

  template <typename TextContainerType>
  void ApplySpacingImpl(ShapeResultSpacing<TextContainerType>&,
                        int text_start_offset = 0);
  template <bool is_horizontal_run>
  void ComputeGlyphPositions(ShapeResult::RunInfo*,
                             unsigned start_glyph,
                             unsigned num_glyphs,
                             hb_buffer_t*);
  template <bool is_horizontal_run>
  void ComputeGlyphBounds(const ShapeResult::RunInfo&);
  void InsertRun(std::unique_ptr<ShapeResult::RunInfo>,
                 unsigned start_glyph,
                 unsigned num_glyphs,
                 hb_buffer_t*);
  void InsertRun(std::unique_ptr<ShapeResult::RunInfo>);
  void InsertRunForIndex(unsigned start_character_index);
  void ReorderRtlRuns(unsigned run_size_before);
  unsigned ComputeStartIndex() const;
  void UpdateStartIndex();

  float LineLeftBounds() const;
  float LineRightBounds() const;

  float width_;
  FloatRect glyph_bounding_box_;
  Vector<std::unique_ptr<RunInfo>> runs_;
  scoped_refptr<const SimpleFontData> primary_font_;
  mutable std::unique_ptr<CharacterPositionData> character_position_;

  unsigned start_index_;
  unsigned num_characters_;
  unsigned num_glyphs_ : 30;

  // Overall direction for the TextRun, dictates which order each individual
  // sub run (represented by RunInfo structs in the m_runs vector) can have a
  // different text direction.
  unsigned direction_ : 1;

  // Tracks whether any runs contain glyphs with a y-offset != 0.
  unsigned has_vertical_offsets_ : 1;

  friend class HarfBuzzShaper;
  friend class ShapeResultBuffer;
  friend class ShapeResultBloberizer;
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const ShapeResult&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_
