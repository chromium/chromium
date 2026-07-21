// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_evaluator_impl.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/layout/anchor_map.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid/layout_grid.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/grid_lanes_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid_lanes/layout_grid_lanes.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/core/style/position_area.h"

namespace blink {

namespace {

CSSAnchorValue PhysicalAnchorValueUsing(CSSAnchorValue x,
                                        CSSAnchorValue flipped_x,
                                        CSSAnchorValue y,
                                        CSSAnchorValue flipped_y,
                                        WritingDirectionMode writing_direction,
                                        bool is_y_axis) {
  if (is_y_axis)
    return writing_direction.IsFlippedY() ? flipped_y : y;
  return writing_direction.IsFlippedX() ? flipped_x : x;
}

// The logical <anchor-side> keywords map to one of the physical keywords
// depending on the property the function is being used in and the writing mode.
// https://drafts.csswg.org/css-anchor-1/#anchor-pos
CSSAnchorValue PhysicalAnchorValueFromLogicalOrAuto(
    CSSAnchorValue anchor_value,
    WritingDirectionMode writing_direction,
    WritingDirectionMode self_writing_direction,
    bool is_y_axis) {
  switch (anchor_value) {
    case CSSAnchorValue::kSelfStart:
      writing_direction = self_writing_direction;
      [[fallthrough]];
    case CSSAnchorValue::kStart:
      return PhysicalAnchorValueUsing(
          CSSAnchorValue::kLeft, CSSAnchorValue::kRight, CSSAnchorValue::kTop,
          CSSAnchorValue::kBottom, writing_direction, is_y_axis);
    case CSSAnchorValue::kSelfEnd:
      writing_direction = self_writing_direction;
      [[fallthrough]];
    case CSSAnchorValue::kEnd:
      return PhysicalAnchorValueUsing(
          CSSAnchorValue::kRight, CSSAnchorValue::kLeft,
          CSSAnchorValue::kBottom, CSSAnchorValue::kTop, writing_direction,
          is_y_axis);
    default:
      return anchor_value;
  }
}

// https://drafts.csswg.org/css-anchor-position-1/#valdef-anchor-inside
// https://drafts.csswg.org/css-anchor-position-1/#valdef-anchor-outside
CSSAnchorValue PhysicalAnchorValueFromInsideOutside(CSSAnchorValue anchor_value,
                                                    bool is_y_axis,
                                                    bool is_right_or_bottom) {
  switch (anchor_value) {
    case CSSAnchorValue::kInside: {
      if (is_y_axis) {
        return is_right_or_bottom ? CSSAnchorValue::kBottom
                                  : CSSAnchorValue::kTop;
      }
      return is_right_or_bottom ? CSSAnchorValue::kRight
                                : CSSAnchorValue::kLeft;
    }
    case CSSAnchorValue::kOutside: {
      if (is_y_axis) {
        return is_right_or_bottom ? CSSAnchorValue::kTop
                                  : CSSAnchorValue::kBottom;
      }
      return is_right_or_bottom ? CSSAnchorValue::kLeft
                                : CSSAnchorValue::kRight;
    }
    default:
      return anchor_value;
  }
}

// Resolve `anchor_value` (part of an anchor() function) for the given anchor
// rectangle. Returns `nullopt` if the query is invalid (due to wrong axis).
std::optional<LayoutUnit> ResolveAnchorValue(
    PhysicalRect anchor_rect,
    CSSAnchorValue anchor_value,
    float percentage,
    LayoutUnit available_size,
    WritingDirectionMode container_writing_direction,
    WritingDirectionMode self_writing_direction,
    const PhysicalOffset& offset_to_padding_box,
    bool is_y_axis,
    bool is_right_or_bottom) {
  // Make the offset relative to the padding box, because the containing block
  // is formed by the padding edge.
  // https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
  anchor_rect.offset -= offset_to_padding_box;

  anchor_value = PhysicalAnchorValueFromLogicalOrAuto(
      anchor_value, container_writing_direction, self_writing_direction,
      is_y_axis);
  anchor_value = PhysicalAnchorValueFromInsideOutside(anchor_value, is_y_axis,
                                                      is_right_or_bottom);
  LayoutUnit value;
  switch (anchor_value) {
    case CSSAnchorValue::kCenter: {
      const LayoutUnit start = is_y_axis ? anchor_rect.Y() : anchor_rect.X();
      const LayoutUnit end =
          is_y_axis ? anchor_rect.Bottom() : anchor_rect.Right();
      value = start + LayoutUnit::FromFloatRound((end - start) * 0.5);
      break;
    }
    case CSSAnchorValue::kLeft:
      if (is_y_axis) {
        return std::nullopt;  // Wrong axis.
      }
      value = anchor_rect.X();
      break;
    case CSSAnchorValue::kRight:
      if (is_y_axis) {
        return std::nullopt;  // Wrong axis.
      }
      value = anchor_rect.Right();
      break;
    case CSSAnchorValue::kTop:
      if (!is_y_axis) {
        return std::nullopt;  // Wrong axis.
      }
      value = anchor_rect.Y();
      break;
    case CSSAnchorValue::kBottom:
      if (!is_y_axis) {
        return std::nullopt;  // Wrong axis.
      }
      value = anchor_rect.Bottom();
      break;
    case CSSAnchorValue::kPercentage: {
      LayoutUnit size;
      if (is_y_axis) {
        value = anchor_rect.Y();
        size = anchor_rect.Height();
        // The percentage is logical, between the `start` and `end` sides.
        // Convert to the physical percentage.
        // https://drafts.csswg.org/css-anchor-1/#anchor-pos
        if (container_writing_direction.IsFlippedY()) {
          percentage = 100 - percentage;
        }
      } else {
        value = anchor_rect.X();
        size = anchor_rect.Width();
        // Convert the logical percentage to physical. See above.
        if (container_writing_direction.IsFlippedX()) {
          percentage = 100 - percentage;
        }
      }
      value += LayoutUnit::FromFloatRound(size * percentage / 100);
      break;
    }
    case CSSAnchorValue::kInside:
    case CSSAnchorValue::kOutside:
      // Should have been handled by `PhysicalAnchorValueFromInsideOutside`.
      [[fallthrough]];
    case CSSAnchorValue::kStart:
    case CSSAnchorValue::kEnd:
    case CSSAnchorValue::kSelfStart:
    case CSSAnchorValue::kSelfEnd:
      // These logical values should have been converted to corresponding
      // physical values in `PhysicalAnchorValueFromLogicalOrAuto`.
      NOTREACHED();
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_right_or_bottom) {
    return available_size - value;
  }
  return value;
}

// Resolve `anchor_size_value` (part of an anchor-size() function) for the given
// anchor size.
LayoutUnit ResolveAnchorSizeValue(const PhysicalSize& anchor_size,
                                  CSSAnchorSizeValue anchor_size_value,
                                  WritingMode container_writing_mode,
                                  WritingMode self_writing_mode) {
  LogicalSize logical_size = ToLogicalSize(anchor_size, container_writing_mode);

  switch (anchor_size_value) {
    case CSSAnchorSizeValue::kInline:
      return logical_size.inline_size;
    case CSSAnchorSizeValue::kBlock:
      return logical_size.block_size;
    case CSSAnchorSizeValue::kWidth:
      return anchor_size.width;
    case CSSAnchorSizeValue::kHeight:
      return anchor_size.height;
    case CSSAnchorSizeValue::kSelfInline:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? logical_size.inline_size
                 : logical_size.block_size;
    case CSSAnchorSizeValue::kSelfBlock:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? logical_size.block_size
                 : logical_size.inline_size;
    case CSSAnchorSizeValue::kImplicit:
      break;
  }
  NOTREACHED();
}

}  // namespace

std::optional<LayoutUnit> AnchorEvaluatorImpl::Evaluate(
    const AnchorQuery& anchor_query,
    const DefaultAnchorData& default_anchor_data,
    const std::optional<PositionAreaOffsets>& position_area_offsets) {
  switch (anchor_query.Type()) {
    case CSSAnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorSpecifier(),
                            anchor_query.AnchorSide(),
                            anchor_query.AnchorSidePercentageOrZero(),
                            default_anchor_data, position_area_offsets);
    case CSSAnchorQueryType::kAnchorSize:
      return EvaluateAnchorSize(anchor_query.AnchorSpecifier(),
                                anchor_query.AnchorSize(), default_anchor_data);
  }
}

