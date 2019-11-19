// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

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
              const LayoutPoint& text_origin,
              const LayoutRect& text_bounds,
              bool horizontal)
      : TextPainterBase(context,
                        font,
                        PhysicalOffset(text_origin),
                        PhysicalRect(text_bounds),
                        horizontal),
        run_(run),
        combined_text_(nullptr) {}
  ~TextPainter() = default;

  void SetCombinedText(LayoutTextCombine* combined_text) {
    combined_text_ = combined_text;
    has_combined_text_ = combined_text_ ? true : false;
  }

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId node_id);

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

  void PaintEmphasisMarkForCombinedText();

  const TextRun& run_;
  LayoutTextCombine* combined_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
