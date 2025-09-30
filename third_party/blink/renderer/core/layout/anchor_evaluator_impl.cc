// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_evaluator_impl.h"

#include <variant>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/transform_utils.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/core/style/position_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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

void PhysicalAnchorReference::InsertInReverseTreeOrderInto(
    Member<PhysicalAnchorReference>* head_ptr) {
  for (;;) {
    PhysicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->GetLayoutObject());
    DCHECK(GetLayoutObject());
    if (!head ||
        head->GetLayoutObject()->IsBeforeInPreOrder(*GetLayoutObject())) {
      next = head;
      *head_ptr = this;
      break;
    }

    head_ptr = &head->next;
  }
}

const PhysicalAnchorReference* PhysicalAnchorQuery::AnchorReference(
    const LayoutBox& query_box,
    const LayoutObject* query_box_actual_containing_block,
    const AnchorKey& key) const {
  const PhysicalAnchorReference* reference = Base::GetAnchorReference(key);
  if (!reference) {
    return nullptr;
  }
  for (const PhysicalAnchorReference* result = reference; result;
       result = result->next) {
    const LayoutObject* layout_object = result->GetLayoutObject();
    // TODO(crbug.com/384523570): If the layout object has been detached, we
    // really shouldn't be here.
    if (!layout_object || layout_object == &query_box ||
        (result->is_out_of_flow &&
         !layout_object->IsBeforeInPreOrder(query_box))) {
      continue;
    }

    // If an actual containing block has been specified, it means that we may
    // have found an anchor that isn't acceptable, due to an inconsistency
    // between the actual (CSS) containing block chain, and the physical
    // fragment tree structure. This happens for OOFs in block fragmentation.
    if (query_box_actual_containing_block &&
        !layout_object->Container()->IsContainedBy(
            query_box_actual_containing_block)) {
      continue;
    }

    return result;
  }
  return nullptr;
}

const LayoutObject* PhysicalAnchorQuery::AnchorLayoutObject(
    const LayoutBox& query_box,
    const AnchorKey& key) const {
  if (const PhysicalAnchorReference* reference = AnchorReference(
          query_box, /*query_box_actual_containing_block=*/nullptr, key)) {
    return reference->GetLayoutObject();
  }
  return nullptr;
}

void PhysicalAnchorQuery::Set(const AnchorKey& key,
                              const LayoutObject& layout_object,
                              const TransformState& transform_state,
                              const PhysicalRect& rect_without_transforms,
                              SetOptions options,
                              Element* element_for_display_lock) {
  GCedHeapHashSet<Member<Element>>* display_locks = nullptr;
  if (element_for_display_lock) {
    display_locks = MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>();
    display_locks->insert(element_for_display_lock);
  }
  Set(key, MakeGarbageCollected<PhysicalAnchorReference>(
               *To<Element>(layout_object.GetNode()), transform_state,
               rect_without_transforms, options == SetOptions::kOutOfFlow,
               display_locks));
}

void PhysicalAnchorQuery::Set(const AnchorKey& key,
                              PhysicalAnchorReference* reference) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = Base::insert(key, reference);
  if (result.is_new_entry)
    return;

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<PhysicalAnchorReference>* const existing_head_ptr =
      result.stored_value;
  DCHECK(*existing_head_ptr);
  DCHECK(reference->GetLayoutObject());
  for (PhysicalAnchorReference* existing = *existing_head_ptr; existing;
       existing = existing->next) {
    DCHECK(existing->GetLayoutObject());
    if (existing->GetLayoutObject() == reference->GetLayoutObject()) {
      existing->rect_without_transforms.Unite(
          reference->rect_without_transforms);

      gfx::RectF rect = existing->transform_state.MappedQuad().BoundingBox();
      rect.Union(reference->transform_state.MappedQuad().BoundingBox());
      existing->transform_state =
          TransformState(TransformState::kApplyTransformDirection,
                         gfx::QuadF(gfx::RectF(rect)));
      return;
    }
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the reverse tree order.
  reference->InsertInReverseTreeOrderInto(existing_head_ptr);
}

