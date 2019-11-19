// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FillLayer;
class LayoutBox;
class LayoutBoxModelObject;
class LayoutObject;
class LayoutRect;
class LayoutTableCell;
class LayoutView;
class Document;
class ComputedStyle;
class ImageResourceObserver;

class BackgroundImageGeometry {
  DISALLOW_NEW();

 public:
  // Constructor for LayoutView where the coordinate space is different.
  BackgroundImageGeometry(const LayoutView&);

  // Constructor for table cells where background_object may be the row or
  // column the background image is attached to.
  BackgroundImageGeometry(const LayoutTableCell&,
                          const LayoutObject* background_object);

  // Generic constructor for all other elements.
  BackgroundImageGeometry(const LayoutBoxModelObject&);

  void Calculate(const LayoutBoxModelObject* container,
                 PaintPhase,
                 GlobalPaintFlags,
                 const FillLayer&,
                 const PhysicalRect& paint_rect);

  // Destination rects define the area into which the image will paint.
  // For cases where no explicit background size is requested, the destination
  // also defines the subset of the image to be drawn. Both border-snapped
  // and unsnapped rectangles are available. The snapped rectangle matches the
  // inner border of the box when such information is available. This may
  // may differ from the PixelSnappedIntRect of the unsnapped rectangle
  // because both border widths and border locations are snapped. The
  // unsnapped rectangle is the size and location intended by the content
  // author, and is needed to correctly subset images when no background-size
  // size is given.
  PhysicalRect UnsnappedDestRect() const {
    return PhysicalRectToBeNoop(unsnapped_dest_rect_);
  }
  PhysicalRect SnappedDestRect() const {
    return PhysicalRectToBeNoop(snapped_dest_rect_);
  }

  // Tile size is the area into which to draw one copy of the image. It
  // need not be the same as the intrinsic size of the image; if not,
  // the image will be resized (via an image filter) when painted into
  // that tile region. This may happen because of CSS background-size and
  // background-repeat requirements.
  const LayoutSize& TileSize() const { return tile_size_; }

  // Phase() represents the point in the image that will appear at (0,0) in the
  // destination space. The point is defined in TileSize() coordinates, that is,
  // in the scaled image.
  const FloatPoint& Phase() const { return phase_; }

  // SpaceSize() represents extra width and height that may be added to
  // the image if used as a pattern with background-repeat: space.
  const LayoutSize& SpaceSize() const { return repeat_spacing_; }

  // Has background-attachment: fixed. Implies that we can't always cheaply
  // compute the destination rects.
  bool HasNonLocalGeometry() const { return has_non_local_geometry_; }

  // Whether the background needs to be positioned relative to a container
  // element. Only used for tables.
  bool CellUsingContainerBackground() const {
    return cell_using_container_background_;
  }

  const ImageResourceObserver& ImageClient() const;
  const Document& ImageDocument() const;
  const ComputedStyle& ImageStyle() const;
  InterpolationQuality ImageInterpolationQuality() const;

  static bool ShouldUseFixedAttachment(const FillLayer&);

 private:
  void SetSpaceSize(const LayoutSize& repeat_spacing) {
    repeat_spacing_ = repeat_spacing;
  }
  void SetPhaseX(float x) { phase_.SetX(x); }
  void SetPhaseY(float y) { phase_.SetY(y); }

  void SetNoRepeatX(const FillLayer&,
                    LayoutUnit x_offset,
                    LayoutUnit snapped_x_offset);
  void SetNoRepeatY(const FillLayer&,
                    LayoutUnit y_offset,
                    LayoutUnit snapped_y_offset);
  void SetRepeatX(const FillLayer&,
                  LayoutUnit available_width,
                  LayoutUnit extra_offset);
  void SetRepeatY(const FillLayer&,
                  LayoutUnit available_height,
                  LayoutUnit extra_offset);
  void SetSpaceX(LayoutUnit space, LayoutUnit extra_offset);
  void SetSpaceY(LayoutUnit space, LayoutUnit extra_offset);

  void UseFixedAttachment(const LayoutPoint& attachment_point);
  void SetHasNonLocalGeometry() { has_non_local_geometry_ = true; }
  LayoutPoint GetOffsetForCell(const LayoutTableCell&, const LayoutBox&);
  LayoutSize GetBackgroundObjectDimensions(const LayoutTableCell&,
                                           const LayoutBox&);

  // Compute adjustments for the destination rects. Adjustments
  // both optimize painting when the background is obscured by a
  // border, and snap the dest rect to the border. They also
  // account for the background-clip property.
  void ComputeDestRectAdjustments(const FillLayer&,
                                  const LayoutRect&,
                                  bool,
                                  LayoutRectOutsets&,
                                  LayoutRectOutsets&) const;

  // Positioning area adjustments modify the size of the
  // positioning area to snap values and apply the
  // background-origin property.
  void ComputePositioningAreaAdjustments(const FillLayer&,
                                         const LayoutRect&,
                                         bool,
                                         LayoutRectOutsets&,
                                         LayoutRectOutsets&) const;

  void ComputePositioningArea(const LayoutBoxModelObject*,
                              PaintPhase,
                              GlobalPaintFlags,
                              const FillLayer&,
                              const LayoutRect&,
                              LayoutRect&,
                              LayoutRect&,
                              LayoutPoint&,
                              LayoutPoint&);
  void CalculateFillTileSize(const FillLayer&,
                             const LayoutSize&,
                             const LayoutSize&);

  // |box_| is the source for the Document. In most cases it also provides the
  // background properties (see |positioning_box_| for exceptions.) It's also
  // the image client unless painting the view background.
  const LayoutBoxModelObject& box_;

  // The positioning box is the source of geometric information for positioning
  // and sizing the background. It also provides the background properties if
  // painting the view background or a table-cell using its container's
  // (row's/column's) background.
  const LayoutBoxModelObject& positioning_box_;

  // When painting table cells or the view, the positioning area
  // differs from the requested paint rect.
  LayoutSize positioning_size_override_;

  // Used only when painting table cells, this is the cell's background
  // offset within the table's background positioning area.
  LayoutPoint offset_in_background_;

  LayoutRect unsnapped_dest_rect_;
  LayoutRect snapped_dest_rect_;
  FloatPoint phase_;
  LayoutSize tile_size_;
  LayoutSize repeat_spacing_;
  bool has_non_local_geometry_;
  bool painting_view_;
  bool painting_table_cell_;
  bool cell_using_container_background_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
