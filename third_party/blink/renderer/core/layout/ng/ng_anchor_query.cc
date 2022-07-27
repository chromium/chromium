// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

const NGPhysicalAnchorReference* NGPhysicalAnchorQuery::AnchorReference(
    const AtomicString& name) const {
  const auto& it = anchor_references_.find(name);
  if (it != anchor_references_.end())
    return it->value.Get();
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
  if (it != anchor_references_.end())
    return &it->value;
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
                               const NGLogicalAnchorReference& reference) {
  const auto result = anchor_references_.insert(name, reference);
  if (result.is_new_entry)
    return;

  NGLogicalAnchorReference& existing = result.stored_value->value;
  if (existing.fragment->GetLayoutObject() !=
      reference.fragment->GetLayoutObject()) {
    // If this is the same name on a different |LayoutObject|, ignore it.
    // This logic assumes that callers call this function in the correct order.
    // TODO(crbug.com/1309178): Should we overwrite or ignore? Needs tests.
    return;
  }

  // If this is a fragment from the same |LayoutObject|, unite the rect.
  existing.rect.Unite(reference.rect);
}

void NGPhysicalAnchorQuery::SetFromLogical(
    const NGLogicalAnchorQuery& logical_query,
    const WritingModeConverter& converter) {
  for (const auto& it : logical_query.anchor_references_) {
    DCHECK_EQ(AnchorReference(it.key), nullptr);
    anchor_references_.Set(
        it.key, MakeGarbageCollected<NGPhysicalAnchorReference>(
                    converter.ToPhysical(it.value.rect), it.value.fragment));
  }
}

void NGLogicalAnchorQuery::SetFromPhysical(
    const NGPhysicalAnchorQuery& physical_query,
    const WritingModeConverter& converter,
    const LogicalOffset& additional_offset) {
  for (const auto& it : physical_query.anchor_references_) {
    LogicalRect rect = converter.ToLogical(it.value->rect);
    rect.offset += additional_offset;
    Set(it.key, NGLogicalAnchorReference{rect, it.value->fragment.Get()});
  }
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const AtomicString& anchor_name,
    AnchorValue anchor_value,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
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
      value = anchor.X();
      break;
    case AnchorValue::kRight:
      if (is_y_axis)
        return absl::nullopt;  // Wrong axis.
      value = anchor.Right();
      break;
    case AnchorValue::kTop:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      value = anchor.Y();
      break;
    case AnchorValue::kBottom:
      if (!is_y_axis)
        return absl::nullopt;  // Wrong axis.
      value = anchor.Bottom();
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

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

void NGPhysicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references_);
}

}  // namespace blink
