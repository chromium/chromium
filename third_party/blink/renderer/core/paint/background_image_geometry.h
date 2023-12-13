// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/box_background_paint_context.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FillLayer;
class LayoutBox;
class LayoutBoxModelObject;
class LayoutTableCell;
class LayoutView;
class PhysicalBoxFragment;
struct PaintInfo;

class BackgroundImageGeometry {
  STACK_ALLOCATED();

 public:
  // Constructor for LayoutView where the coordinate space is different.
  BackgroundImageGeometry(
      const LayoutView&,
      const PhysicalOffset& element_positioning_area_offset);

  // Generic constructor for all other elements.
  explicit BackgroundImageGeometry(const LayoutBoxModelObject&);

  // Constructor for TablesNG table parts.
  BackgroundImageGeometry(const LayoutTableCell& cell,
                          PhysicalOffset cell_offset,
                          const LayoutBox& table_part,
                          PhysicalSize table_part_size);

  explicit BackgroundImageGeometry(const PhysicalBoxFragment&);

  // Calculates data members. This must be called before any of the following
  // getters is called. The document lifecycle phase must be at least
  // PrePaintClean.
  void Calculate(const PaintInfo& paint_info,
                 const FillLayer&,
                 const PhysicalRect& paint_rect);

  const BoxBackgroundPaintContext& GetContext() const { return paint_context_; }

  // Destination rects define the area into which the image will paint.
  // For cases where no explicit background size is requested, the destination
  // also defines the subset of the image to be drawn. Both border-snapped
  // and unsnapped rectangles are available. The snapped rectangle matches the
  // inner border of the box when such information is available. This may
  // may differ from the ToPixelSnappedRect of the unsnapped rectangle
  // because both border widths and border locations are snapped. The
  // unsnapped rectangle is the size and location intended by the content
  // author, and is needed to correctly subset images when no background-size
  // size is given.
  const PhysicalRect& UnsnappedDestRect() const { return unsnapped_dest_rect_; }
  const PhysicalRect& SnappedDestRect() const { return snapped_dest_rect_; }

  // Compute the phase of the image accounting for the size and spacing of the
  // image.
  PhysicalOffset ComputePhase() const;

  // Tile size is the area into which to draw one copy of the image. It
  // need not be the same as the intrinsic size of the image; if not,
  // the image will be resized (via an image filter) when painted into
  // that tile region. This may happen because of CSS background-size and
  // background-repeat requirements.
  const PhysicalSize& TileSize() const { return tile_size_; }

  // Phase() represents the point in the image that will appear at (0,0) in the
  // destination space. The point is defined in TileSize() coordinates, that is,
  // in the scaled image.
  const PhysicalOffset& Phase() const { return phase_; }

  // SpaceSize() represents extra width and height that may be added to
  // the image if used as a pattern with background-repeat: space.
  const PhysicalSize& SpaceSize() const { return repeat_spacing_; }

 private:
  void SetSpaceSize(const PhysicalSize& repeat_spacing) {
    repeat_spacing_ = repeat_spacing;
  }
  void SetPhaseX(LayoutUnit x) { phase_.left = x; }
  void SetPhaseY(LayoutUnit y) { phase_.top = y; }

  void SetNoRepeatX(const FillLayer&,
                    LayoutUnit x_offset,
                    LayoutUnit snapped_x_offset);
  void SetNoRepeatY(const FillLayer&,
                    LayoutUnit y_offset,
                    LayoutUnit snapped_y_offset);
  void SetRepeatX(LayoutUnit x_offset);
  void SetRepeatY(LayoutUnit y_offset);
  void SetSpaceX(LayoutUnit space, LayoutUnit extra_offset);
  void SetSpaceY(LayoutUnit space, LayoutUnit extra_offset);

  void UseFixedAttachment(const PhysicalOffset& attachment_point);

  // Compute adjustments for the destination rects. Adjustments
  // both optimize painting when the background is obscured by a
  // border, and snap the dest rect to the border. They also
  // account for the background-clip property.
  SnappedAndUnsnappedOutsets ComputeDestRectAdjustments(
      const FillLayer&,
      const PhysicalRect& unsnapped_positioning_area,
      bool disallow_border_derived_adjustment) const;

  // Positioning area adjustments modify the size of the
  // positioning area to snap values and apply the
  // background-origin property.
  SnappedAndUnsnappedOutsets ComputePositioningAreaAdjustments(
      const FillLayer&,
      const PhysicalRect& unsnapped_positioning_area,
      bool disallow_border_derived_adjustment) const;

  void AdjustPositioningArea(const PaintInfo&,
                             const FillLayer&,
                             const PhysicalRect&,
                             PhysicalRect&,
                             PhysicalRect&,
                             PhysicalOffset&,
                             PhysicalOffset&);
  void CalculateFillTileSize(const FillLayer&,
                             const PhysicalSize&,
                             const PhysicalSize&);
  void CalculateRepeatAndPosition(
      const FillLayer&,
      const PhysicalOffset& offset_in_background,
      const PhysicalSize& unsnapped_positioning_area_size,
      const PhysicalSize& snapped_positioning_area_size,
      const PhysicalOffset& unsnapped_box_offset,
      const PhysicalOffset& snapped_box_offset);

  const BoxBackgroundPaintContext paint_context_;

  PhysicalRect unsnapped_dest_rect_;
  PhysicalRect snapped_dest_rect_;
  PhysicalOffset phase_;
  PhysicalSize tile_size_;
  PhysicalSize repeat_spacing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
