// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/ng/ng_logical_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"

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
CSSAnchorValue PhysicalAnchorValueFromLogical(
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

}  // namespace

NGPhysicalAnchorReference::NGPhysicalAnchorReference(
    const NGLogicalAnchorReference& logical_reference,
    const WritingModeConverter& converter)
    : rect(converter.ToPhysical(logical_reference.rect)),
      fragment(logical_reference.fragment),
      is_invalid(logical_reference.is_invalid) {}

void NGLogicalAnchorReference::InsertInPreOrderInto(
    Member<NGLogicalAnchorReference>* head_ptr) {
  const LayoutObject* const object = fragment->GetLayoutObject();
  for (;;) {
    NGLogicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->fragment->GetLayoutObject());
    if (!head ||
        object->IsBeforeInPreOrder(*head->fragment->GetLayoutObject())) {
      next = head;
      *head_ptr = this;
      break;
    }

    // Skip adding if there is a reference with the same validity status and is
    // before in the tree order. Only the first one in the tree order is needed
    // for each validity status.
    if (is_invalid == head->is_invalid)
      break;

    head_ptr = &head->next;
  }
}

// static
const NGLogicalAnchorQuery& NGLogicalAnchorQuery::Empty() {
  DEFINE_STATIC_LOCAL(Persistent<NGLogicalAnchorQuery>, empty,
                      (MakeGarbageCollected<NGLogicalAnchorQuery>()));
  return *empty;
}

// static
const NGPhysicalAnchorQuery* NGPhysicalAnchorQuery::GetFromLayoutResult(
    const LayoutObject& layout_object) {
  if (!layout_object.IsOutOfFlowPositioned()) {
    return nullptr;
  }
  LayoutBox::NGPhysicalFragmentList containing_block_fragments =
      layout_object.ContainingBlock()->PhysicalFragments();
  if (containing_block_fragments.IsEmpty()) {
    return nullptr;
  }
  // TODO(crbug.com/1309178): Make it work when the containing block is
  // fragmented or inline.
  return containing_block_fragments.front().AnchorQuery();
}

// static
NGAnchorEvaluatorImpl NGAnchorEvaluatorImpl::BuildFromLayoutResult(
    const LayoutObject& layout_object) {
  const NGPhysicalAnchorQuery* physical_query =
      NGPhysicalAnchorQuery::GetFromLayoutResult(layout_object);
  if (!physical_query) {
    return NGAnchorEvaluatorImpl();
  }

  // TODO(crbug.com/1309178): Make it work when the containing block is
  // fragmented or inline.

  DCHECK(layout_object.IsOutOfFlowPositioned());
  DCHECK(layout_object.ContainingBlock());
  const LayoutBlock* container = layout_object.ContainingBlock();
  PhysicalSize container_size = container->PhysicalFragments().front().Size();
  WritingModeConverter container_converter(
      container->StyleRef().GetWritingDirection(), container_size);

  // TODO(crbug.com/1423493): The following doesn't support top-layer
  // |layout_object| well. We need to include & filter "invalid" anchors.

  NGLogicalAnchorQuery* logical_query =
      MakeGarbageCollected<NGLogicalAnchorQuery>();
  logical_query->SetFromPhysical(
      *physical_query, container_converter,
      LogicalOffset() /* additional_offset */,
      NGLogicalAnchorQuery::SetOptions::kValidInOrder);

  Element* element = DynamicTo<Element>(layout_object.GetNode());
  Element* implicit_anchor =
      element ? element->ImplicitAnchorElement() : nullptr;
  LayoutObject* implicit_anchor_object =
      implicit_anchor ? implicit_anchor->GetLayoutObject() : nullptr;

  return NGAnchorEvaluatorImpl(*logical_query,
                               layout_object.StyleRef().AnchorDefault(),
                               implicit_anchor_object, container_converter,
                               layout_object.StyleRef().GetWritingDirection(),
                               PhysicalOffset() /* offset_to_padding_box */,
                               layout_object.IsInTopOrViewTransitionLayer());
}

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const NGAnchorKey& key,
    bool can_use_invalid_anchors) const {
  if (const NGPhysicalAnchorReference* reference = Base::AnchorReference(key)) {
    if (can_use_invalid_anchors || !reference->is_invalid)
      return reference;
  }
  return nullptr;
}

const NGPhysicalFragment* NGPhysicalAnchorQuery::Fragment(
    const NGAnchorKey& key,
    bool can_use_invalid_anchors) const {
  if (const NGPhysicalAnchorReference* reference =
          AnchorReference(key, can_use_invalid_anchors)) {
    return reference->fragment.Get();
  }
  return nullptr;
}

const NGLogicalAnchorReference* NGLogicalAnchorQuery::AnchorReference(
    const NGAnchorKey& key,
    bool can_use_invalid_anchor) const {
  if (const NGLogicalAnchorReference* reference = Base::AnchorReference(key)) {
    for (const NGLogicalAnchorReference* result = reference; result;
         result = result->next) {
      if (can_use_invalid_anchor || !result->is_invalid)
        return result;
    }
  }
  return nullptr;
}