void PhysicalAnchorQuery::SetFromChild(
    const PhysicalAnchorQuery& physical_query,
    const PhysicalFragment& child_fragment,
    PhysicalOffset additional_offset,
    const LayoutObject& container_object,
    PhysicalSize container_size,
    SetOptions options,
    Element* element_for_display_lock) {
  for (auto entry : physical_query) {
    // For each key, only the last reference in tree order is reachable
    // under normal circumstances. However, the presence of anchor-scope
    // can make it necessary to skip past any number of references to reach
    // an earlier one. Therefore, all references must be propagated.
    //
    // See also InSameAnchorScope.
    for (PhysicalAnchorReference* reference = entry.value; reference;
         reference = reference->next) {
      PhysicalRect rect_without_transforms = reference->rect_without_transforms;
      rect_without_transforms.offset += additional_offset;

      TransformState transform_state(reference->transform_state);
      UpdateTransformState(child_fragment, additional_offset, container_object,
                           container_size, &transform_state);

      GCedHeapHashSet<Member<Element>>* display_locks = nullptr;
      if (reference->display_locks || element_for_display_lock) {
        display_locks =
            MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>();
      }
      if (reference->display_locks) {
        *display_locks = *reference->display_locks;
      }
      if (element_for_display_lock) {
        display_locks->insert(element_for_display_lock);
      }
      DCHECK(reference->GetLayoutObject());
      Set(entry.key,
          MakeGarbageCollected<PhysicalAnchorReference>(
              *reference->element, transform_state, rect_without_transforms,
              options == SetOptions::kOutOfFlow, display_locks));
    }
  }
}

const PhysicalAnchorQuery* AnchorEvaluatorImpl::AnchorQuery() const {
  // TODO(crbug.com/436305267): Remove these two members when
  // StitchedAnchorQueries is removed.
  DCHECK((!anchor_queries_ && !containing_block_) ||
         !RuntimeEnabledFeatures::CSSAnchorSimplifiedFragmentationEnabled());

  if (anchor_query_)
    return anchor_query_;
  if (anchor_queries_) {
    DCHECK(containing_block_);
    anchor_query_ = anchor_queries_->AnchorQuery(*containing_block_);
    if (!anchor_query_) {
      // The above operation is expensive. If there were no anchors for the
      // containing block, make sure that we don't try again every time this
      // function is called.
      anchor_queries_ = nullptr;
    }
    return anchor_query_;
  }
  return nullptr;
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::Evaluate(
    const class AnchorQuery& anchor_query,
    const ScopedCSSName* position_anchor,
    const std::optional<PositionAreaOffsets>& position_area_offsets) {
  switch (anchor_query.Type()) {
    case CSSAnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorSpecifier(),
                            anchor_query.AnchorSide(),
                            anchor_query.AnchorSidePercentageOrZero(),
                            position_anchor, position_area_offsets);
    case CSSAnchorQueryType::kAnchorSize:
      return EvaluateAnchorSize(anchor_query.AnchorSpecifier(),
                                anchor_query.AnchorSize(), position_anchor);
  }
}

const PhysicalAnchorReference* AnchorEvaluatorImpl::ResolveAnchorReference(
    const AnchorSpecifierValue& anchor_specifier,
    const ScopedCSSName* position_anchor) const {
  if (!anchor_specifier.IsNamed() && !position_anchor && !implicit_anchor_) {
    return nullptr;
  }
  const PhysicalAnchorQuery* anchor_query = AnchorQuery();
  if (!anchor_query) {
    return nullptr;
  }
  DCHECK(RuntimeEnabledFeatures::CSSAnchorSimplifiedFragmentationEnabled() ||
         !query_box_actual_containing_block_);
  if (anchor_specifier.IsNamed()) {
    return anchor_query->AnchorReference(
        *query_box_, query_box_actual_containing_block_,
        ToAnchorScopedName(anchor_specifier.GetName(), *query_box_));
  }
  if (anchor_specifier.IsDefault() && position_anchor) {
    return anchor_query->AnchorReference(
        *query_box_, query_box_actual_containing_block_,
        ToAnchorScopedName(*position_anchor, *query_box_));
  }
  return anchor_query->AnchorReference(
      *query_box_, query_box_actual_containing_block_,
      To<Element>(implicit_anchor_->GetNode()));
}

const LayoutObject* AnchorEvaluatorImpl::DefaultAnchor(
    const ScopedCSSName* position_anchor) const {
  return cached_default_anchor_.Get(position_anchor, [&]() {
    const PhysicalAnchorReference* reference = ResolveAnchorReference(
        *AnchorSpecifierValue::Default(), position_anchor);
    return reference ? reference->GetLayoutObject() : nullptr;
  });
}

