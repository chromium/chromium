// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGFloatsUtils_h
#define NGFloatsUtils_h

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float_vector.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBlockNode;
class NGConstraintSpace;
class NGContainerFragmentBuilder;
class NGExclusionSpace;
struct NGBfcOffset;
struct NGLogicalSize;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

typedef Vector<NGPositionedFloat, 8> NGPositionedFloatVector;

enum NGFloatTypeValue {
  kFloatTypeNone = 0b00,
  kFloatTypeLeft = 0b01,
  kFloatTypeRight = 0b10,
  kFloatTypeBoth = 0b11
};
typedef int NGFloatTypes;

// Returns the inline size (relative to {@code parent_space}) of the
// unpositioned float.
CORE_EXPORT LayoutUnit ComputeMarginBoxInlineSizeForUnpositionedFloat(
    const NGConstraintSpace& parent_space,
    NGUnpositionedFloat* unpositioned_float);

// Positions {@code unpositioned_float} into {@code new_parent_space}.
// @returns A positioned float.
CORE_EXPORT NGPositionedFloat
PositionFloat(const NGLogicalSize& float_available_size,
              const NGLogicalSize& float_percentage_size,
              const NGLogicalSize& float_replaced_percentage_size,
              const NGBfcOffset& origin_bfc_offset,
              LayoutUnit parent_bfc_block_offset,
              NGUnpositionedFloat*,
              const NGConstraintSpace& parent_space,
              NGExclusionSpace* exclusion_space);

// Positions the list of {@code unpositioned_floats}. Adds them as exclusions to
// {@code space}.
CORE_EXPORT void PositionFloats(
    const NGLogicalSize& float_available_size,
    const NGLogicalSize& float_percentage_size,
    const NGLogicalSize& float_replaced_percentage_size,
    const NGBfcOffset& origin_bfc_offset,
    LayoutUnit container_block_offset,
    NGUnpositionedFloatVector& unpositioned_floats,
    const NGConstraintSpace& space,
    NGExclusionSpace* exclusion_space,
    NGPositionedFloatVector* positioned_floats);

// Add a pending float to the list. It will be committed (positioned) once we
// have resolved the BFC block offset.
void AddUnpositionedFloat(NGUnpositionedFloatVector* unpositioned_floats,
                          NGContainerFragmentBuilder* fragment_builder,
                          NGUnpositionedFloat unpositioned_float);

// Remove a pending float from the list.
bool RemoveUnpositionedFloat(NGUnpositionedFloatVector* unpositioned_floats,
                             NGBlockNode float_node);

NGFloatTypes ToFloatTypes(EClear clear);

}  // namespace blink

#endif  // NGFloatsUtils_h
