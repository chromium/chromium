// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGConstraintSpace;
struct NGLogicalStaticPosition;

struct CORE_EXPORT NGLogicalOutOfFlowPosition {
  NGBoxStrut inset;
  LogicalSize size;
  NGBoxStrut margins;
};

// Implements <dialog> static positioning.
//
// Returns new dialog top position if layout_dialog requires <dialog>
// OOF-positioned centering.
CORE_EXPORT base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& layout_dialog,
    LayoutUnit height);

// The following routines implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as |NGLogicalOutOfFlowPosition|.
// It needs to be computed in 4 stages:
// 1. If |AbsoluteNeedsChildInlineSize| is true, compute estimated inline_size
//    using |NGBlockNode::MinMaxSize|.
// 2. Compute part of the |NGLogicalOutOfFlowPosition| which depends on the
//    child inline-size with |ComputePartialAbsoluteWithChildInlineSize|.
// 3. If |AbsoluteNeedsChildBlockSize| is true, compute estimated block_size by
//    performing layout with the inline_size calculated from (2).
// 4. Compute the full |NGLogicalOutOfFlowPosition| with
//    |ComputeFullAbsoluteWithChildBlockSize|.

// Returns true if |ComputePartialAbsoluteWithChildInlineSize| will need an
// estimated inline-size.
CORE_EXPORT bool AbsoluteNeedsChildInlineSize(const ComputedStyle&);

// Returns true if |ComputeFullAbsoluteWithChildBlockSize| will need an
// estimated block-size.
CORE_EXPORT bool AbsoluteNeedsChildBlockSize(const ComputedStyle&);

// Computes part of the absolute position which depends on the child's
// inline-size.
// |replaced_size| should be set if and only if element is replaced element.
// Returns the partially filled position.
CORE_EXPORT NGLogicalOutOfFlowPosition
ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const base::Optional<MinMaxSize>& child_minmax,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction);

// Computes the rest of the absolute position which depends on child's
// block-size.
CORE_EXPORT void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction,
    NGLogicalOutOfFlowPosition* position);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
