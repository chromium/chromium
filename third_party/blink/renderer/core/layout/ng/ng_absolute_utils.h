// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGBlockNode;
class NGConstraintSpace;
class NGLayoutResult;
struct NGLogicalStaticPosition;

struct CORE_EXPORT NGLogicalOutOfFlowDimensions {
  NGBoxStrut inset;
  LogicalSize size = {kIndefiniteSize, kIndefiniteSize};
  NGBoxStrut margins;
};

// The following routines implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as |NGLogicalOutOfFlowDimensions|.
// It needs to be computed in 2 stages:
// 1. The inline-dimensions with |ComputeOutOfFlowInlineDimensions|.
// 2. The block-dimensions with |ComputeOutOfFlowBlockDimensions|.
//
// NOTE: |ComputeOutOfFlowInlineDimensions| may call
// |ComputeOutOfFlowBlockDimensions| if its required to correctly determine the
// min/max content sizes.

// |replaced_size| should be set if and only if element is replaced element.
// Will return true if |NGBlockNode::ComputeMinMaxSizes| was called.
CORE_EXPORT bool ComputeOutOfFlowInlineDimensions(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions);

// If layout was performed to determine the position, this will be returned
// otherwise it will return nullptr.
CORE_EXPORT scoped_refptr<const NGLayoutResult> ComputeOutOfFlowBlockDimensions(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions);

CORE_EXPORT void AdjustOffsetForSplitInline(
    const NGBlockNode& node,
    const NGBoxFragmentBuilder* container_builder,
    LogicalOffset& offset);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
