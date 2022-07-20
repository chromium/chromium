// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

namespace blink {

void NGPhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(fragment);
}

void NGPhysicalAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_references);
}

absl::optional<LayoutUnit> NGLogicalAnchorQuery::EvaluateAnchor(
    const AtomicString& anchor_name,
    AnchorValue anchor_value,
    LayoutUnit available_size,
    const WritingModeConverter& container_converter,
    bool is_y_axis,
    bool is_right_or_bottom) const {
  const auto it = anchor_references.find(anchor_name);
  if (it == anchor_references.end())
    return absl::nullopt;  // No targets.

  const PhysicalRect anchor = container_converter.ToPhysical(it->value.rect);
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
  const auto it = anchor_references.find(anchor_name);
  if (it == anchor_references.end())
    return absl::nullopt;  // No targets.

  const LogicalSize& anchor = it->value.rect.size;
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

}  // namespace blink
