// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class NGConstraintSpace;
class NGExclusionSpace;
struct NGBfcOffset;
struct LogicalSize;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

typedef Vector<NGPositionedFloat, 8> NGPositionedFloatVector;

// Returns the inline size (relative to {@code parent_space}) of the
// unpositioned float.
LayoutUnit ComputeMarginBoxInlineSizeForUnpositionedFloat(
    const NGConstraintSpace& parent_space,
    const ComputedStyle& parent_style,
    NGUnpositionedFloat* unpositioned_float);

// Positions {@code unpositioned_float} into {@code new_parent_space}.
// @returns A positioned float.
CORE_EXPORT NGPositionedFloat
PositionFloat(const LogicalSize& float_available_size,
              const LogicalSize& float_percentage_size,
              const LogicalSize& float_replaced_percentage_size,
              const NGBfcOffset& origin_bfc_offset,
              NGUnpositionedFloat*,
              const NGConstraintSpace& parent_space,
              const ComputedStyle& parent_style,
              NGExclusionSpace* exclusion_space);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
