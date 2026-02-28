// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_STICKY_POSITION_SCROLLING_CONSTRAINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_STICKY_POSITION_SCROLLING_CONSTRAINTS_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/layout/geometry/box_edge.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LayoutBoxModelObject;
class PaintLayer;

// Encapsulates the constraint information for a position: sticky element and
// does calculation of the sticky offset for a given overflow clip rectangle.
//
// To avoid slowing down scrolling we cannot make the offset calculation a
// layout-inducing event. Instead constraint information is cached during layout
// and used as the scroll position changes to determine the current offset. In
// most cases the only information that is needed is the sticky element's layout
// rectangle and its containing block rectangle (both respective to the nearest
// ancestor scroller which the element is sticking to), and the set of sticky
// edge constraints (i.e. the distance from each edge the element should stick).
//
// For a given (non-cached) overflow clip rectangle, calculating the current
// offset in most cases just requires sliding the (cached) sticky element
// rectangle until it satisfies the (cached) sticky edge constraints for the
// overflow clip rectangle, whilst not letting the sticky element rectangle
// escape its (cached) containing block rect. For example, consider the
// following situation (where positions are relative to the scroll container):
//
//    +---------------------+ <-- Containing Block (150x70 at 10,0)
//    | +-----------------+ |
//    | |    top: 50px;   |<-- Sticky Box (130x10 at 20,0)
//    | +-----------------+ |
//    |                     |
//  +-------------------------+ <-- Overflow Clip Rectangle (170x60 at 0,50)
//  | |                     | |
//  | |                     | |
//  | +---------------------+ |
//  |                         |
//  |                         |
//  |                         |
//  +-------------------------+
//
// Here the cached sticky box would be moved downwards to try and be at position
// (20,100) - 50 pixels down from the top of the clip rectangle. However doing
// so would take it outside the cached containing block rectangle, so the final
// sticky position would be capped to (20,20).
//
// Unfortunately this approach breaks down in the presence of nested sticky
// elements, as the cached locations would be moved by ancestor sticky elements.
// Consider:
//
//  +------------------------+ <-- Outer sticky (top: 10px, 150x50 at 0,0)
//  |  +------------------+  |
//  |  |                  | <-- Inner sticky (top: 25px, 100x20 at 20,0)
//  |  +------------------+  |
//  |                        |
//  +------------------------+
//
// Assume the overflow clip rectangle is centered perfectly over the outer
// sticky. We would then want to move the outer sticky element down by 10
// pixels, and the inner sticky element down by only 15 pixels - because it is
// already being shifted by its ancestor. To correctly handle such situations we
// apply more complicated logic which is explained in the implementation of
// |ComputeStickyOffset|.
struct CORE_EXPORT StickyPositionScrollingConstraints final {
  STACK_ALLOCATED();

 public:
  // The containing block rect and sticky box rect are the basic components
  // for calculating the sticky offset to apply after a scroll. Consider the
  // following setup:
  //
  // <scroll-container>
  //   <containing-block> (*)
  //     <sticky-element>
  //
  // (*) <containing-block> may be the same as <scroll-container>.
  //
  // Constraint data for a specific physical axis (horizontal or vertical).
  struct CORE_EXPORT PerAxisData : public GarbageCollected<PerAxisData> {
    PerAxisData(
        PhysicalAxis axis,
        const PhysicalRect& containing_block,
        const PhysicalRect& sticky_box,
        const PhysicalRect& constraining,
        const LayoutBoxModelObject* nearest_sticky_layer_shifting_sticky_box,
        const LayoutBoxModelObject*
            nearest_sticky_layer_shifting_containing_block,
        const PaintLayer* containing_scroll_container_layer,
        bool is_fixed_to_view,
        std::optional<LayoutUnit> min_inset,
        std::optional<LayoutUnit> max_inset);

    // The axis this data represents.
    const PhysicalAxis axis;

    // The distances from the scroll container edges that the sticky box is
    // constrained to stay within.
    const std::optional<LayoutUnit> min_inset;
    const std::optional<LayoutUnit> max_inset;

    // The layout position of the containing block relative to the scroll
    // container. When calculating the sticky offset it is used to ensure the
    // sticky element stays bounded by its containing block.
    const BoxEdge scroll_container_relative_containing_block_range;

    // The layout position of the sticky element relative to the scroll
    // container. When calculating the sticky offset it is used to determine
    // how large the offset needs to be to satisfy the sticky constraints.
    const BoxEdge scroll_container_relative_sticky_box_range;

    // The rectangle in which the sticky box is able to be positioned. This may
    // be smaller than the scroller viewport due to things like padding.
    const BoxEdge constraining_range;

    // In the case of nested sticky elements the layout position of the sticky
    // element and its containing block are not accurate (as they are affected
    // by ancestor sticky offsets). To ensure a correct sticky offset
    // calculation in that case we must track any sticky ancestors between the
    // sticky element and its containing block, and between its containing block
    // and the overflow clip ancestor.
    //
    // See the implementation of |ComputeOffset| for documentation on how
    // these ancestors are used to correct the offset calculation.
    const Member<const LayoutBoxModelObject>
        nearest_sticky_layer_shifting_sticky_box;
    const Member<const LayoutBoxModelObject>
        nearest_sticky_layer_shifting_containing_block;

    // These fields cache the result of
    // PaintLayer::ContainingScrollContainerLayer() for this axis.
    const Member<const PaintLayer> containing_scroll_container_layer;

    // Whether or not a `position: fixed` element exists in the chain up to
    // the parent `containing_scroll_container_layer`. If so - the sticky
    // element (for this axis) will not be affected by scrolling from that
    // parent.
    const bool is_fixed_to_view;

