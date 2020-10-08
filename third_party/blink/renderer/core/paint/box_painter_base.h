// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class BackgroundImageGeometry;
class ComputedStyle;
class Document;
class FillLayer;
class FloatRoundedRect;
class GraphicsContext;
class ImageResourceObserver;
class IntRect;
class LayoutBox;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

// Base class for box painting. Has no dependencies on the layout tree and thus
// provides functionality and definitions that can be shared between both legacy
// layout and LayoutNG.
class BoxPainterBase {
  STACK_ALLOCATED();

 public:
  BoxPainterBase(const Document* document,
                 const ComputedStyle& style,
                 Node* node)
      : document_(document), style_(style), node_(node) {}

  void PaintFillLayers(const PaintInfo&,
                       const Color&,
                       const FillLayer&,
                       const PhysicalRect&,
                       BackgroundImageGeometry&,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  void PaintFillLayer(const PaintInfo&,
                      const Color&,
                      const FillLayer&,
                      const PhysicalRect&,
                      BackgroundBleedAvoidance,
                      BackgroundImageGeometry&,
                      bool object_has_multiple_boxes = false,
                      const PhysicalSize& flow_box_size = PhysicalSize());

  void PaintMaskImages(const PaintInfo&,
                       const PhysicalRect&,
                       const ImageResourceObserver&,
                       BackgroundImageGeometry&,
                       PhysicalBoxSides sides_to_include);

  static void PaintNormalBoxShadow(
      const PaintInfo&,
      const PhysicalRect&,
      const ComputedStyle&,
      PhysicalBoxSides sides_to_include = PhysicalBoxSides(),
      bool background_is_skipped = true);

  static void PaintInsetBoxShadowWithBorderRect(
      const PaintInfo&,
      const PhysicalRect&,
      const ComputedStyle&,
      PhysicalBoxSides sides_to_include = PhysicalBoxSides());

  static void PaintInsetBoxShadowWithInnerRect(const PaintInfo&,
                                               const PhysicalRect&,
                                               const ComputedStyle&);

  static void PaintBorder(
      const ImageResourceObserver&,
      const Document&,
      Node*,
      const PaintInfo&,
      const PhysicalRect&,
      const ComputedStyle&,
      BackgroundBleedAvoidance = kBackgroundBleedNone,
      PhysicalBoxSides sides_to_include = PhysicalBoxSides());

  static bool ShouldForceWhiteBackgroundForPrintEconomy(const Document&,
                                                        const ComputedStyle&);

  typedef Vector<const FillLayer*, 8> FillLayerOcclusionOutputList;
  // Returns true if the result fill layers have non-associative blending or
  // compositing mode.  (i.e. The rendering will be different without creating
  // isolation group by context.saveLayer().) Note that the output list will be
  // in top-bottom order.
  bool CalculateFillLayerOcclusionCulling(
      FillLayerOcclusionOutputList& reversed_paint_list,
      const FillLayer&);

  static bool ShouldSkipPaintUnderInvalidationChecking(const LayoutBox&);

  struct FillLayerInfo {
    STACK_ALLOCATED();

   public:
    FillLayerInfo(const Document&,
                  const ComputedStyle&,
                  bool is_scroll_container,
                  Color bg_color,
                  const FillLayer&,
                  BackgroundBleedAvoidance,
                  RespectImageOrientationEnum,
                  PhysicalBoxSides sides_to_include,
                  bool is_inline,
                  bool is_painting_scrolling_background);

    // FillLayerInfo is a temporary, stack-allocated container which cannot
    // outlive the StyleImage.  This would normally be a raw pointer, if not for
    // the Oilpan tooling complaints.
    StyleImage* image;
    Color color;

    RespectImageOrientationEnum respect_image_orientation;
    PhysicalBoxSides sides_to_include;
    bool is_bottom_layer;
    bool is_border_fill;
    bool is_clipped_with_local_scrolling;
    bool is_rounded_fill;
    bool should_paint_image;
    bool should_paint_color;
  };

 protected:
  virtual LayoutRectOutsets ComputeBorders() const = 0;
  virtual LayoutRectOutsets ComputePadding() const = 0;
  LayoutRectOutsets AdjustedBorderOutsets(const FillLayerInfo&) const;
  void PaintFillLayerTextFillBox(GraphicsContext&,
                                 const FillLayerInfo&,
                                 Image*,
                                 SkBlendMode composite_op,
                                 const BackgroundImageGeometry&,
                                 const PhysicalRect&,
                                 const PhysicalRect& scrolled_paint_rect,
                                 bool object_has_multiple_boxes);
  virtual void PaintTextClipMask(GraphicsContext&,
                                 const IntRect& mask_rect,
                                 const PhysicalOffset& paint_offset,
                                 bool object_has_multiple_boxes) = 0;

  virtual PhysicalRect AdjustRectForScrolledContent(const PaintInfo&,
                                                    const FillLayerInfo&,
                                                    const PhysicalRect&) = 0;
  virtual FillLayerInfo GetFillLayerInfo(
      const Color&,
      const FillLayer&,
      BackgroundBleedAvoidance,
      bool is_painting_scrolling_background) const = 0;
  virtual bool IsPaintingScrollingBackground(const PaintInfo&) const = 0;
  static void PaintInsetBoxShadow(
      const PaintInfo&,
      const FloatRoundedRect&,
      const ComputedStyle&,
      PhysicalBoxSides sides_to_include = PhysicalBoxSides());

 private:
  const Document* document_;
  const ComputedStyle& style_;
  Node* node_;
};

}  // namespace blink

#endif
