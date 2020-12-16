// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_RELATIVE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_RELATIVE_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class NGConstraintSpace;

// Implements relative positioning:
// https://www.w3.org/TR/css-position-3/#rel-pos
// Returns the relative position offset as defined by |child_style|.
CORE_EXPORT LogicalOffset
ComputeRelativeOffset(const ComputedStyle& child_style,
                      WritingDirectionMode container_writing_direction,
                      const LogicalSize& available_size);

CORE_EXPORT LogicalOffset ComputeRelativeOffsetForBoxFragment(
    const NGPhysicalBoxFragment& fragment,
    WritingDirectionMode container_writing_direction,
    const LogicalSize& available_size);

CORE_EXPORT LogicalOffset
ComputeRelativeOffsetForInline(const NGConstraintSpace& space,
                               const ComputedStyle& child_style);

// Un-apply any offset caused by relative positioning. When re-using a previous
// fragment's offset (from the cache), we need to convert from "paint offset" to
// "layout offset" before re-adding the fragment, in order to get overflow
// calculation right.
inline void RemoveRelativeOffset(const NGBoxFragmentBuilder& builder,
                                 const NGPhysicalFragment& fragment,
                                 LogicalOffset* offset) {
  if (fragment.Style().GetPosition() != EPosition::kRelative)
    return;
  if (const auto* box_fragment = DynamicTo<NGPhysicalBoxFragment>(&fragment)) {
    *offset -= ComputeRelativeOffsetForBoxFragment(
        *box_fragment, builder.GetWritingDirection(),
        builder.ChildAvailableSize());
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_RELATIVE_UTILS_H_
