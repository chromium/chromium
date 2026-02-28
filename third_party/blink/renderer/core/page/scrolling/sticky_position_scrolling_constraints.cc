// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"

#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {
BoxEdge RectToBoxEdge(PhysicalAxis axis, const PhysicalRect& rect) {
  return axis == PhysicalAxis::kHorizontal ? BoxEdge(rect.X(), rect.Width())
                                           : BoxEdge(rect.Y(), rect.Height());
}
}  // namespace

StickyPositionScrollingConstraints::PerAxisData::PerAxisData(
    PhysicalAxis axis,
    const PhysicalRect& containing_block,
    const PhysicalRect& sticky_box,
    const PhysicalRect& constraining,
    const LayoutBoxModelObject* nearest_sticky_layer_shifting_sticky_box,
    const LayoutBoxModelObject* nearest_sticky_layer_shifting_containing_block,
    const PaintLayer* containing_scroll_container_layer,
    bool is_fixed_to_view,
    std::optional<LayoutUnit> min_inset,
    std::optional<LayoutUnit> max_inset)
    : axis(axis),
      min_inset(min_inset),
      max_inset(max_inset),
      scroll_container_relative_containing_block_range(
          RectToBoxEdge(axis, containing_block)),
      scroll_container_relative_sticky_box_range(
          RectToBoxEdge(axis, sticky_box)),
      constraining_range(RectToBoxEdge(axis, constraining)),
      nearest_sticky_layer_shifting_sticky_box(
          nearest_sticky_layer_shifting_sticky_box),
      nearest_sticky_layer_shifting_containing_block(
          nearest_sticky_layer_shifting_containing_block),
      containing_scroll_container_layer(containing_scroll_container_layer),
      is_fixed_to_view(is_fixed_to_view) {}

void StickyPositionScrollingConstraints::PerAxisData::ComputeOffset(
    float scroll_position) {
  BoxEdge sticky_box_range = scroll_container_relative_sticky_box_range;
  BoxEdge containing_block_range =
      scroll_container_relative_containing_block_range;
  LayoutUnit ancestor_sticky_box_offset = AncestorStickyBoxOffset();
  LayoutUnit ancestor_containing_block_offset = AncestorContainingBlockOffset();
  // Adjust the cached rect locations for any sticky ancestor elements. The
  // sticky offset applied to those ancestors affects us as follows:
  //
  //   1. |nearest_sticky_layer_shifting_sticky_box| is a sticky layer between
  //      ourselves and our containing block, e.g. a nested inline parent.
  //      It shifts only the sticky_box_rect and not the containing_block_rect.
  //   2. |nearest_sticky_layer_shifting_containing_block| is a sticky layer
  //      between our containing block (inclusive) and our scroll ancestor
  //      (exclusive). As such, it shifts both the sticky_box_rect and the
  //      containing_block_rect.
  //
  // Note that this calculation assumes that |ComputeStickyOffset| is being
  // called top down, e.g. it has been called on any ancestors we have before
  // being called on us.
  sticky_box_range.Move(ancestor_sticky_box_offset +
                        ancestor_containing_block_offset);
  containing_block_range.Move(ancestor_containing_block_offset);

  // We now attempt to shift sticky_box_rect to obey the specified sticky
  // constraints, whilst always staying within our containing block. This
  // shifting produces the final sticky offset below.
  //
  // As per the spec, 'left' overrides 'right' and 'top' overrides 'bottom'.
  BoxEdge box_range = sticky_box_range;

  BoxEdge content_box_range = constraining_range;
  // If the sticky object is fixed to view, it doesn't scroll, so ignore
  // scroll_position.
  if (!is_fixed_to_view) {
    content_box_range.Move(LayoutUnit::FromFloatFloor(scroll_position));
  }
  if (max_inset) {
    LayoutUnit limit = content_box_range.End() - *max_inset;
    LayoutUnit delta = limit - sticky_box_range.End();
    LayoutUnit available_space =
        containing_block_range.offset - sticky_box_range.offset;

    delta = delta.ClampPositiveToZero();
    available_space = available_space.ClampPositiveToZero();

    if (delta < available_space) {
      delta = available_space;
    }

    box_range.Move(delta);
  }

  if (min_inset) {
    LayoutUnit limit = content_box_range.offset + *min_inset;
    LayoutUnit delta = limit - sticky_box_range.offset;
    LayoutUnit available_space =
        containing_block_range.End() - sticky_box_range.End();

    delta = delta.ClampNegativeToZero();
    available_space = available_space.ClampNegativeToZero();

    if (delta > available_space) {
      delta = available_space;
    }

    box_range.Move(delta);
  }

  sticky_offset = box_range.offset - sticky_box_range.offset;

  // Now that we have computed our current sticky offset, update the cached
  // accumulated sticky offsets.
  total_sticky_box_sticky_offset = ancestor_sticky_box_offset + sticky_offset;
  total_containing_block_sticky_offset = ancestor_sticky_box_offset +
                                         ancestor_containing_block_offset +
                                         sticky_offset;
}