void NGLogicalAnchorQuery::Set(const NGAnchorKey& key,
                               const NGPhysicalFragment& fragment,
                               const LogicalRect& rect,
                               SetOptions options) {
  DCHECK(fragment.GetLayoutObject());
  Set(key,
      MakeGarbageCollected<NGLogicalAnchorReference>(
          fragment, rect, options == SetOptions::kInvalid),
      options == SetOptions::kValidOutOfOrder);
}

void NGLogicalAnchorQuery::Set(const NGAnchorKey& key,
                               NGLogicalAnchorReference* reference,
                               bool maybe_out_of_order) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = Base::insert(key, reference);
  if (result.is_new_entry)
    return;

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<NGLogicalAnchorReference>* const existing_head_ptr =
      result.stored_value;
  NGLogicalAnchorReference* const existing_head = *existing_head_ptr;
  DCHECK(existing_head);
  const NGLogicalAnchorReference* last_valid_existing = nullptr;
  const LayoutObject* new_object = reference->fragment->GetLayoutObject();
  DCHECK(new_object);
  for (NGLogicalAnchorReference* existing = existing_head; existing;
       existing = existing->next) {
    const LayoutObject* existing_object = existing->fragment->GetLayoutObject();
    DCHECK(existing_object);
    if (existing_object == new_object) {
      existing->rect.Unite(reference->rect);
      return;
    }
    if (!existing->is_invalid)
      last_valid_existing = existing;
  }

  // Ignore the new value if both new and existing values are valid, and the
  // call order is in the tree order.
  if (!maybe_out_of_order && !reference->is_invalid && last_valid_existing) {
    DCHECK(last_valid_existing->fragment->GetLayoutObject()->IsBeforeInPreOrder(
        *new_object));
    return;
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the tree order.
  reference->InsertInPreOrderInto(existing_head_ptr);
}

void NGPhysicalAnchorQuery::SetFromLogical(
    const NGLogicalAnchorQuery& logical_query,
    const WritingModeConverter& converter) {
  // This function assumes |this| is empty on the entry. Merging multiple
  // references is not supported.
  DCHECK(IsEmpty());
  for (const auto entry : logical_query) {
    // For each key, only the first one in the tree order, valid or invalid, is
    // needed to be propagated, because the validity is re-computed for each
    // containing block. Please see |SetFromPhysical|.
    const auto result =
        Base::insert(entry.key, MakeGarbageCollected<NGPhysicalAnchorReference>(
                                    *entry.value, converter));
    DCHECK(result.is_new_entry);
  }
}

void NGLogicalAnchorQuery::SetFromPhysical(
    const NGPhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset,
    SetOptions options) {
  for (auto entry : physical_query) {
    LogicalRect rect = converter.ToLogical(entry.value->rect);
    rect.offset += additional_offset;
    Set(entry.key,
        MakeGarbageCollected<NGLogicalAnchorReference>(
            *entry.value->fragment, rect, options == SetOptions::kInvalid),
        options == SetOptions::kValidOutOfOrder);
  }
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const NGLogicalAnchorReference& reference,
    CSSAnchorValue anchor_value,
    float percentage,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
    WritingDirectionMode self_writing_direction,
    const PhysicalOffset& offset_to_padding_box,
    bool is_y_axis,
    bool is_right_or_bottom) const {
  const PhysicalRect anchor = container_converter.ToPhysical(reference.rect);
  anchor_value = PhysicalAnchorValueFromLogical(
      anchor_value, container_converter.GetWritingDirection(),
      self_writing_direction, is_y_axis);
  LayoutUnit value;
  switch (anchor_value) {
    case CSSAnchorValue::kCenter: {
      const LayoutUnit start = is_y_axis
                                   ? anchor.Y() - offset_to_padding_box.top
                                   : anchor.X() - offset_to_padding_box.left;
      const LayoutUnit end = is_y_axis
                                 ? anchor.Bottom() - offset_to_padding_box.top
                                 : anchor.Right() - offset_to_padding_box.left;
      value = start + LayoutUnit::FromFloatRound((end - start) * 0.5);
      break;
    }
    case CSSAnchorValue::kLeft:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // Make the offset relative to the padding box, because the containing
      // block is formed by the padding edge.
      // https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
      value = anchor.X() - offset_to_padding_box.left;
      break;
    case CSSAnchorValue::kRight:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Right() - offset_to_padding_box.left;
      break;
    case CSSAnchorValue::kTop:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Y() - offset_to_padding_box.top;
      break;
    case CSSAnchorValue::kBottom:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |CSSAnchorValue::kLeft|.
      value = anchor.Bottom() - offset_to_padding_box.top;
      break;
    case CSSAnchorValue::kPercentage: {
      LayoutUnit size;
      if (is_y_axis) {
        value = anchor.Y() - offset_to_padding_box.top;
        size = anchor.Height();
        // The percentage is logical, between the `start` and `end` sides.
        // Convert to the physical percentage.
        // https://drafts.csswg.org/css-anchor-1/#anchor-pos
        if (container_converter.GetWritingDirection().IsFlippedY())
          percentage = 100 - percentage;
      } else {
        value = anchor.X() - offset_to_padding_box.left;
        size = anchor.Width();
        // Convert the logical percentage to physical. See above.
        if (container_converter.GetWritingDirection().IsFlippedX())
          percentage = 100 - percentage;
      }
      value += LayoutUnit::FromFloatRound(size * percentage / 100);
      break;
    }
    case CSSAnchorValue::kStart:
    case CSSAnchorValue::kEnd:
    case CSSAnchorValue::kSelfStart:
    case CSSAnchorValue::kSelfEnd:
      // These logical values should have been converted to corresponding
      // physical values in `PhysicalAnchorValueFromLogical`.
      NOTREACHED();
      return absl::nullopt;
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_right_or_bottom)
    return available_size - value;
  return value;
}

