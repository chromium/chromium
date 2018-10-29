// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGSpaceUtils_h
#define NGSpaceUtils_h

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class ComputedStyle;
struct NGBfcOffset;

// Whether child's constraint space should shrink to its intrinsic width.
// This is needed for buttons, select, input, floats and orthogonal children.
// See LayoutBox::sizesLogicalWidthToFitContent for the rationale behind this.
bool ShouldShrinkToFit(const ComputedStyle& parent_style,
                       const ComputedStyle& style);

// Adjusts {@code offset} to the clearance line.
CORE_EXPORT bool AdjustToClearance(LayoutUnit clearance_offset,
                                   NGBfcOffset* offset);

// Create a child constraint space with only extrinsic block sizing data. This
// will and can not be used for final layout, but is needed in an intermediate
// measure pass that calculates the min/max size contribution from a child that
// establishes an orthogonal flow root.
//
// Note that it's the child's *block* size that will be propagated as min/max
// inline size to the container. Therefore it's crucial to provide the child
// with an available inline size (which can be derived from the block size of
// the container if definite). We'll provide any extrinsic available block size
// that we have. This includes fixed and resolvable percentage sizes, for
// instance, while auto will not resolve. If no extrinsic size can be
// determined, we will resort to using a fallback later on, such as the initial
// containing block size. Spec:
// https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
NGConstraintSpace CreateExtrinsicConstraintSpaceForChild(
    const NGConstraintSpace& container_constraint_space,
    LayoutUnit container_extrinsic_block_size,
    NGLayoutInputNode child);

}  // namespace blink

#endif  // NGSpaceUtils_h
