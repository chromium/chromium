// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/text_decoration_thickness.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ComputedStyle;
class Document;
class GraphicsContext;
class NGInlinePaintContext;
class Node;

namespace {

// We usually use the text decoration thickness to determine how far
// ink-skipped text decorations should be away from the glyph
// contours. Cap this at 5 CSS px in each direction when thickness
// growths larger than that. A value of 13 closely matches FireFox'
// implementation.
constexpr float kDecorationClipMaxDilation = 13;

}  // anonymous namespace

// Base class for text painting. This is the base class of NGTextPainter and
// NGTextCombinePainter.
class CORE_EXPORT TextPainterBase {
  STACK_ALLOCATED();

 public:
  TextPainterBase(GraphicsContext&,
                  const Font&,
                  const LineRelativeOffset& text_origin,
                  const LineRelativeRect& text_frame_rect,
                  NGInlinePaintContext* inline_context,
                  bool horizontal);
  ~TextPainterBase();

  const NGInlinePaintContext* InlineContext() const { return inline_context_; }

  void SetEmphasisMark(const AtomicString&, TextEmphasisPosition);

  enum ShadowMode { kBothShadowsAndTextProper, kShadowsOnly, kTextProperOnly };
  static void UpdateGraphicsContext(GraphicsContext&,
                                    const TextPaintStyle&,
                                    GraphicsContextStateSaver&,
                                    ShadowMode = kBothShadowsAndTextProper);
  static sk_sp<SkDrawLooper> CreateDrawLooper(
      const ShadowList* shadow_list,
      DrawLooperBuilder::ShadowAlphaMode,
      const Color& current_color,
      mojom::blink::ColorScheme color_scheme,
      ShadowMode = kBothShadowsAndTextProper);

  static Color TextColorForWhiteBackground(Color);
  static TextPaintStyle TextPaintingStyle(const Document&,
                                          const ComputedStyle&,
                                          const PaintInfo&);
  static TextPaintStyle SelectionPaintingStyle(
      const Document&,
      const ComputedStyle&,
      Node*,
      const PaintInfo&,
      const TextPaintStyle& text_style);

  enum RotationDirection { kCounterclockwise, kClockwise };
  static AffineTransform Rotation(const PhysicalRect& box_rect,
                                  RotationDirection);
  static AffineTransform Rotation(const PhysicalRect& box_rect, WritingMode);

 protected:
  void UpdateGraphicsContext(const TextPaintStyle& style,
                             GraphicsContextStateSaver& saver) {
    UpdateGraphicsContext(graphics_context_, style, saver);
  }
  void DecorationsStripeIntercepts(
      float upper,
      float stripe_width,
      float dilation,
      const Vector<Font::TextIntercept>& text_intercepts);

  void PaintDecorationsOnlyLineThrough(TextDecorationInfo&,
                                       const PaintInfo&,
                                       const TextPaintStyle&,
                                       const cc::PaintFlags* flags = nullptr);

  // We have two functions to paint text decorations, because we should paint
  // text and decorations in following order:
  //   1. Paint underline or overline text decorations
  //   2. Paint text
  //   3. Paint line through text decoration
  void PaintUnderOrOverLineDecorations(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const NGTextDecorationOffset& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const PaintInfo& paint_info,
      const TextPaintStyle& text_style,
      const cc::PaintFlags* flags = nullptr);

  virtual void ClipDecorationsStripe(const NGTextFragmentPaintInfo&,
                                     float upper,
                                     float stripe_width,
                                     float dilation) = 0;

  enum PaintInternalStep { kPaintText, kPaintEmphasisMark };

  NGInlinePaintContext* inline_context_ = nullptr;
  GraphicsContext& graphics_context_;
  const Font& font_;
  const LineRelativeOffset text_origin_;
  const LineRelativeRect text_frame_rect_;
  AtomicString emphasis_mark_;
  int emphasis_mark_offset_ = 0;
  const bool horizontal_;

 private:
  void PaintDecorationUnderOrOverLine(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      GraphicsContext& context,
      TextDecorationInfo& decoration_info,
      TextDecorationLine line,
      const cc::PaintFlags* flags = nullptr);

  void PaintUnderOrOverLineDecorationShadows(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const NGTextDecorationOffset& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const cc::PaintFlags* flags,
      const TextPaintStyle& text_style,
      GraphicsContext& context);

  void PaintUnderOrOverLineDecorations(
      const NGTextFragmentPaintInfo& fragment_paint_info,
      const NGTextDecorationOffset& decoration_offset,
      TextDecorationInfo& decoration_info,
      TextDecorationLine lines_to_paint,
      const cc::PaintFlags* flags,
      GraphicsContext& context);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINTER_BASE_H_
