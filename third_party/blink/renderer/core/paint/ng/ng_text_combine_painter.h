// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_COMBINE_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_COMBINE_PAINTER_H_

#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class ComputedStyle;
class LayoutNGTextCombine;

// The painter for painting text decorations and emphasis marks for
// LayoutNGTextCombine.
class NGTextCombinePainter final : public TextPainterBase {
 public:
  NGTextCombinePainter(GraphicsContext& context,
                       const ComputedStyle& style,
                       const PhysicalRect& text_frame_rect);
  ~NGTextCombinePainter();

  static void Paint(const PaintInfo& paint_info,
                    const PhysicalOffset& paint_offset,
                    const LayoutNGTextCombine& text_combine);

  static bool ShouldPaint(const LayoutNGTextCombine& text_combine);

 private:
  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;

  void PaintDecorations(const PaintInfo& paint_info,
                        const TextPaintStyle& text_style);
  void PaintEmphasisMark(const TextPaintStyle& text_style,
                         const Font& emphasis_mark_font);

  const ComputedStyle& style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_COMBINE_PAINTER_H_