void StickyPositionScrollingConstraints::ComputeStickyOffset(
    const gfx::PointF& scroll_position,
    PhysicalAxes scroll_axes) {
  if (x_data_ && (scroll_axes & kPhysicalAxesHorizontal)) {
    x_data_->ComputeOffset(scroll_position.x());
  }

  if (y_data_ && (scroll_axes & kPhysicalAxesVertical)) {
    y_data_->ComputeOffset(scroll_position.y());
  }
}

bool StickyPositionScrollingConstraints::HasScrollDependentOffset() const {
  auto is_axis_scroll_dependent = [](const PerAxisData* axis_data) {
    // A sticky element without explicit edge constraints (e.g. `top: auto`)
    // behaves identically to `position: relative`. This optimization prevents
    // promoting these elements to independent compositor layers.
    if (!axis_data || (!axis_data->min_inset && !axis_data->max_inset) ||
        axis_data->is_fixed_to_view) {
      return false;
    }
    const auto* layer = axis_data->containing_scroll_container_layer.Get();
    return layer && layer->GetScrollableArea() &&
           layer->GetScrollableArea()->HasOverflow();
  };

  return is_axis_scroll_dependent(x_data_) || is_axis_scroll_dependent(y_data_);
}

PhysicalOffset StickyPositionScrollingConstraints::StickyOffset() const {
  return PhysicalOffset(x_data_ ? x_data_->sticky_offset : LayoutUnit(),
                        y_data_ ? y_data_->sticky_offset : LayoutUnit());
}

void StickyPositionScrollingConstraints::PerAxisData::Trace(
    Visitor* visitor) const {
  visitor->Trace(nearest_sticky_layer_shifting_sticky_box);
  visitor->Trace(nearest_sticky_layer_shifting_containing_block);
  visitor->Trace(containing_scroll_container_layer);
}

LayoutUnit
StickyPositionScrollingConstraints::PerAxisData::AncestorStickyBoxOffset()
    const {
  if (!nearest_sticky_layer_shifting_sticky_box) {
    return LayoutUnit();
  }
  const auto constraints =
      nearest_sticky_layer_shifting_sticky_box->StickyConstraints();
  DCHECK(constraints);

  if (const auto* ancestor_data = constraints.AxisData(axis)) {
    return ancestor_data->total_sticky_box_sticky_offset;
  }
  return LayoutUnit();
}

LayoutUnit
StickyPositionScrollingConstraints::PerAxisData::AncestorContainingBlockOffset()
    const {
  if (!nearest_sticky_layer_shifting_containing_block) {
    return LayoutUnit();
  }
  const auto constraints =
      nearest_sticky_layer_shifting_containing_block->StickyConstraints();
  DCHECK(constraints);

  if (const auto* ancestor_data = constraints.AxisData(axis)) {
    return ancestor_data->total_containing_block_sticky_offset;
  }
  return LayoutUnit();
}

const StickyPositionScrollingConstraints::PerAxisData*
StickyPositionScrollingConstraints::PreferredAxisData() const {
  return x_data_ ? x_data_ : y_data_;
}

}  // namespace blink
