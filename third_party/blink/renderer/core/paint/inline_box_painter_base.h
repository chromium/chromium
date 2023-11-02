// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gfx {
class Rect;
}

namespace blink {

class Color;
class ComputedStyle;
class FillLayer;
class NinePieceImage;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

// Common base class for InlineFlowBoxPainter and NGInlineBoxFragmentPainter.
// Implements layout agnostic inline box painting behavior.
class InlineBoxPainterBase {
  STACK_ALLOCATED();

 public:
  InlineBoxPainterBase(const ImageResourceObserver& image_observer,
                       const Document* document,
                       Node* node,
                       const ComputedStyle& style,
                       const ComputedStyle& line_style)
      : image_observer_(image_observer),
        document_(document),
        node_(node),
        style_(style),
        line_style_(line_style) {}

  void PaintBoxDecorationBackground(BoxPainterBase&,
                                    const PaintInfo&,
                                    const PhysicalOffset& paint_offset,
                                    const PhysicalRect& adjusted_frame_rect,
                                    BackgroundImageGeometry,
                                    bool object_has_multiple_boxes,
                                    PhysicalBoxSides sides_to_include);

 protected:
  void PaintFillLayers(BoxPainterBase&,
                       const PaintInfo&,
                       const Color&,
                       const FillLayer&,
                       const PhysicalRect&,
                       BackgroundImageGeometry& geometry,
                       bool object_has_multiple_boxes);
  void PaintFillLayer(BoxPainterBase&,
                      const PaintInfo&,
                      const Color&,
                      const FillLayer&,
                      const PhysicalRect&,
                      BackgroundImageGeometry& geometry,
                      bool object_has_multiple_boxes);
  void PaintMask(BoxPainterBase&,
                 const PaintInfo&,
                 const PhysicalRect& paint_rect,
                 BackgroundImageGeometry&,
                 bool object_has_multiple_boxes,
                 PhysicalBoxSides sides_to_include);
  virtual void PaintNormalBoxShadow(const PaintInfo&,
                                    const ComputedStyle&,
                                    const PhysicalRect& paint_rect) = 0;
  virtual void PaintInsetBoxShadow(const PaintInfo&,
                                   const ComputedStyle&,
                                   const PhysicalRect& paint_rect) = 0;

  static PhysicalRect ClipRectForNinePieceImageStrip(
      const ComputedStyle& style,
      PhysicalBoxSides sides_to_include,
      const NinePieceImage& image,
      const PhysicalRect& paint_rect);
  virtual PhysicalRect PaintRectForImageStrip(
      const PhysicalRect&,
      TextDirection direction) const = 0;

  enum BorderPaintingType {
    kDontPaintBorders,
    kPaintBordersWithoutClip,
    kPaintBordersWithClip
  };
  virtual BorderPaintingType GetBorderPaintType(
      const PhysicalRect& adjusted_frame_rect,
      gfx::Rect& adjusted_clip_rect,
      bool object_has_multiple_boxes) const = 0;

  const ImageResourceObserver& image_observer_;
  const Document* document_;
  Node* node_;

  // Style for the corresponding node.
  const ComputedStyle& style_;

  // Style taking ::first-line into account.
  const ComputedStyle& line_style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_