const PhysicalAnchorReference* AnchorEvaluatorImpl::ResolveAnchorReference(
    const AnchorSpecifierValue& anchor_specifier,
    const DefaultAnchorData& default_anchor_data) const {
  if (!anchor_map_) {
    return nullptr;
  }

  if (anchor_specifier.IsNamed()) {
    return anchor_map_->AnchorReference(
        *query_box_, query_box_actual_containing_block_,
        ToAnchorScopedName(anchor_specifier.GetName(), *query_box_));
  }

  DCHECK(anchor_specifier.IsDefault());
  using Type = StylePositionAnchor::Type;
  switch (default_anchor_data.GetType()) {
    case Type::kNone:
      return nullptr;
    case Type::kAuto:
      if (!implicit_anchor_) {
        return nullptr;
      }
      return anchor_map_->AnchorReference(
          *query_box_, query_box_actual_containing_block_,
          To<Element>(implicit_anchor_->GetNode()));
    case Type::kName:
      return anchor_map_->AnchorReference(
          *query_box_, query_box_actual_containing_block_,
          ToAnchorScopedName(default_anchor_data.GetName(), *query_box_));
    case Type::kNormal:
      NOTREACHED();
  }
}

const LayoutObject* AnchorEvaluatorImpl::DefaultAnchor(
    const DefaultAnchorData& default_anchor_data) const {
  return cached_default_anchor_.Get(default_anchor_data, [&]() {
    const PhysicalAnchorReference* reference = ResolveAnchorReference(
        *AnchorSpecifierValue::Default(), default_anchor_data);
    return reference ? reference->GetLayoutObject() : nullptr;
  });
}