const PaintLayer* AnchorEvaluatorImpl::DefaultAnchorScrollContainerLayer(
    const ScopedCSSName* position_anchor) const {
  return cached_default_anchor_scroll_container_layer_.Get(
      position_anchor, [&]() {
        return DefaultAnchor(position_anchor)
            ->ContainingScrollContainerLayer(
                true /*ignore_layout_view_for_fixed_pos*/);
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
    const ScopedCSSName* position_anchor) const {
  if (!DefaultAnchor(position_anchor)) {
    return false;
  }
  if (anchor == DefaultAnchor(position_anchor)) {
    return true;
  }
  return anchor->ContainingScrollContainerLayer(
             true /*ignore_layout_view_for_fixed_pos*/) ==
         DefaultAnchorScrollContainerLayer(position_anchor);
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchor(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorValue anchor_value,
    float percentage,
    const ScopedCSSName* position_anchor,
    const std::optional<PositionAreaOffsets>& position_area_offsets) const {
  if (!AllowAnchor()) {
    return std::nullopt;
  }

  const PhysicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  UpdateAccessibilityAnchor(anchor_reference->GetLayoutObject());

  if (anchor_reference->display_locks) {
    for (auto& display_lock : *anchor_reference->display_locks) {
      display_locks_affected_by_anchors_->insert(display_lock);
    }
  }

  const bool has_default_anchor = DefaultAnchor(position_anchor);
  const PhysicalRect position_area_modified_containing_block_rect =
      PositionAreaModifiedContainingBlock(position_area_offsets,
                                          has_default_anchor);

  const bool is_y_axis = IsYAxis();

  PhysicalRect anchor_rect = GetAnchorRect(*anchor_reference, position_anchor);
  if (std::optional<LayoutUnit> result = ResolveAnchorValue(
          anchor_rect, anchor_value, percentage,
          AvailableSizeAlongAxis(position_area_modified_containing_block_rect),
          container_writing_direction_,
          query_box_->StyleRef().GetWritingDirection(),
          position_area_modified_containing_block_rect.offset, is_y_axis,
          IsRightOrBottom())) {
    bool& needs_scroll_adjustment = is_y_axis ? needs_scroll_adjustment_in_y_
                                              : needs_scroll_adjustment_in_x_;
    if (!needs_scroll_adjustment &&
        ShouldUseScrollAdjustmentFor(anchor_reference->GetLayoutObject(),
                                     position_anchor)) {
      needs_scroll_adjustment = true;
    }
    return result;
  }
  return std::nullopt;
}

std::optional<LayoutUnit> AnchorEvaluatorImpl::EvaluateAnchorSize(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorSizeValue anchor_size_value,
    const ScopedCSSName* position_anchor) const {
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
      ResolveAnchorReference(anchor_specifier, position_anchor);
  if (!anchor_reference) {
    return std::nullopt;
  }

  UpdateAccessibilityAnchor(anchor_reference->GetLayoutObject());

  if (anchor_reference->display_locks) {
    for (auto& display_lock : *anchor_reference->display_locks) {
      display_locks_affected_by_anchors_->insert(display_lock);
    }
  }

  PhysicalSize anchor_size =
      GetAnchorRect(*anchor_reference, position_anchor).size;
  return ResolveAnchorSizeValue(anchor_size, anchor_size_value,
                                container_writing_direction_.GetWritingMode(),
                                query_box_->StyleRef().GetWritingMode());
}

PhysicalRect AnchorEvaluatorImpl::GetAnchorRect(
    const PhysicalAnchorReference& anchor_reference,
    const ScopedCSSName* position_anchor) const {
  PhysicalRect result;
  if (RuntimeEnabledFeatures::CSSAnchorWithTransformsEnabled()) {
    result = anchor_reference.TransformedBoundingRect();
  } else {
    result = anchor_reference.RectWithoutTransforms();
  }

  if (!RuntimeEnabledFeatures::CSSAnchorUpdateEnabled()) {
    return result;
  }

  // Update the anchor rect based on remembered (or current) scroll offsets.
  PhysicalOffset scroll_offset = [&]() {
    if (remembered_scroll_offsets_) {
      if (auto offset = remembered_scroll_offsets_->GetOffsetForAnchor(
              anchor_reference.element)) {
        return *offset;
      }
    }

    if (used_scroll_offsets_) {
      if (auto offset = used_scroll_offsets_->GetOffsetForAnchor(
              anchor_reference.element)) {
        return *offset;
      }
    }

    Element* anchored_element = To<Element>(query_box_->GetNode());
    LayoutObject* anchor_object = anchor_reference.element->GetLayoutObject();
    CHECK(anchored_element && anchor_object);

    return AnchorPositionScrollData::ComputeAdjustmentContainersData(
               anchored_element, *anchor_object)
        .accumulated_adjustment;
  }();

  result.Move(-scroll_offset);

  if (!used_scroll_offsets_) {
    used_scroll_offsets_ =
        MakeGarbageCollected<OutOfFlowData::RememberedScrollOffsets>();
  }
  used_scroll_offsets_->SetOffsetForAnchor(anchor_reference.element,
                                           scroll_offset);
  return result;
}

void AnchorEvaluatorImpl::UpdateAccessibilityAnchor(
    const LayoutObject* anchor) const {
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
    top =
        EvaluateAnchor(*AnchorSpecifierValue::Default(),
                       CSSAnchorValue::kCenter, dummy_percentage,
                       builder.PositionAnchor(), builder.PositionAreaOffsets());
  }
  {
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    left =
        EvaluateAnchor(*AnchorSpecifierValue::Default(),
                       CSSAnchorValue::kCenter, dummy_percentage,
                       builder.PositionAnchor(), builder.PositionAreaOffsets());
  }
  CHECK(top.has_value() == left.has_value());
  if (top.has_value()) {
    return PhysicalOffset(left.value(), top.value());
  }
  return std::nullopt;
}

std::optional<PositionAreaOffsets>
AnchorEvaluatorImpl::ComputePositionAreaOffsetsForLayout(
    const ScopedCSSName* position_anchor,
    PositionArea position_area) {
  CHECK(!position_area.IsNone());

  if (!DefaultAnchor(position_anchor)) {
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
    const blink::AnchorQuery query = (area == PositionAreaRegion::kBottom)
                                         ? PositionArea::AnchorBottom()
                                         : PositionArea::AnchorTop();
    AnchorScope anchor_scope(AnchorScope::Mode::kTop, this);
    if (const std::optional<LayoutUnit> value =
            Evaluate(query, position_anchor, std::nullopt)) {
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
    const blink::AnchorQuery query = (area == PositionAreaRegion::kTop)
                                         ? PositionArea::AnchorTop()
                                         : PositionArea::AnchorBottom();
    AnchorScope anchor_scope(AnchorScope::Mode::kBottom, this);
    if (const std::optional<LayoutUnit> value =
            Evaluate(query, position_anchor, std::nullopt)) {
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
    const blink::AnchorQuery query = (area == PositionAreaRegion::kRight)
                                         ? PositionArea::AnchorRight()
                                         : PositionArea::AnchorLeft();
    AnchorScope anchor_scope(AnchorScope::Mode::kLeft, this);
    if (const std::optional<LayoutUnit> value =
            Evaluate(query, position_anchor, std::nullopt)) {
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
    const blink::AnchorQuery query = (area == PositionAreaRegion::kLeft)
                                         ? PositionArea::AnchorLeft()
                                         : PositionArea::AnchorRight();
    AnchorScope anchor_scope(AnchorScope::Mode::kRight, this);
    if (const std::optional<LayoutUnit> value =
            Evaluate(query, position_anchor, std::nullopt)) {
      return (area == PositionAreaRegion::kRight && *value > LayoutUnit())
                 ? LayoutUnit()
                 : *value;
    }
    return LayoutUnit();
  })();

  return PositionAreaOffsets(offsets, behaves_as_auto);
}

PhysicalRect AnchorEvaluatorImpl::PositionAreaModifiedContainingBlock(
    const std::optional<PositionAreaOffsets>& position_area_offsets,
    bool has_default_anchor) const {
  PhysicalRect rect =
      has_default_anchor && scroll_rect_ ? *scroll_rect_ : container_rect_;

  // If calculated, reduce the containing-block rect based on the position-area.
  if (position_area_offsets) {
    rect.Contract(position_area_offsets->insets);
  }

  DCHECK_GE(rect.size.width, LayoutUnit());
  DCHECK_GE(rect.size.height, LayoutUnit());
  return rect;
}

void PhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(element);
  visitor->Trace(next);
  visitor->Trace(display_locks);
}

}  // namespace blink
