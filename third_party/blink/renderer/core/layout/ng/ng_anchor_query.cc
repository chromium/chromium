// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_logical_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

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

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const AtomicString& name) const {
  const auto& it = anchor_references_.find(name);
  if (it != anchor_references_.end()) {
    const NGPhysicalAnchorReference& result = *it->value;
    if (!result.is_invalid)
      return &result;
  }
  return nullptr;
}

const PhysicalRect* NGPhysicalAnchorQuery::Rect(
    const AtomicString& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGPhysicalAnchorQuery::Fragment(
    const AtomicString& name) const {
  if (const NGPhysicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment.Get();
  return nullptr;
}

const NGLogicalAnchorReference* NGLogicalAnchorQuery::AnchorReference(
    const AtomicString& name) const {
  const auto& it = anchor_references_.find(name);
  if (it != anchor_references_.end()) {
    for (const NGLogicalAnchorReference* result = it->value; result;
         result = result->next) {
      if (!result->is_invalid)
        return result;
    }
  }
  return nullptr;
}

const LogicalRect* NGLogicalAnchorQuery::Rect(const AtomicString& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return &reference->rect;
  return nullptr;
}

const NGPhysicalFragment* NGLogicalAnchorQuery::Fragment(
    const AtomicString& name) const {
  if (const NGLogicalAnchorReference* reference = AnchorReference(name))
    return reference->fragment;
  return nullptr;
}

void NGLogicalAnchorQuery::Set(const AtomicString& name,
                               const NGPhysicalFragment& fragment,
                               const LogicalRect& rect) {
  DCHECK(fragment.GetLayoutObject());
  Set(name,
      MakeGarbageCollected<NGLogicalAnchorReference>(
          fragment, rect, /* is_invalid */ fragment.IsOutOfFlowPositioned()));
}

void NGLogicalAnchorQuery::Set(const AtomicString& name,
                               NGLogicalAnchorReference* reference) {
  DCHECK(reference);
  DCHECK(!reference->next);
  const auto result = anchor_references_.insert(name, reference);
  if (result.is_new_entry)
    return;

  DCHECK(result.stored_value->value);
  NGLogicalAnchorReference& existing = *result.stored_value->value;
  const LayoutObject* existing_object = existing.fragment->GetLayoutObject();
  DCHECK(existing_object);
  const LayoutObject* new_object = reference->fragment->GetLayoutObject();
  DCHECK(new_object);
  if (existing_object != new_object) {
    if (!reference->is_invalid && !existing.is_invalid) {
      // If both new and existing values are valid, ignore the new value. This
      // logic assumes the callers call this function in the correct order.
      DCHECK(existing_object->IsBeforeInPreOrder(*new_object));
      return;
    }
    // When out-of-flow objects are involved, callers can't guarantee the call
    // order. Insert into the list in the tree order.
    reference->InsertInPreOrderInto(&result.stored_value->value);
    return;
  }

  // If this is a fragment from the same |LayoutObject|, unite the rect.
  existing.rect.Unite(reference->rect);
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
    bool is_invalid) {
  for (const auto& it : physical_query.anchor_references_) {
    LogicalRect rect = converter.ToLogical(it.value->rect);
    rect.offset += additional_offset;
    Set(it.key, MakeGarbageCollected<NGLogicalAnchorReference>(
                    *it.value->fragment, rect, is_invalid));
  }
}

void NGLogicalAnchorQuery::SetAsStitched(
    base::span<const NGLogicalLink> children,
    WritingDirectionMode writing_direction) {
  // This struct is a variation of |NGAnchorReference|, using the stitched
  // coordinate system for the block-fragmented out-of-flow positioned objects.
  struct NGStitchedAnchorReference
      : public GarbageCollected<NGStitchedAnchorReference> {
    NGStitchedAnchorReference(NGPhysicalAnchorReference* reference,
                              const LogicalRect& rect,
                              LogicalOffset first_container_offset,
                              LayoutUnit first_container_stitched_offset)
        : reference(reference),
          rect(rect),
          first_container_offset(first_container_offset),
          first_container_stitched_offset(first_container_stitched_offset) {}

    LogicalRect StitchedRect() const {
      LogicalRect stitched_rect = rect;
      stitched_rect.offset.block_offset += first_container_stitched_offset;
      return stitched_rect;
    }

    NGLogicalAnchorReference* StitchedAnchorReference() const {
      return MakeGarbageCollected<NGLogicalAnchorReference>(
          *reference->fragment, StitchedRect(), /* is_invalid */ false);
    }

    void Unite(const LogicalRect& other_rect,
               const LogicalOffset& container_offset) {
      // To unite fragments in the physical coordinate system as defined in the
      // spec while keeping the |reference.rect| relative to the first
      // container, make the |container_offset| relative to the first container.
      const LogicalRect other_rect_in_first_container =
          other_rect + (container_offset - first_container_offset);
      rect.Unite(other_rect_in_first_container);
    }

    void Trace(Visitor* visitor) const { visitor->Trace(reference); }

    // The |rect| is relative to the first container, so that it can a) unite
    // following fragments in the physical coordinate system, and b) compute the
    // result in the stitched coordinate system.
    Member<NGPhysicalAnchorReference> reference;
    LogicalRect rect;
    LogicalOffset first_container_offset;
    LayoutUnit first_container_stitched_offset;
  };

  struct NGStitchedAnchorQuery
      : public GarbageCollected<NGStitchedAnchorQuery> {
    void AddChild(const NGLogicalLink& child,
                  const LayoutUnit stitched_offset,
                  WritingDirectionMode writing_direction) {
      const NGPhysicalAnchorQuery* anchor_query = child->AnchorQuery();
      if (!anchor_query)
        return;
      DCHECK_EQ(child->Style().GetWritingDirection(), writing_direction);
      const WritingModeConverter converter(writing_direction, child->Size());
      for (const auto& it : *anchor_query) {
        const LogicalRect rect = converter.ToLogical(it.value->rect);
        const auto result = references.insert(
            it.key, MakeGarbageCollected<NGStitchedAnchorReference>(
                        it.value, rect, child.offset, stitched_offset));
        if (!result.is_new_entry)
          result.stored_value->value->Unite(rect, child.offset);
      }
    }

    void Trace(Visitor* visitor) const { visitor->Trace(references); }

    HeapHashMap<AtomicString, Member<NGStitchedAnchorReference>> references;
  };

  auto* stitched_anchor_query = MakeGarbageCollected<NGStitchedAnchorQuery>();
  LayoutUnit stitched_offset;
  for (const NGLogicalLink& child : children) {
    stitched_anchor_query->AddChild(child, stitched_offset, writing_direction);
    stitched_offset += child->Size()
                           .ConvertToLogical(writing_direction.GetWritingMode())
                           .block_size;
  }

  // Convert the united anchor references to the stitched coordinate system.
  DCHECK(IsEmpty());
  for (const auto& it : stitched_anchor_query->references)
    Set(it.key, it.value->StitchedAnchorReference());
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const AtomicString& anchor_name,
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
    const AtomicString& anchor_name,
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

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchor(
    const AtomicString& anchor_name,
    AnchorValue anchor_value) const {
  has_anchor_functions_ = true;
  return anchor_query_.EvaluateAnchor(
      anchor_name, anchor_value, available_size_, container_converter_,
      offset_to_padding_box_, is_y_axis_, is_right_or_bottom_);
}

absl::optional<LayoutUnit> NGAnchorEvaluatorImpl::EvaluateAnchorSize(
    const AtomicString& anchor_name,
    AnchorSizeValue anchor_size_value) const {
  has_anchor_functions_ = true;
  return anchor_query_.EvaluateSize(anchor_name, anchor_size_value,
                                    container_converter_.GetWritingMode(),
                                    self_writing_mode_);
}

void NGLogicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
  visitor->Trace(next);
}

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

void NGPhysicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

}  // namespace blink