const PaintLayer* AnchorEvaluatorImpl::DefaultAnchorScrollContainerLayer(
    PhysicalAxis axis,
    const DefaultAnchorData& default_anchor_data) const {
  auto& cache = (axis == PhysicalAxis::kVertical)
                    ? cached_default_anchor_scroll_container_layer_y_
                    : cached_default_anchor_scroll_container_layer_x_;
  return cache.Get(default_anchor_data, [&]() {
    const auto* default_anchor = DefaultAnchor(default_anchor_data);
    return default_anchor ? default_anchor->ContainingScrollContainerLayer(
                                axis, /*ignore_layout_view_for_fixed_pos=*/true)
                          : nullptr;
  });
}

bool AnchorEvaluatorImpl::AllowAnchor() const {
  switch (GetMode()) {
    case Mode::kLeft:
    case Mode::kRight:
    case Mode::kTop:
    case Mode::kBottom:
      return true;
    case Mode::kNone:
    case Mode::kWidth:
    case Mode::kHeight:
      return false;
  }
}

bool AnchorEvaluatorImpl::AllowAnchorSize() const {
  switch (GetMode()) {
    case Mode::kWidth:
    case Mode::kHeight:
    case Mode::kLeft:
    case Mode::kRight:
    case Mode::kTop:
    case Mode::kBottom:
      return true;
    case Mode::kNone:
      return false;
  }
}

bool AnchorEvaluatorImpl::IsYAxis() const {
  return GetMode() == Mode::kTop || GetMode() == Mode::kBottom ||
         GetMode() == Mode::kHeight;
}

bool AnchorEvaluatorImpl::IsRightOrBottom() const {
  return GetMode() == Mode::kRight || GetMode() == Mode::kBottom;
}

