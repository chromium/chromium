// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGAbsoluteUtils_h
#define NGAbsoluteUtils_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGConstraintSpace;
struct NGStaticPosition;

struct CORE_EXPORT NGAbsolutePhysicalPosition {
  NGPhysicalBoxStrut inset;
  NGPhysicalSize size;
  NGPhysicalBoxStrut margins;
  String ToString() const;
};

// Implements <dialog> special case abspos static positining.
// Returns new dialog top position if layout_dialog requires
// <dialog> abspos centering.
CORE_EXPORT base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& layout_dialog,
    LayoutUnit height);

// The following routines implement absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as NGAbsolutePhysicalPosition.
// It needs to be computed in 4 stages:
// 1. If AbsoluteNeedsChildInlineSize compute estimated inline_size using
//    MinMaxSize.ShrinkToFit.
// 2. Compute part of PhysicalPosition that depends upon child inline size
//    with ComputePartialAbsoluteWithChildInlineSize.
// 3. If AbsoluteNeedsChildBlockSize compute estimated block_size by
//    performing layout with the inline_size calculated from (2).
// 4. Compute full PhysicalPosition by filling it in with parts that depend
//    upon child's block_size.

// True if ComputePartialAbsoluteWithChildInlineSize will need
// estimated inline size.
CORE_EXPORT bool AbsoluteNeedsChildInlineSize(const ComputedStyle&);

// True if ComputeFullAbsoluteWithChildBlockSize will need
// estimated block size.
CORE_EXPORT bool AbsoluteNeedsChildBlockSize(const ComputedStyle&);

// Compute part of position that depends on child's inline_size.
// replaced_size should be set if and only if element is replaced element.
// Returns partially filled position.
CORE_EXPORT NGAbsolutePhysicalPosition
ComputePartialAbsoluteWithChildInlineSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition&,
    const base::Optional<MinMaxSize>& child_minmax,
    const base::Optional<NGLogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction);

// Compute rest of NGPhysicalRect that depends on child's block_size.
CORE_EXPORT void ComputeFullAbsoluteWithChildBlockSize(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGStaticPosition&,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<NGLogicalSize>& replaced_size,
    const WritingMode container_writing_mode,
    const TextDirection container_direction,
    NGAbsolutePhysicalPosition* position);

}  // namespace blink

#endif  // NGAbsoluteUtils_h
