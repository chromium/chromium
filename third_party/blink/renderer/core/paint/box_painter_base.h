// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
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
                       bool include_logical_left_edge,
                       bool include_logical_right_edge);

  static void PaintNormalBoxShadow(const PaintInfo&,
                                   const PhysicalRect&,
                                   const ComputedStyle&,
                                   bool include_logical_left_edge = true,
                                   bool include_logical_right_edge = true,
                                   bool background_is_skipped = true);

  static void PaintInsetBoxShadowWithBorderRect(
      const PaintInfo&,
      const PhysicalRect&,
      const ComputedStyle&,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true);

  static void PaintInsetBoxShadowWithInnerRect(const PaintInfo&,
                                               const PhysicalRect&,
                                               const ComputedStyle&);

  static void PaintBorder(const ImageResourceObserver&,
                          const Document&,
                          Node*,
                          const PaintInfo&,
                          const PhysicalRect&,
                          const ComputedStyle&,
                          BackgroundBleedAvoidance = kBackgroundBleedNone,
                          bool include_logical_left_edge = true,
                          bool include_logical_right_edge = true);

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

  struct FillLayerInfo {
    STACK_ALLOCATED();

   public:
    FillLayerInfo(const Document&,
                  const ComputedStyle&,
                  bool has_overflow_clip,
                  Color bg_color,
                  const FillLayer&,
                  BackgroundBleedAvoidance,
                  bool include_left_edge,
                  bool include_right_edge,
                  bool is_inline);

    // FillLayerInfo is a temporary, stack-allocated container which cannot
    // outlive the StyleImage.  This would normally be a raw pointer, if not for
    // the Oilpan tooling complaints.
    Member<StyleImage> image;
    Color color;

    bool include_left_edge;
    bool include_right_edge;
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
  virtual FillLayerInfo GetFillLayerInfo(const Color&,
                                         const FillLayer&,
                                         BackgroundBleedAvoidance) const = 0;
  static void PaintInsetBoxShadow(const PaintInfo&,
                                  const FloatRoundedRect&,
                                  const ComputedStyle&,
                                  bool include_logical_left_edge = true,
                                  bool include_logical_right_edge = true);

 private:
  Member<const Document> document_;
  const ComputedStyle& style_;
  Member<Node> node_;
};

}  // namespace blink

#endif
