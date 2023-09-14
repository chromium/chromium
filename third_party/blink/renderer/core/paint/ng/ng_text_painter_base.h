// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_BASE_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Font;
class GraphicsContext;
class NGInlinePaintContext;
class TextDecorationInfo;
class TextDecorationOffsetBase;
struct NGTextFragmentPaintInfo;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;
struct TextPaintStyle;

// LayoutNG-specific base class for text painting. Augments TextPainterBase with
// functionality to be shared between NGTextPainter and NGTextCombinePainter.
class CORE_EXPORT NGTextPainterBase : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  NGTextPainterBase(GraphicsContext& context,
                    const Font& font,
                    const PhysicalOffset& text_origin,
                    const PhysicalRect& text_frame_rect,
                    NGInlinePaintContext* inline_context,
                    bool horizontal)
      : TextPainterBase(context,
                        font,
                        text_origin,
                        text_frame_rect,
                        inline_context,
                        horizontal) {}
  ~NGTextPainterBase() = default;

 protected:
  // We have two functions to paint text decorations, because we should paint
  // text and decorations in following order:
  //   1. Paint underline or overline text decorations
  //   2. Paint text
  //   3. Paint line through text decoration
  void PaintUnderOrOverLineDecorations(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const TextDecorationOffsetBase& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const PaintInfo& paint_info,
      const TextPaintStyle& text_style,
      const cc::PaintFlags* flags = nullptr);

  virtual void ClipDecorationsStripe(const NGTextFragmentPaintInfo&,
                                     float upper,
                                     float stripe_width,
                                     float dilation) = 0;

 private:
  void PaintDecorationUnderOrOverLine(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      GraphicsContext& context,
      TextDecorationInfo& decoration_info,
      TextDecorationLine line,
      const cc::PaintFlags* flags = nullptr);

  void PaintUnderOrOverLineDecorationShadows(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const TextDecorationOffsetBase& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const cc::PaintFlags* flags,
      const TextPaintStyle& text_style,
      GraphicsContext& context);

  void PaintUnderOrOverLineDecorations(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const TextDecorationOffsetBase& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const cc::PaintFlags* flags,
      GraphicsContext& context);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_