LayoutUnit NGLogicalAnchorQuery::EvaluateSize(
    const NGLogicalAnchorReference& reference,
    CSSAnchorSizeValue anchor_size_value,
    WritingMode container_writing_mode,
    WritingMode self_writing_mode) const {
  const LogicalSize& anchor = reference.rect.size;
  switch (anchor_size_value) {
    case CSSAnchorSizeValue::kInline:
      return anchor.inline_size;
    case CSSAnchorSizeValue::kBlock:
      return anchor.block_size;
    case CSSAnchorSizeValue::kWidth:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case CSSAnchorSizeValue::kHeight:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
    case CSSAnchorSizeValue::kSelfInline:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case CSSAnchorSizeValue::kSelfBlock:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
  }
  NOTREACHED();
  return LayoutUnit();
}

const NGLogicalAnchorQuery* NGAnchorEvaluatorImpl::AnchorQuery() const {
  if (anchor_query_)
    return anchor_query_;
  if (anchor_queries_) {
    DCHECK(containing_block_);
    anchor_query_ = &anchor_queries_->AnchorQuery(*containing_block_);
    DCHECK(anchor_query_);
    return anchor_query_;
  }
  return nullptr;
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::Evaluate(
    const CalculationExpressionNode& node) const {
  DCHECK(node.IsAnchorQuery());
  const auto& anchor_query = To<CalculationExpressionAnchorQueryNode>(node);
  switch (anchor_query.Type()) {
    case CSSAnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorSpecifier(),
                            anchor_query.AnchorSide(),
                            anchor_query.AnchorSidePercentageOrZero());
    case CSSAnchorQueryType::kAnchorSize:
      return EvaluateAnchorSize(anchor_query.AnchorSpecifier(),
                                anchor_query.AnchorSize());
  }
}

const NGLogicalAnchorReference* NGAnchorEvaluatorImpl::ResolveAnchorReference(
    const AnchorSpecifierValue& anchor_specifier) const {
  if (!anchor_specifier.IsNamed() && !default_anchor_specifier_ &&
      !implicit_anchor_) {
    return nullptr;
  }
  const NGLogicalAnchorQuery* anchor_query = AnchorQuery();
  if (!anchor_query) {
    return nullptr;
  }
  if (anchor_specifier.IsNamed()) {
    return anchor_query->AnchorReference(&anchor_specifier.GetName(),
                                         is_in_top_layer_);
  }
  if (anchor_specifier.IsDefault() && default_anchor_specifier_) {
    return anchor_query->AnchorReference(default_anchor_specifier_,
                                         is_in_top_layer_);
  }
  return anchor_query->AnchorReference(implicit_anchor_, is_in_top_layer_);
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchor(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorValue anchor_value,
    float percentage) const {
  has_anchor_functions_ = true;
  const NGLogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier);
  if (!anchor_reference) {
    return absl::nullopt;
  }

  DCHECK(AnchorQuery());
  return AnchorQuery()->EvaluateAnchor(
      *anchor_reference, anchor_value, percentage, available_size_,
      container_converter_, self_writing_direction_, offset_to_padding_box_,
      is_y_axis_, is_right_or_bottom_);
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchorSize(
    const AnchorSpecifierValue& anchor_specifier,
    CSSAnchorSizeValue anchor_size_value) const {
  has_anchor_functions_ = true;
  const NGLogicalAnchorReference* anchor_reference =
      ResolveAnchorReference(anchor_specifier);
  if (!anchor_reference) {
    return absl::nullopt;
  }

  DCHECK(AnchorQuery());
  return AnchorQuery()->EvaluateSize(*anchor_reference, anchor_size_value,
                                     container_converter_.GetWritingMode(),
                                     self_writing_direction_.GetWritingMode());
}

void NGLogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
  visitor->Trace(next);
}

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

}  // namespace blink
