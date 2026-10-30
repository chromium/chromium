// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class DOMRectReadOnly;
class ExceptionState;
class PlainTextPainter;

// Shaping state and shaping-based queries for a single-style text string, as
// measured by CanvasRenderingContext2D::measureText().
class CORE_EXPORT ShapedText final : public GarbageCollected<ShapedText> {
 public:
  ShapedText();
  ShapedText(const Font* font,
             const TextDirection& direction,
             V8CanvasTextBaseline::Enum baseline,
             V8CanvasTextAlign::Enum align,
             const String& text,
             PlainTextPainter& text_painter);

  double width() const { return width_; }

  unsigned getIndexFromOffset(double x);

  const HeapVector<Member<DOMRectReadOnly>> getSelectionRects(
      uint32_t start,
      uint32_t end,
      ExceptionState& exception_state);
  DOMRectReadOnly* getActualBoundingBox(uint32_t start,
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

  const Font* GetFont() const { return font_.Get(); }
  const String& GetText() const { return text_; }
  TextDirection Direction() const { return direction_; }
  V8CanvasTextAlign::Enum ContextTextAlign() const { return ctx_text_align_; }
  V8CanvasTextBaseline::Enum ContextTextBaseline() const {
    return ctx_text_baseline_;
  }
  float TextAlignDx() const { return text_align_dx_; }
  const gfx::RectF& GlyphBounds() const { return glyph_bounds_; }
  const HeapVector<RunWithOffset>& RunsWithOffset() const {
    return runs_with_offset_;
  }

  void Trace(Visitor*) const;

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

  // Values from the canvas context at the moment the text was measured.
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_SHAPED_TEXT_H_