    // For performance we cache our accumulated sticky offset to allow
    // descendant sticky elements to offset their constraint rects. Because we
    // can either affect a descendant element's sticky box constraint rect or
    // containing block constraint rect, we need to accumulate two offsets.
    //
    // The sticky box offset accumulates the chain of sticky elements that are
    // between this sticky element and its containing block. Any descendant
    // using |total_sticky_box_sticky_offset| has the same containing block as
    // this element, so |total_sticky_box_sticky_offset| does not accumulate
    // containing block sticky offsets. For example, consider the following
    // chain:
    //
    // <div style="position: sticky;">
    //   <div id="outerInline" style="position: sticky; display: inline;">
    //     <div id="innerInline" style="position: sticky; display:
    //     inline;"><div>
    //   </div>
    // </div>
    //
    // In the above example, both outerInline and innerInline have the same
    // containing block - the outermost <div>.
    LayoutUnit total_sticky_box_sticky_offset;

    // The containing block offset accumulates all sticky-related offsets
    // between this element and the ancestor scroller. If this element is a
    // containing block shifting ancestor for some descendant, it shifts the
    // descendant's constraint rects by its entire offset.
    LayoutUnit total_containing_block_sticky_offset;

    // This is the real sticky offset which is |total_sticky_box_sticky_offset|
    // - |AncestorStickyBoxOffset()|. It's stored to avoid access to the
    // Member<PaintLayer> fields from StickyOffset() in case it's called during
    // layout.
    LayoutUnit sticky_offset;

    void ComputeOffset(float scroll_position);

    void Trace(Visitor* visitor) const;

   private:
    LayoutUnit AncestorStickyBoxOffset() const;
    LayoutUnit AncestorContainingBlockOffset() const;
  };

  StickyPositionScrollingConstraints() = default;
  StickyPositionScrollingConstraints(PerAxisData* x_data, PerAxisData* y_data)
      : x_data_(x_data), y_data_(y_data) {}

  explicit operator bool() const { return x_data_ || y_data_; }

  // Computes the sticky offset for a given scroll position of the containing
  // scroll container. When the scroll position changed in a ScrollableArea,
  // this method must be called for all affected sticky objects in pre-tree
  // order.
  void ComputeStickyOffset(const gfx::PointF& scroll_position,
                           PhysicalAxes scroll_axes);

  const PerAxisData* AxisData(PhysicalAxis axis) const {
    return (axis == PhysicalAxis::kHorizontal) ? x_data_ : y_data_;
  }

  bool HasScrollDependentOffset() const;

  // Returns the last-computed offset of the sticky box from its original
  // position before scroll.
  PhysicalOffset StickyOffset() const;

  // TODO(crbug.com/481019005): Remove 2D rect reconstruction once the
  // compositor supports independent axes.
  PhysicalRect ConstrainingRect() const {
    return BuildRect(x_data_ ? &x_data_->constraining_range : nullptr,
                     y_data_ ? &y_data_->constraining_range : nullptr);
  }
  PhysicalRect ScrollContainerRelativeContainingBlockRect() const {
    return BuildRect(
        x_data_ ? &x_data_->scroll_container_relative_containing_block_range
                : nullptr,
        y_data_ ? &y_data_->scroll_container_relative_containing_block_range
                : nullptr);
  }
  PhysicalRect ScrollContainerRelativeStickyBoxRect() const {
    return BuildRect(
        x_data_ ? &x_data_->scroll_container_relative_sticky_box_range
                : nullptr,
        y_data_ ? &y_data_->scroll_container_relative_sticky_box_range
                : nullptr);
  }

  std::optional<LayoutUnit> LeftInset() const {
    return x_data_ ? x_data_->min_inset : std::nullopt;
  }
  std::optional<LayoutUnit> RightInset() const {
    return x_data_ ? x_data_->max_inset : std::nullopt;
  }
  std::optional<LayoutUnit> TopInset() const {
    return y_data_ ? y_data_->min_inset : std::nullopt;
  }
  std::optional<LayoutUnit> BottomInset() const {
    return y_data_ ? y_data_->max_inset : std::nullopt;
  }

  const PaintLayer* ContainingScrollContainerLayer() const {
    if (const auto* data = PreferredAxisData()) {
      return data->containing_scroll_container_layer.Get();
    }
    return nullptr;
  }

  const LayoutBoxModelObject* NearestStickyLayerShiftingStickyBox() const {
    if (const auto* data = PreferredAxisData()) {
      return data->nearest_sticky_layer_shifting_sticky_box.Get();
    }
    return nullptr;
  }

  const LayoutBoxModelObject* NearestStickyLayerShiftingContainingBlock()
      const {
    if (const auto* data = PreferredAxisData()) {
      return data->nearest_sticky_layer_shifting_containing_block.Get();
    }
    return nullptr;
  }

 private:
  const PerAxisData* PreferredAxisData() const;

  // Safely builds a 2D rect from 1D ranges, falling back to 0 if an axis is
  // missing.
  PhysicalRect BuildRect(const BoxEdge* x, const BoxEdge* y) const {
    return PhysicalRect(x ? x->offset : LayoutUnit(),
                        y ? y->offset : LayoutUnit(),
                        x ? x->size : LayoutUnit(), y ? y->size : LayoutUnit());
  }

  PerAxisData* x_data_ = nullptr;
  PerAxisData* y_data_ = nullptr;
};

// Per-axis sticky constraints update payload.
struct CORE_EXPORT StickyConstraintsData {
  STACK_ALLOCATED();

 public:
  StickyPositionScrollingConstraints::PerAxisData* x_data = nullptr;
  StickyPositionScrollingConstraints::PerAxisData* y_data = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_STICKY_POSITION_SCROLLING_CONSTRAINTS_H_
