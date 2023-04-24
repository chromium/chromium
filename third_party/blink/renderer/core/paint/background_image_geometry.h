// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class Document;
class FillLayer;
class ImageResourceObserver;
class LayoutBox;
class LayoutBoxModelObject;
class LayoutNGTableCell;
class LayoutView;
class NGPhysicalBoxFragment;
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
  BackgroundImageGeometry(const LayoutNGTableCell& cell,
                          PhysicalOffset cell_offset,
                          const LayoutBox& table_part,
                          PhysicalSize table_part_size);

  explicit BackgroundImageGeometry(const NGPhysicalBoxFragment&);

  // Calculates data members. This must be called before any of the following
  // getters is called. The document lifecycle phase must be at least
  // PrePaintClean.
  void Calculate(const PaintInfo& paint_info,
                 const FillLayer&,
                 const PhysicalRect& paint_rect);

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

  // Compute the phase relative to the (snapped) destination offset.
  PhysicalOffset ComputeDestPhase() const;

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

  // Whether the background needs to be positioned relative to a container
  // element. Only used for tables.
  bool CellUsingContainerBackground() const {
    return cell_using_container_background_;
  }

  const ImageResourceObserver& ImageClient() const;
  const Document& ImageDocument() const;
  const ComputedStyle& ImageStyle(const ComputedStyle& fragment_style) const;
  InterpolationQuality ImageInterpolationQuality() const;

  bool CanCompositeBackgroundAttachmentFixed() const;

  static bool HasBackgroundFixedToViewport(const LayoutBoxModelObject&);

 private:
  BackgroundImageGeometry(const LayoutBoxModelObject* box,
                          const LayoutBoxModelObject* positioning_box);

  bool ShouldUseFixedAttachment(const FillLayer&) const;

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
  void SetRepeatX(const FillLayer&,
                  LayoutUnit available_width,
                  LayoutUnit extra_offset);
  void SetRepeatY(const FillLayer&,
                  LayoutUnit available_height,
                  LayoutUnit extra_offset);
  void SetSpaceX(LayoutUnit space, LayoutUnit extra_offset);
  void SetSpaceY(LayoutUnit space, LayoutUnit extra_offset);

  PhysicalRect FixedAttachmentPositioningArea(const PaintInfo&) const;
  void UseFixedAttachment(const PhysicalOffset& attachment_point);

  // Compute adjustments for the destination rects. Adjustments
  // both optimize painting when the background is obscured by a
  // border, and snap the dest rect to the border. They also
  // account for the background-clip property.
  void ComputeDestRectAdjustments(const FillLayer&,
                                  const PhysicalRect&,
                                  bool,
                                  NGPhysicalBoxStrut&,
                                  NGPhysicalBoxStrut&) const;

  // Positioning area adjustments modify the size of the
  // positioning area to snap values and apply the
  // background-origin property.
  void ComputePositioningAreaAdjustments(const FillLayer&,
                                         const PhysicalRect&,
                                         bool,
                                         NGPhysicalBoxStrut&,
                                         NGPhysicalBoxStrut&) const;

  void ComputePositioningArea(const PaintInfo&,
                              const FillLayer&,
                              const PhysicalRect&,
                              PhysicalRect&,
                              PhysicalRect&,
                              PhysicalOffset&,
                              PhysicalOffset&);
  void CalculateFillTileSize(const FillLayer&,
                             const PhysicalSize&,
                             const PhysicalSize&);

  // The offset of the background image within the background positioning area.
  PhysicalOffset OffsetInBackground(const FillLayer&) const;

  // In most cases this is the same as positioning_box_. They are different
  // when we are painting:
  // 1. the view background (box_ is the LayoutView, and positioning_box_ is
  //    the LayoutView's RootBox()), or
  // 2. a table cell using its row/column's background (box_ is the table cell,
  //    and positioning_box_ is the row/column).
  // When they are different:
  // - ImageDocument() uses box_;
  // - ImageClient() uses box_ if painting view, otherwise positioning_box_;
  // - ImageStyle() uses positioning_box_;
  // - ImageInterpolationQuality() uses box_;
  // - FillLayers come from box_ if painting view, otherwise positioning_box_.
  const LayoutBoxModelObject* const box_;

  // The positioning box is the source of geometric information for positioning
  // and sizing the background. It also provides the information listed in the
  // comment for box_.
  const LayoutBoxModelObject* const positioning_box_;

  // When painting table cells or the view, the positioning area
  // differs from the requested paint rect.
  PhysicalSize positioning_size_override_;

  // The background image offset from within the background positioning area for
  // non-fixed background attachment. Used for table cells and the view, and
  // also when an element is block-fragmented.
  PhysicalOffset element_positioning_area_offset_;

  PhysicalRect unsnapped_dest_rect_;
  PhysicalRect snapped_dest_rect_;
  PhysicalOffset phase_;
  PhysicalSize tile_size_;
  PhysicalSize repeat_spacing_;
  bool has_background_fixed_to_viewport_ = false;
  bool painting_view_ = false;
  bool painting_table_cell_ = false;
  bool cell_using_container_background_ = false;
  bool box_has_multiple_fragments_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BACKGROUND_IMAGE_GEOMETRY_H_
