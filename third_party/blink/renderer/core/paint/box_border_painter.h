// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_BORDER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_BORDER_PAINTER_H_

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class Path;
struct PaintInfo;
struct PhysicalRect;

typedef unsigned BorderEdgeFlags;

class BoxBorderPainter {
  STACK_ALLOCATED();

 public:
  BoxBorderPainter(const PhysicalRect& border_rect,
                   const ComputedStyle&,
                   BackgroundBleedAvoidance,
                   bool include_logical_left_edge,
                   bool include_logical_right_edge);

  BoxBorderPainter(const ComputedStyle&,
                   const PhysicalRect& outer,
                   const PhysicalRect& inner,
                   const BorderEdge& uniform_edge_info);

  void PaintBorder(const PaintInfo&, const PhysicalRect& border_rect) const;

 private:
  struct ComplexBorderInfo;
  enum MiterType {
    kNoMiter,
    kSoftMiter,  // Anti-aliased
    kHardMiter,  // Not anti-aliased
  };

  void ComputeBorderProperties();

  BorderEdgeFlags PaintOpacityGroup(GraphicsContext&,
                                    const ComplexBorderInfo&,
                                    unsigned index,
                                    float accumulated_opacity) const;
  void PaintSide(GraphicsContext&,
                 const ComplexBorderInfo&,
                 BoxSide,
                 unsigned alpha,
                 BorderEdgeFlags) const;
  void PaintOneBorderSide(GraphicsContext&,
                          const FloatRect& side_rect,
                          BoxSide,
                          BoxSide adjacent_side1,
                          BoxSide adjacent_side2,
                          const Path*,
                          bool antialias,
                          Color,
                          BorderEdgeFlags) const;
  bool PaintBorderFastPath(GraphicsContext&,
                           const PhysicalRect& border_rect) const;
  void DrawDoubleBorder(GraphicsContext&,
                        const PhysicalRect& border_rect) const;

  void DrawBoxSideFromPath(GraphicsContext&,
                           const PhysicalRect&,
                           const Path&,
                           float thickness,
                           float draw_thickness,
                           BoxSide,
                           Color,
                           EBorderStyle) const;
  void DrawDashedDottedBoxSideFromPath(GraphicsContext&,
                                       const PhysicalRect&,
                                       float thickness,
                                       float draw_thickness,
                                       Color,
                                       EBorderStyle) const;
  void DrawWideDottedBoxSideFromPath(GraphicsContext&,
                                     const Path&,
                                     float thickness) const;
  void DrawDoubleBoxSideFromPath(GraphicsContext&,
                                 const PhysicalRect&,
                                 const Path&,
                                 float thickness,
                                 float draw_thickness,
                                 BoxSide,
                                 Color) const;
  void DrawRidgeGrooveBoxSideFromPath(GraphicsContext&,
                                      const PhysicalRect&,
                                      const Path&,
                                      float thickness,
                                      float draw_thickness,
                                      BoxSide,
                                      Color,
                                      EBorderStyle) const;
  void ClipBorderSidePolygon(GraphicsContext&,
                             BoxSide,
                             MiterType miter1,
                             MiterType miter2) const;
  void ClipBorderSideForComplexInnerPath(GraphicsContext&, BoxSide) const;

  MiterType ComputeMiter(BoxSide,
                         BoxSide adjacent_side,
                         BorderEdgeFlags,
                         bool antialias) const;
  static bool MitersRequireClipping(MiterType miter1,
                                    MiterType miter2,
                                    EBorderStyle,
                                    bool antialias);

  const BorderEdge& FirstEdge() const {
    DCHECK(visible_edge_set_);
    return edges_[first_visible_edge_];
  }

  // const inputs
  const ComputedStyle& style_;
  const BackgroundBleedAvoidance bleed_avoidance_;
  const bool include_logical_left_edge_;
  const bool include_logical_right_edge_;

  // computed attributes
  FloatRoundedRect outer_;
  FloatRoundedRect inner_;
  BorderEdge edges_[4];

  unsigned visible_edge_count_;
  unsigned first_visible_edge_;
  BorderEdgeFlags visible_edge_set_;

  bool is_uniform_style_;
  bool is_uniform_width_;
  bool is_uniform_color_;
  bool is_rounded_;
  bool has_alpha_;
};

}  // namespace blink

#endif
