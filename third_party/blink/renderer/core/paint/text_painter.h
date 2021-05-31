// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class TextDecorationOffsetBase;
class TextRun;
struct TextRunPaintInfo;
class LayoutTextCombine;

// Text painter for legacy layout. Operates on TextRuns.
class CORE_EXPORT TextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  TextPainter(GraphicsContext& context,
              const Font& font,
              const TextRun& run,
              const PhysicalOffset& text_origin,
              const PhysicalRect& text_frame_rect,
              bool horizontal)
      : TextPainterBase(context,
                        font,
                        text_origin,
                        text_frame_rect,
                        horizontal),
        run_(run) {}
  ~TextPainter() = default;

  void SetCombinedText(LayoutTextCombine* combined_text) {
    combined_text_ = combined_text;
  }

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId node_id);

  void PaintDecorationsExceptLineThrough(const TextDecorationOffsetBase&,
                                         TextDecorationInfo&,
                                         const PaintInfo&,
                                         const Vector<AppliedTextDecoration>&,
                                         const TextPaintStyle& text_style,
                                         bool* has_line_through_decoration);
  void PaintDecorationsOnlyLineThrough(TextDecorationInfo&,
                                       const PaintInfo&,
                                       const Vector<AppliedTextDecoration>&,
                                       const TextPaintStyle&);

 private:
  template <PaintInternalStep step>
  void PaintInternalRun(TextRunPaintInfo&,
                        unsigned from,
                        unsigned to,
                        DOMNodeId node_id);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point,
                     DOMNodeId node_id);

  const TextRun& run_;
  LayoutTextCombine* combined_text_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
