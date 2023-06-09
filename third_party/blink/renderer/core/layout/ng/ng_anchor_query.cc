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
CSSAnchorValue PhysicalAnchorValueFromLogicalOrAuto(
    CSSAnchorValue anchor_value,
    WritingDirectionMode writing_direction,
    WritingDirectionMode self_writing_direction,
    bool is_y_axis,
    bool is_right_or_bottom) {
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
    case CSSAnchorValue::kAuto:
    case CSSAnchorValue::kAutoSame: {
      bool use_right_or_bottom =
          is_right_or_bottom == (anchor_value == CSSAnchorValue::kAutoSame);
      if (is_y_axis) {
        return use_right_or_bottom ? CSSAnchorValue::kBottom
                                   : CSSAnchorValue::kTop;
      }
      return use_right_or_bottom ? CSSAnchorValue::kRight
                                 : CSSAnchorValue::kLeft;
    }
    default:
      return anchor_value;
  }
}

}  // namespace

NGPhysicalAnchorReference::NGPhysicalAnchorReference(
    const NGLogicalAnchorReference& logical_reference,
    const WritingModeConverter& converter)
    : rect(converter.ToPhysical(logical_reference.rect)),
      layout_object(logical_reference.layout_object),
      is_out_of_flow(logical_reference.is_out_of_flow) {}

void NGLogicalAnchorReference::InsertInReverseTreeOrderInto(
    Member<NGLogicalAnchorReference>* head_ptr) {
  for (;;) {
    NGLogicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->layout_object);
    if (!head || head->layout_object->IsBeforeInPreOrder(*layout_object)) {
      // An in-flow reference has higher precedence than any other reference
      // before it in tree order, in which case there's no need to keep the
      // other references.
      if (is_out_of_flow) {
        next = head;
      }
      *head_ptr = this;
      break;
    }

    // Skip adding if there is already an in-flow reference that is after in
    // the tree order, which always has higher precedence than |this|.
    if (!head->is_out_of_flow) {
      break;
    }

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

  NGLogicalAnchorQuery* logical_query =
      MakeGarbageCollected<NGLogicalAnchorQuery>();
  logical_query->SetFromPhysical(*physical_query, container_converter,
                                 LogicalOffset() /* additional_offset */,
                                 NGLogicalAnchorQuery::SetOptions::kInFlow);

  Element* element = DynamicTo<Element>(layout_object.GetNode());
  Element* implicit_anchor =
      element ? element->ImplicitAnchorElement() : nullptr;
  LayoutObject* implicit_anchor_object =
      implicit_anchor ? implicit_anchor->GetLayoutObject() : nullptr;

  return NGAnchorEvaluatorImpl(layout_object, *logical_query,
                               layout_object.StyleRef().AnchorDefault(),
                               implicit_anchor_object, container_converter,
                               layout_object.StyleRef().GetWritingDirection(),
                               PhysicalOffset() /* offset_to_padding_box */);
}

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const LayoutObject& query_object,
    const NGAnchorKey& key) const {
  if (const NGPhysicalAnchorReference* reference = Base::AnchorReference(key)) {
    for (const NGPhysicalAnchorReference* result = reference; result;
         result = result->next) {
      if (!result->is_out_of_flow ||
          result->layout_object->IsBeforeInPreOrder(query_object)) {
        return result;
      }
    }
  }
  return nullptr;
}

const LayoutObject* NGPhysicalAnchorQuery::AnchorLayoutObject(
    const LayoutObject& query_object,
    const NGAnchorKey& key) const {
  if (const NGPhysicalAnchorReference* reference =
          AnchorReference(query_object, key)) {
    return reference->layout_object;
  }
  return nullptr;
}

const NGLogicalAnchorReference* NGLogicalAnchorQuery::AnchorReference(
    const LayoutObject& query_object,
    const NGAnchorKey& key) const {
  if (const NGLogicalAnchorReference* reference = Base::AnchorReference(key)) {
    for (const NGLogicalAnchorReference* result = reference; result;
         result = result->next) {
      if (!result->is_out_of_flow ||
          result->layout_object->IsBeforeInPreOrder(query_object)) {
        return result;
      }
    }
  }
  return nullptr;
}

void NGLogicalAnchorQuery::Set(const NGAnchorKey& key,
                               const LayoutObject& layout_object,
                               const LogicalRect& rect,
                               SetOptions options) {
  Set(key, MakeGarbageCollected<NGLogicalAnchorReference>(
               layout_object, rect, options == SetOptions::kOutOfFlow));
}

void NGLogicalAnchorQuery::Set(const NGAnchorKey& key,
                               NGLogicalAnchorReference* reference) {
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
  const LayoutObject* new_object = reference->layout_object;
  DCHECK(new_object);
  for (NGLogicalAnchorReference* existing = existing_head; existing;
       existing = existing->next) {
    const LayoutObject* existing_object = existing->layout_object;
    DCHECK(existing_object);
    if (existing_object == new_object) {
      existing->rect.Unite(reference->rect);
      return;
    }
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the reverse tree order.
  reference->InsertInReverseTreeOrderInto(existing_head_ptr);
}

void NGPhysicalAnchorQuery::SetFromLogical(
    const NGLogicalAnchorQuery& logical_query,
    const WritingModeConverter& converter) {
  // This function assumes |this| is empty on the entry. Merging multiple
  // references is not supported.
  DCHECK(IsEmpty());
  for (const auto entry : logical_query) {
    NGPhysicalAnchorReference* head =
        MakeGarbageCollected<NGPhysicalAnchorReference>(*entry.value,
                                                        converter);
    NGPhysicalAnchorReference* tail = head;
    for (NGLogicalAnchorReference* runner = entry.value->next; runner;
         runner = runner->next) {
      tail->next =
          MakeGarbageCollected<NGPhysicalAnchorReference>(*runner, converter);
      tail = tail->next;
    }
    const auto result = Base::insert(entry.key, head);
    DCHECK(result.is_new_entry);
  }
}

void NGLogicalAnchorQuery::SetFromPhysical(
    const NGPhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset,
    SetOptions options) {
  for (auto entry : physical_query) {
    // For each key, only the last one in the tree order, in or out of flow, is
    // needed to be propagated, because whether it's in flow is re-computed for
    // each containing block.
    LogicalRect rect = converter.ToLogical(entry.value->rect);
    rect.offset += additional_offset;
    Set(entry.key, MakeGarbageCollected<NGLogicalAnchorReference>(
                       *entry.value->layout_object, rect,
                       options == SetOptions::kOutOfFlow));
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
  anchor_value = PhysicalAnchorValueFromLogicalOrAuto(
      anchor_value, container_converter.GetWritingDirection(),
      self_writing_direction, is_y_axis, is_right_or_bottom);
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
    case CSSAnchorValue::kAuto:
    case CSSAnchorValue::kAutoSame:
      // These logical values should have been converted to corresponding
      // physical values in `PhysicalAnchorValueFromLogicalOrAuto`.
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
    return anchor_query->AnchorReference(*query_object_,
                                         &anchor_specifier.GetName());
  }
  if (anchor_specifier.IsDefault() && default_anchor_specifier_) {
    return anchor_query->AnchorReference(*query_object_,
                                         default_anchor_specifier_);
  }
  return anchor_query->AnchorReference(*query_object_, implicit_anchor_);
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
  visitor->Trace(layout_object);
  visitor->Trace(next);
}

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
  visitor->Trace(next);
}

}  // namespace blink
