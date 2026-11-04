// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/glyph_run.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class DOMRectReadOnly;
class ExceptionState;
class PlainTextPainter;

// Shaping state and shaping-based queries for a single-style text string,
// shaped with the canvas text drawing styles captured at shaping time. Returned
// by CanvasRenderingContext2D::shapeText(), and backs measureText() through
// TextMetrics.
class CORE_EXPORT ShapedText final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ShapedText();
  ShapedText(const Font* font,
             const TextDirection& direction,
             V8CanvasTextBaseline::Enum baseline,
             V8CanvasTextAlign::Enum align,
             const String& text,
             PlainTextPainter& text_painter);

  double width() const { return width_; }

  // Converts between a horizontal offset from the alignment point and a
  // caret position (a code unit index) in the text.
  unsigned advanceToPosition(double x);
  // Returns the caret offset of `position`: the leading edge of the character
  // at `position`, or the trailing edge of the last character at the end of the
  // text (i.e. downstream affinity). For a position inside a cluster, this is
  // the leading edge of the cluster.
  //
  // Limitation: at the boundary of text in the opposite direction, a position
  // has two visual caret positions, and this returns only one. As a result,
  // both logical edges of that text can map to the same offset, e.g. both edges
  // of an RTL word between LTR words map to its right edge, and its left edge
  // has no position. Such positions can also map back to a different position
  // in advanceToPosition(), which handles these boundaries differently (see
  // CorrectForMixedBidi()). Resolving this needs an explicit caret affinity, or
  // both caret positions.
  double positionToAdvance(uint32_t position, ExceptionState& exception_state);

  // Selection rects for [start, end), one per direction run.
  const HeapVector<Member<DOMRectReadOnly>>
  selectionRects(uint32_t start, uint32_t end, ExceptionState& exception_state);
  // The union of the glyph ink bounds for [start, end).
  DOMRectReadOnly* actualBoundingBox(uint32_t start,
                                     uint32_t end,
                                     ExceptionState& exception_state);

  // Font- and direction-resolved runs, in visual order. Runs are split at
  // fallback font boundaries.
  HeapVector<Member<GlyphRun>> glyphRuns();
  // Minimal glyph clusters, with their resolved font. The range version
  // returns the clusters intersecting [start, end).
  HeapVector<Member<GlyphRun>> glyphClusters();
  HeapVector<Member<GlyphRun>> glyphClusters(uint32_t start,
                                             uint32_t end,
                                             ExceptionState& exception_state);

  struct RunWithOffset {
    DISALLOW_NEW();

    void Trace(Visitor* visitor) const { visitor->Trace(shape_result_); }

    Member<const ShapeResult> shape_result_{
        nullptr, Member<const ShapeResult>::AtomicInitializerTag{}};
    String text_;
    TextDirection direction_;
    unsigned character_offset_;
    unsigned num_characters_;
    float x_position_;
  };

  // A minimal glyph cluster (as produced by the shaper), as an absolute
  // [start, end) range in the text, with its horizontal position (not adjusted
  // for textAlign), advance, and resolved font. A glyph cluster can span
  // several grapheme clusters, e.g. for ligatures.
  struct ClusterData {
    DISALLOW_NEW();

    void Trace(Visitor* visitor) const { visitor->Trace(font_data); }

    unsigned start = 0;
    unsigned end = 0;
    float x_position = 0.0f;
    float width = 0.0f;
    Member<const SimpleFontData> font_data;
  };

  // Returns the minimal clusters intersecting [start, end). For out of range
  // indexes, throws an IndexSizeError and returns an empty vector.
  // `exception_state` may be null only when the range is known to be valid;
  // out of range indexes then CHECK-fail. No range is valid for an empty text.
  HeapVector<ClusterData> CollectClustersForRange(
      uint32_t start,
      uint32_t end,
      ExceptionState* exception_state) const;

  // Returns the union of the glyph bounds for [start, end), relative to the
  // text origin (not adjusted for textAlign/textBaseline). Does not validate
  // the range: callers must check it against the text length first.
  gfx::RectF ComputeGlyphBoundsForRange(uint32_t start, uint32_t end) const;

  const Font* GetFont() const { return font_.Get(); }
  const String& GetText() const { return text_; }
  TextDirection Direction() const { return direction_; }
  V8CanvasTextAlign::Enum ContextTextAlign() const { return ctx_text_align_; }
  V8CanvasTextBaseline::Enum ContextTextBaseline() const {
    return ctx_text_baseline_;
  }
  float TextAlignDx() const { return text_align_dx_; }
  // The offset of the alphabetic baseline below the textBaseline anchor, from
  // the primary font. All the runs are drawn at this offset, including the ones
  // using fallback fonts.
  float BaselineY() const { return baseline_y_; }
  const gfx::RectF& GlyphBounds() const { return glyph_bounds_; }

  void Trace(Visitor*) const override;

 private:
  void Update(const Font*,
              const TextDirection& direction,
              V8CanvasTextBaseline::Enum baseline,
              V8CanvasTextAlign::Enum align,
              const String&,
              PlainTextPainter& text_painter);
  // A helper for Update().  This function updates `runs_with_offset_`, and
  // returns a pair of the total width and the glyph bounding rectangle.
  std::pair<float, gfx::RectF> MeasureRuns(PlainTextPainter& text_painter);

  unsigned CorrectForMixedBidi(HeapVector<RunWithOffset>::reverse_iterator&,
                               unsigned);
  HeapVector<Member<GlyphRun>> GetGlyphClustersImpl(
      uint32_t start,
      uint32_t end,
      ExceptionState* exception_state);

  // x-direction
  double width_ = 0.0;
  // Delta needed for handling textAlign correctly.
  float text_align_dx_ = 0.0;
  // y-direction
  float baseline_y_ = 0.0;
  double font_bounding_box_ascent_ = 0.0;
  double font_bounding_box_descent_ = 0.0;
  gfx::RectF glyph_bounds_;

  // Needed for selection rects, bounding boxes and caret position.
  Member<const Font> font_;
  TextDirection direction_ = TextDirection::kLtr;
  String text_;

  // Values from the canvas context at the moment the text was shaped.
  V8CanvasTextAlign::Enum ctx_text_align_ = V8CanvasTextAlign::Enum::kStart;
  V8CanvasTextBaseline::Enum ctx_text_baseline_ =
      V8CanvasTextBaseline::Enum::kAlphabetic;

  HeapVector<RunWithOffset> runs_with_offset_;
};

template <>
struct VectorTraits<ShapedText::RunWithOffset>
    : VectorTraitsBase<ShapedText::RunWithOffset> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
  static constexpr bool kCanTraceConcurrently = true;
};

template <>
struct VectorTraits<ShapedText::ClusterData>
    : VectorTraitsBase<ShapedText::ClusterData> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_
