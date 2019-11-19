// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_VIEW_H_

#include <memory>
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ShapeResult;

// Class representing a read-only composite of views into one or more existing
// shape results.
// Implemented as a list of ref counted RunInfo instances and a start/end
// offset for each, represented using the internal RunInfoPart struct.
// This allows lines to be reference sections of the overall paragraph shape
// results without the memory or computational overhead of a copy.
//
// The example below shows the shape result and the individual lines as
// ShapeResultView instances pointing to the original paragraph results for
// the string "Pack my box with five dozen liquor jugs.":
//  ╔═════════════════════════════════════════════════════╗
//  ║ Paragraph with single run, no re-shaping for lines. ║
//  ╟─────────────────────────────────────────────────────╢
//  ║ runs_ ╭───────────────────────────────────────────╮ ║
//  ║   1:  │ Pack my box with five dozen liquor jugs.  │ ║
//  ║       ╰───────────────────────────────────────────╯ ║
//  ║ lines ╭───────────────────────────────────────────╮ ║
//  ║   1:  │ Pack my box with    -> view, run 1:  0-16 │ ║
//  ║   2:  │ five dozen liquor   -> view, run 1: 17-34 │ ║
//  ║   3:  │ jugs.               -> view, run 1: 35-40 │ ║
//  ║       ╰───────────────────────────────────────────╯ ║
//  ╚═════════════════════════════════════════════════════╝
//
// In cases where a portion of the line needs re-shaping the new results are
// added as separate runs at the beginning and/or end of the runs_ vector with a
// reference to zero or more sub-runs in the middle representing the original
// content that could be reused.
//
// In the example below the end of the first line "Jack!" needs to be re-shaped:
//  ╔═════════════════════════════════════════════════════╗
//  ║ Paragraph with single run, requiring re-shape.      ║
//  ╟─────────────────────────────────────────────────────╢
//  ║ runs_ ╭───────────────────────────────────────────╮ ║
//  ║   1:  │ "Now fax quiz Jack!" my brave ghost pled. │ ║
//  ║       ╰───────────────────────────────────────────╯ ║
//  ║ lines ╭───────────────────────────────────────────╮ ║
//  ║   1:  │ "Now fax quiz     -> view, run 1:  0-14   │ ║
//  ║   1:  │ Jack!             -> new result/run       │ ║
//  ║   2:  │ my brave ghost    -> view, run 1: 21-35   │ ║
//  ║   3:  │ pled.             -> view, run 1: 41-36   │ ║
//  ║       ╰───────────────────────────────────────────╯ ║
//  ╚═════════════════════════════════════════════════════╝
//
// In this case the beginning of the first line would be represented as a part
// referecing the a range into the original ShapeResult while the last word wold
// be a separate result owned by the ShapeResultView instance. The second
// and third lines would again be represented as parts.
class PLATFORM_EXPORT ShapeResultView final
    : public RefCounted<ShapeResultView> {
 public:
  // Create a new ShapeResultView from a pre-defined list of segments.
  // The segments list is assumed to be in logical order.
  struct Segment {
    Segment() = default;
    Segment(const ShapeResult* result, unsigned start_index, unsigned end_index)
        : result(result),
          view(nullptr),
          start_index(start_index),
          end_index(end_index) {}
    Segment(const ShapeResultView* view,
            unsigned start_index,
            unsigned end_index)
        : result(nullptr),
          view(view),
          start_index(start_index),
          end_index(end_index) {}
    const ShapeResult* result;
    const ShapeResultView* view;
    unsigned start_index;
    unsigned end_index;
  };
  static scoped_refptr<ShapeResultView> Create(const Segment*, size_t);

  // Creates a new ShapeResultView from a single segment.
  static scoped_refptr<ShapeResultView> Create(const ShapeResult*);
  static scoped_refptr<ShapeResultView> Create(const ShapeResult*,
                                               unsigned start_index,
                                               unsigned end_index);
  static scoped_refptr<ShapeResultView> Create(const ShapeResultView*,
                                               unsigned start_index,
                                               unsigned end_index);

  ~ShapeResultView();

  scoped_refptr<ShapeResult> CreateShapeResult() const;

  unsigned StartIndex() const { return start_index_ + char_index_offset_; }
  unsigned EndIndex() const { return StartIndex() + num_characters_; }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return num_glyphs_; }
  float Width() const { return width_; }
  LayoutUnit SnappedWidth() const { return LayoutUnit::FromFloatCeil(width_); }
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool Rtl() const { return Direction() == TextDirection::kRtl; }
  bool HasVerticalOffsets() const { return has_vertical_offsets_; }
  void FallbackFonts(HashSet<const SimpleFontData*>* fallback) const;

  unsigned PreviousSafeToBreakOffset(unsigned index) const;

  float ForEachGlyph(float initial_advance, GlyphCallback, void* context) const;
  float ForEachGlyph(float initial_advance,
                     unsigned from,
                     unsigned to,
                     unsigned index_offset,
                     GlyphCallback,
                     void* context) const;

  float ForEachGraphemeClusters(const StringView& text,
                                float initial_advance,
                                unsigned from,
                                unsigned to,
                                unsigned index_offset,
                                GraphemeClusterCallback,
                                void* context) const;

  // Computes and returns the ink bounds (or visual overflow rect). This is
  // quite expensive and involves measuring each glyph accumulating the bounds.
  FloatRect ComputeInkBounds() const;

  scoped_refptr<const SimpleFontData> PrimaryFont() const {
    return primary_font_;
  }
  void GetRunFontData(Vector<ShapeResult::RunFontData>*) const;

 private:
  template <class ShapeResultType>
  ShapeResultView(const ShapeResultType*);

  struct RunInfoPart;
  template <class ShapeResultType>
  void CreateViewsForResult(const ShapeResultType*,
                            unsigned start_index,
                            unsigned end_index);
  void AddSegments(const Segment*, size_t);

  unsigned CharacterIndexOffsetForGlyphData(const RunInfoPart&) const;

  template <bool is_horizontal_run, bool has_glyph_offsets>
  void ComputePartInkBounds(const ShapeResultView::RunInfoPart&,
                            float run_advance,
                            FloatRect* ink_bounds) const;

  // Common signatures with ShapeResult, to templatize algorithms.
  base::span<const RunInfoPart> RunsOrParts() const { return Parts(); }

  base::span<RunInfoPart> Parts() {
    return {reinterpret_cast<RunInfoPart*>(parts_), num_parts_};
  }
  base::span<const RunInfoPart> Parts() const {
    return {reinterpret_cast<const RunInfoPart*>(parts_), num_parts_};
  }
  unsigned StartIndexOffsetForRun() const { return char_index_offset_; }

  scoped_refptr<const SimpleFontData> primary_font_;

  unsigned start_index_;
  unsigned num_characters_;
  unsigned num_glyphs_ : 30;

  // Overall direction for the TextRun, dictates which order each individual
  // sub run (represented by RunInfo structs in the m_runs vector) can
  // have a different text direction.
  unsigned direction_ : 1;

  // Tracks whether any runs contain glyphs with a y-offset != 0.
  unsigned has_vertical_offsets_ : 1;

  // Offset of the first component added to the view. Used for compatibility
  // with ShapeResult::SubRange
  unsigned char_index_offset_;

  float width_;
  wtf_size_t num_parts_ = 0;

  // TODO(yosin): We should declare |RunInoPart| in this file to avoid using
  // dummy struct.
  // Note: To avoid declaring |RunInfoPart| here, we use dummy struct.
  struct {
    void* alignment;
  } parts_[];

 private:
  friend class ShapeResult;

  template <bool has_glyph_offsets>
  float ForEachGlyphImpl(float initial_advance,
                         GlyphCallback,
                         void* context,
                         const RunInfoPart& part) const;

  template <bool has_glyph_offsets>
  float ForEachGlyphImpl(float initial_advance,
                         unsigned from,
                         unsigned to,
                         unsigned index_offset,
                         GlyphCallback,
                         void* context,
                         const RunInfoPart& part) const;

  DISALLOW_COPY_AND_ASSIGN(ShapeResultView);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_VIEW_H_
