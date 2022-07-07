// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

namespace blink {

absl::optional<LayoutUnit> NGLogicalAnchorQuery::Evaluate(
    const AtomicString& anchor_name,
    AnchorValue anchor_value,
    LayoutUnit available_size,
    bool is_block_direction,
    bool is_end) const {
  const auto it = anchor_references.find(anchor_name);
  if (it == anchor_references.end())
    return absl::nullopt;  // No targets.

  LayoutUnit value;
  switch (anchor_value) {
    case AnchorValue::kLeft:
      if (is_block_direction)
        return absl::nullopt;  // Wrong axis.
      value = it->value.offset.inline_offset;
      break;
    case AnchorValue::kRight:
      if (is_block_direction)
        return absl::nullopt;  // Wrong axis.
      value = it->value.InlineEndOffset();
      break;
    case AnchorValue::kTop:
      if (!is_block_direction)
        return absl::nullopt;  // Wrong axis.
      value = it->value.offset.block_offset;
      break;
    case AnchorValue::kBottom:
      if (!is_block_direction)
        return absl::nullopt;  // Wrong axis.
      value = it->value.BlockEndOffset();
      break;
    default:
      NOTREACHED();
      return absl::nullopt;
  }

  // The |value| is for the "start" side of insets. For the "end" side of
  // insets, return the distance from |available_size|.
  if (is_end)
    return available_size - value;
  return value;
}

}  // namespace blink