bool AnchorEvaluatorImpl::ShouldUseScrollAdjustmentFor(
    const LayoutObject* anchor,
    PhysicalAxis axis,
    const DefaultAnchorData& default_anchor_data) const {
  if (!anchor) {
    return false;
  }
  if (anchor == DefaultAnchor(default_anchor_data)) {
    return true;
  }
  return anchor->ContainingScrollContainerLayer(
             axis, /*ignore_layout_view_for_fixed_pos=*/true) ==
         DefaultAnchorScrollContainerLayer(axis, default_anchor_data);
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchor(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorValue anchor_value,
    float percentage,
    const DefaultAnchorData& default_anchor_data,
    const std::optional<PositionAreaOffsets>& position_area_offsets) {
  if (!AllowAnchor()) {
    return std::nullopt;
  }

  const PhysicalAnchorReference* anchor_reference =
      ResolveAnchorForEvaluation(anchor_specifier, default_anchor_data);
  if (!anchor_reference) {
    return std::nullopt;
  }

  const bool has_default_anchor = DefaultAnchor(default_anchor_data);
  const PhysicalRect containing_block_rect =
      WritingModeConverter(container_writing_direction_, container_size_)
          .ToPhysical(AdjustedContainingBlockRect(position_area_offsets,
                                                  has_default_anchor));

  const bool is_y_axis = IsYAxis();
  const LayoutUnit axis_size = is_y_axis ? containing_block_rect.Height()
                                         : containing_block_rect.Width();

  const PhysicalRect anchor_rect =
      CalculateAnchorRectWithScrollOffset(*anchor_reference);
  if (std::optional<LayoutUnit> result = ResolveAnchorValue(
          anchor_rect, anchor_value, percentage, axis_size,
          container_writing_direction_,
          query_box_->StyleRef().GetWritingDirection(),
          containing_block_rect.offset, is_y_axis, IsRightOrBottom())) {
    bool& needs_scroll_adjustment = is_y_axis ? needs_scroll_adjustment_in_y_
                                              : needs_scroll_adjustment_in_x_;
    if (!needs_scroll_adjustment &&
        ShouldUseScrollAdjustmentFor(
            anchor_reference->GetLayoutObject(),
            is_y_axis ? PhysicalAxis::kVertical : PhysicalAxis::kHorizontal,
            default_anchor_data)) {
      needs_scroll_adjustment = true;
    }
    return result;
  }
  return std::nullopt;
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchorSize(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorSizeValue anchor_size_value,
    const DefaultAnchorData& default_anchor_data) {
  if (!AllowAnchorSize()) {
    return std::nullopt;
  }

  if (anchor_size_value == CSSAnchorSizeValue::kImplicit) {
    if (IsYAxis()) {
      anchor_size_value = CSSAnchorSizeValue::kHeight;
    } else {
      anchor_size_value = CSSAnchorSizeValue::kWidth;
    }
  }

  const PhysicalAnchorReference* anchor_reference =
      ResolveAnchorForEvaluation(anchor_specifier, default_anchor_data);
  if (!anchor_reference) {
    return std::nullopt;
  }

  PhysicalRect anchor_rect =
      CalculateAnchorRectWithScrollOffset(*anchor_reference);
  return ResolveAnchorSizeValue(anchor_rect.size, anchor_size_value,
                                container_writing_direction_.GetWritingMode(),
                                query_box_->StyleRef().GetWritingMode());
}

const PhysicalAnchorReference* AnchorEvaluatorImpl::ResolveAnchorForEvaluation(
    const AnchorSpecifierValue& anchor_specifier,
    const DefaultAnchorData& default_anchor_data) {
  const PhysicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, default_anchor_data);
  if (!anchor_reference) {
    return nullptr;
  }

  UpdateAccessibilityAnchor(anchor_reference->GetLayoutObject());

  if (anchor_reference->GetDisplayLocks()) {
    if (!display_locks_affected_by_anchors_) {
      display_locks_affected_by_anchors_ =
          MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>();
    }
    for (auto& display_lock : *anchor_reference->GetDisplayLocks()) {
      display_locks_affected_by_anchors_->insert(display_lock);
    }
  }

  did_resolve_anchor_with_running_transform_animation_ =
      did_resolve_anchor_with_running_transform_animation_ ||
      anchor_reference->HasRunningTransformAnimation();

  return anchor_reference;
}

PhysicalRect AnchorEvaluatorImpl::CalculateAnchorRectWithScrollOffset(
    const PhysicalAnchorReference& anchor_reference) {
  PhysicalRect result = anchor_reference.TransformedBoundingRect();

  // Update the anchor rect based on remembered (or current) scroll offsets.
  OutOfFlowData::ScrollOffsetPair scroll_offsets = [&]() {
    if (remembered_scroll_offsets_) {
      if (auto offsets = remembered_scroll_offsets_->GetOffsetsForAnchor(
              &anchor_reference.GetElement())) {
        return *offsets;
      }
    }

    if (used_scroll_offsets_) {
      if (auto offsets = used_scroll_offsets_->GetOffsetsForAnchor(
              &anchor_reference.GetElement())) {
        return *offsets;
      }
    }

    const Element* anchored_element = To<Element>(query_box_->GetNode());
    const LayoutObject* anchor_object = anchor_reference.GetLayoutObject();
    CHECK(anchored_element && anchor_object);

    const auto& adjustment_data =
        AnchorPositionScrollData::ComputeAdjustmentContainersData(
            anchored_element, *anchor_object);
    return OutOfFlowData::ScrollOffsetPair{
        .scroll_offset_for_layout = adjustment_data.accumulated_adjustment,
        .scroll_offset_for_range_adjustment =
            adjustment_data.accumulated_range_adjustment_offset};
  }();

  result.Move(-scroll_offsets.scroll_offset_for_layout);

  if (!used_scroll_offsets_) {
    used_scroll_offsets_ =
        MakeGarbageCollected<OutOfFlowData::RememberedScrollOffsets>();
  }
  used_scroll_offsets_->SetOffsetsForAnchor(&anchor_reference.GetElement(),
                                            scroll_offsets);
  return result;
}

void AnchorEvaluatorImpl::UpdateAccessibilityAnchor(
    const LayoutObject* anchor) {
  if (!anchor->GetDocument().ExistingAXObjectCache()) {
    return;
  }

  Element* anchor_element = To<Element>(anchor->GetNode());
  if (accessibility_anchor_ && accessibility_anchor_ != anchor_element) {
    has_multiple_accessibility_anchors_ = true;
  }
  accessibility_anchor_ = anchor_element;
}

Element* AnchorEvaluatorImpl::AccessibilityAnchor() const {
  if (has_multiple_accessibility_anchors_) {
    return nullptr;
  }
  return accessibility_anchor_;
}

void AnchorEvaluatorImpl::ClearAccessibilityAnchor() {
  accessibility_anchor_ = nullptr;
  has_multiple_accessibility_anchors_ = false;
}

std::optional<PhysicalOffset> AnchorEvaluatorImpl::ComputeAnchorCenterOffsets(
    const ComputedStyleBuilder& builder) {
  // Parameter `percentage` is unused for any non-percentage anchor value.
  const double dummy_percentage = 0;

  // Do not let the pre-computation of anchor-center offsets mark for needing
  // scroll adjustments. It is not known at this point if anchor-center will be
  // used at all, and allowing this marking could cause unnecessary work and
  // paint invalidations.
  base::AutoReset<bool> reset_adjust_x(&needs_scroll_adjustment_in_x_, true);
  base::AutoReset<bool> reset_adjust_y(&needs_scroll_adjustment_in_y_, true);
  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> left;
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    top = EvaluateAnchor(*AnchorSpecifierValue::Default(),
                         CSSAnchorValue::kCenter, dummy_percentage,
                         builder.GetDefaultAnchorData(),
                         builder.PositionAreaOffsets());
  }
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left = EvaluateAnchor(*AnchorSpecifierValue::Default(),
                          CSSAnchorValue::kCenter, dummy_percentage,
                          builder.GetDefaultAnchorData(),
                          builder.PositionAreaOffsets());
  }
  CHECK(top.has_value() == left.has_value());
  if (top.has_value()) {
    return PhysicalOffset(left.value(), top.value());
  }
  return std::nullopt;
}

