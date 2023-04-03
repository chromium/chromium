// Copyright 2014 The Chromium Authors
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
                        /* inline_context */ nullptr,
                        horizontal),
        run_(run) {}
  ~TextPainter() = default;

  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId node_id,
             const AutoDarkMode& auto_dark_mode);

  void PaintDecorationsExceptLineThrough(
      const TextDecorationOffsetBase&,
      TextDecorationInfo&,
      const PaintInfo&,
      const Vector<AppliedTextDecoration, 1>&,
      const TextPaintStyle& text_style);
  void PaintDecorationsOnlyLineThrough(TextDecorationInfo&,
                                       const PaintInfo&,
                                       const Vector<AppliedTextDecoration, 1>&,
                                       const TextPaintStyle&);

 private:
  template <PaintInternalStep step>
  void PaintInternalRun(TextRunPaintInfo&,
                        unsigned from,
                        unsigned to,
                        DOMNodeId node_id,
                        const AutoDarkMode& auto_dark_mode);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point,
                     DOMNodeId node_id,
                     const AutoDarkMode& auto_dark_mode);

  void ClipDecorationsStripe(float upper, float stripe_width, float dilation);

  void PaintDecorationUnderOrOverLine(GraphicsContext& context,
                                      TextDecorationInfo& decoration_info,
                                      TextDecorationLine line,
                                      const cc::PaintFlags* flags = nullptr);

  const TextRun& run_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_H_
