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

namespace blink {

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

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const ScopedCSSName& name) const {
  const auto& it = anchor_references_.find(&name);
  if (it != anchor_references_.end()) {
    const NGPhysicalAnchorReference& result = *it->value;
    if (!result.is_invalid)
      return &result;
  }
  return nullptr;
}

const PhysicalRect* NGPhysicalAnchorQuery::Rect(
    const ScopedCSSName& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGPhysicalAnchorQuery::Fragment(
    const ScopedCSSName& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment.Get();
  return nullptr;
}

const NGLogicalAnchorReference* NGLogicalAnchorQuery::AnchorReference(
    const ScopedCSSName& name) const {
  const auto& it = anchor_references_.find(&name);
  if (it != anchor_references_.end()) {
    for (const NGLogicalAnchorReference* result = it->value; result;
         result = result->next) {
      if (!result->is_invalid)
        return result;
    }
  }
  return nullptr;
}

const LogicalRect* NGLogicalAnchorQuery::Rect(const ScopedCSSName& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGLogicalAnchorQuery::Fragment(
    const ScopedCSSName& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment;
  return nullptr;
}

void NGLogicalAnchorQuery::Set(const ScopedCSSName& name,
                               const NGPhysicalFragment& fragment,
                               const LogicalRect& rect,
                               SetOptions options) {
  DCHECK(fragment.GetLayoutObject());
  Set(name,
      MakeGarbageCollected<NGLogicalAnchorReference>(
          fragment, rect, options == SetOptions::kInvalid),
      options == SetOptions::kValidOutOfOrder);
}

void NGLogicalAnchorQuery::Set(const ScopedCSSName& name,
                               NGLogicalAnchorReference* reference,
                               bool maybe_out_of_order) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = anchor_references_.insert(&name, reference);
  if (result.is_new_entry)
    return;

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<NGLogicalAnchorReference>* const existing_head_ptr =
      &result.stored_value->value;
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
  for (const auto& it : logical_query.anchor_references_) {
    // For each key, only the first one in the tree order, valid or invalid, is
    // needed to be propagated, because the validity is re-computed for each
    // containing block. Please see |SetFromPhysical|.
    const auto result = anchor_references_.Set(
        it.key,
        MakeGarbageCollected<NGPhysicalAnchorReference>(*it.value, converter));
    DCHECK(result.is_new_entry);
  }
}

void NGLogicalAnchorQuery::SetFromPhysical(
    const NGPhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset,
    SetOptions options) {
  for (const auto& it : physical_query.anchor_references_) {
    LogicalRect rect = converter.ToLogical(it.value->rect);
    rect.offset += additional_offset;
    Set(*it.key,
        MakeGarbageCollected<NGLogicalAnchorReference>(
            *it.value->fragment, rect, options == SetOptions::kInvalid),
        options == SetOptions::kValidOutOfOrder);
  }
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const ScopedCSSName& anchor_name,
    AnchorValue anchor_value,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
    const PhysicalOffset& offset_to_padding_box,
    bool is_y_axis,
    bool is_right_or_bottom) const {
  const NGLogicalAnchorReference* reference = AnchorReference(anchor_name);
  if (!reference)
    return absl::nullopt;  // No targets.

  const PhysicalRect anchor = container_converter.ToPhysical(reference->rect);
  LayoutUnit value;
  switch (anchor_value) {
    case AnchorValue::kLeft:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // Make the offset relative to the padding box, because the containing
      // block is formed by the padding edge.
      // https://www.w3.org/TR/CSS21/visudet.html#containing-block-details
      value = anchor.X() - offset_to_padding_box.left;
      break;
    case AnchorValue::kRight:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Right() - offset_to_padding_box.left;
      break;
    case AnchorValue::kTop:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Y() - offset_to_padding_box.top;
      break;
    case AnchorValue::kBottom:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      // See |AnchorValue::kLeft|.
      value = anchor.Bottom() - offset_to_padding_box.top;
      break;
    default:
      NOTREACHED();
      return absl::nullopt;
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_right_or_bottom)
    return available_size - value;
  return value;
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateSize(
    const ScopedCSSName& anchor_name,
    AnchorSizeValue anchor_size_value,
    WritingMode container_writing_mode,
    WritingMode self_writing_mode) const {
  const NGLogicalAnchorReference* reference = AnchorReference(anchor_name);
  if (!reference)
    return absl::nullopt;  // No targets.

  const LogicalSize& anchor = reference->rect.size;
  switch (anchor_size_value) {
    case AnchorSizeValue::kInline:
      return anchor.inline_size;
    case AnchorSizeValue::kBlock:
      return anchor.block_size;
    case AnchorSizeValue::kWidth:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case AnchorSizeValue::kHeight:
      return IsHorizontalWritingMode(container_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
    case AnchorSizeValue::kSelfInline:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.inline_size
                 : anchor.block_size;
    case AnchorSizeValue::kSelfBlock:
      return IsHorizontalWritingMode(container_writing_mode) ==
                     IsHorizontalWritingMode(self_writing_mode)
                 ? anchor.block_size
                 : anchor.inline_size;
  }
  NOTREACHED();
  return absl::nullopt;
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
    case AnchorQueryType::kAnchor:
      return EvaluateAnchor(anchor_query.AnchorName(),
                            anchor_query.AnchorSide());
    case AnchorQueryType::kAnchorSize:
      return EvaluateAnchorSize(anchor_query.AnchorName(),
                                anchor_query.AnchorSize());
  }
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchor(
    const ScopedCSSName& anchor_name,
    AnchorValue anchor_value) const {
  has_anchor_functions_ = true;
  // TODO(crbug.com/1380112): Support implicit anchor.
  if (const NGLogicalAnchorQuery* anchor_query = AnchorQuery()) {
    return anchor_query->EvaluateAnchor(
        anchor_name, anchor_value, available_size_, container_converter_,
        offset_to_padding_box_, is_y_axis_, is_right_or_bottom_);
  }
  return absl::nullopt;
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchorSize(
    const ScopedCSSName& anchor_name,
    AnchorSizeValue anchor_size_value) const {
  has_anchor_functions_ = true;
  // TODO(crbug.com/1380112): Support implicit anchor.
  if (const NGLogicalAnchorQuery* anchor_query = AnchorQuery()) {
    return anchor_query->EvaluateSize(anchor_name, anchor_size_value,
                                      container_converter_.GetWritingMode(),
                                      self_writing_mode_);
  }
  return absl::nullopt;
}

void NGLogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
  visitor->Trace(next);
}

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

void NGLogicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

void NGPhysicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

}  // namespace blink