std::optional<PositionAreaOffsets>
AnchorEvaluatorImpl::ComputePositionAreaOffsetsForLayout(
    const DefaultAnchorData& default_anchor_data) {
  const PositionArea& position_area = default_anchor_data.GetPositionArea();
  CHECK(!position_area.IsNone());

  if (!DefaultAnchor(default_anchor_data)) {
    return std::nullopt;
  }
  const PositionArea physical_position_area =
      position_area.ToPhysical(container_writing_direction_,
                               query_box_->StyleRef().GetWritingDirection());
  CHECK(!position_area.ContainsAny())
      << "The 'any' keyword can only be used for anchored(fallback) container "
         "queries";

  PhysicalBoxStrut offsets;
  PhysicalBoxSides behaves_as_auto;

  offsets.top = ([&]() -> LayoutUnit {
    const PositionAreaRegion area = physical_position_area.FirstStart();
    if (area == PositionAreaRegion::kNone) {
      return LayoutUnit();
    }
    behaves_as_auto.top = (area == PositionAreaRegion::kTop);
    const CSSAnchorValue side = (area == PositionAreaRegion::kBottom)
                                    ? CSSAnchorValue::kBottom
                                    : CSSAnchorValue::kTop;
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    if (const std::optional<LayoutUnit> value = EvaluateAnchor(
            *AnchorSpecifierValue::Default(), side, /*percentage=*/0.f,
            default_anchor_data, std::nullopt)) {
      return (area == PositionAreaRegion::kTop && *value > LayoutUnit())
                 ? LayoutUnit()
                 : *value;
    }
    return LayoutUnit();
  })();

  offsets.bottom = ([&]() -> LayoutUnit {
    const PositionAreaRegion area = physical_position_area.FirstEnd();
    if (area == PositionAreaRegion::kNone) {
      return LayoutUnit();
    }
    behaves_as_auto.bottom = (area == PositionAreaRegion::kBottom);
    const CSSAnchorValue side = (area == PositionAreaRegion::kTop)
                                    ? CSSAnchorValue::kTop
                                    : CSSAnchorValue::kBottom;
    AnchorScope anchor_scope(AnchorScope::Mode::kBottom, this);
    if (const std::optional<LayoutUnit> value = EvaluateAnchor(
            *AnchorSpecifierValue::Default(), side, /*percentage=*/0.f,
            default_anchor_data, std::nullopt)) {
      return (area == PositionAreaRegion::kBottom && *value > LayoutUnit())
                 ? LayoutUnit()
                 : *value;
    }
    return LayoutUnit();
  })();

  offsets.left = ([&]() -> LayoutUnit {
    const PositionAreaRegion area = physical_position_area.SecondStart();
    if (area == PositionAreaRegion::kNone) {
      return LayoutUnit();
    }
    behaves_as_auto.left = (area == PositionAreaRegion::kLeft);
    const CSSAnchorValue side = (area == PositionAreaRegion::kRight)
                                    ? CSSAnchorValue::kRight
                                    : CSSAnchorValue::kLeft;
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    if (const std::optional<LayoutUnit> value = EvaluateAnchor(
            *AnchorSpecifierValue::Default(), side, /*percentage=*/0.f,
            default_anchor_data, std::nullopt)) {
      return (area == PositionAreaRegion::kLeft && *value > LayoutUnit())
                 ? LayoutUnit()
                 : *value;
    }
    return LayoutUnit();
  })();

  offsets.right = ([&]() -> LayoutUnit {
    const PositionAreaRegion area = physical_position_area.SecondEnd();
    if (area == PositionAreaRegion::kNone) {
      return LayoutUnit();
    }
    behaves_as_auto.right = (area == PositionAreaRegion::kRight);
    const CSSAnchorValue side = (area == PositionAreaRegion::kLeft)
                                    ? CSSAnchorValue::kLeft
                                    : CSSAnchorValue::kRight;
    AnchorScope anchor_scope(AnchorScope::Mode::kRight, this);
    if (const std::optional<LayoutUnit> value = EvaluateAnchor(
            *AnchorSpecifierValue::Default(), side, /*percentage=*/0.f,
            default_anchor_data, std::nullopt)) {
      return (area == PositionAreaRegion::kRight && *value > LayoutUnit())
                 ? LayoutUnit()
                 : *value;
    }
    return LayoutUnit();
  })();

  return PositionAreaOffsets(offsets, behaves_as_auto);
}

LogicalRect AnchorEvaluatorImpl::AdjustedContainingBlockRect(
    const std::optional<PositionAreaOffsets>& position_area_offsets,
    bool has_default_anchor) const {
  LogicalRect rect =
      has_default_anchor && scroll_rect_ ? *scroll_rect_ : container_rect_;

  // Apply the grid/grid-lanes area if needed.
  const ComputedStyle& style = containing_block_->StyleRef();
  if (const auto* grid = DynamicTo<LayoutGrid>(containing_block_)) {
    rect = GridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
        grid->CachedPlacementData(),
        grid_layout_data_ ? *grid_layout_data_ : *grid->LayoutData(), style,
        rect,
        MakeGarbageCollected<GridItemData>(
            BlockNode(const_cast<LayoutBox*>(query_box_)), style));
  } else if (const auto* grid_lanes =
                 DynamicTo<LayoutGridLanes>(containing_block_)) {
    rect = GridLanesLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
        grid_lanes->CachedPlacementData(),
        grid_layout_data_ ? *grid_layout_data_ : *grid_lanes->LayoutData(),
        style, rect,
        MakeGarbageCollected<GridItemData>(
            BlockNode(const_cast<LayoutBox*>(query_box_)), style));
  }

  // If present, reduce the containing-block rect based on the position-area.
  if (position_area_offsets) {
    rect.Contract(position_area_offsets->insets.ConvertToLogical(
        container_writing_direction_));
  }

  DCHECK_GE(rect.size.inline_size, LayoutUnit());
  DCHECK_GE(rect.size.block_size, LayoutUnit());
  return rect;
}

}  // namespace blink
