// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExclusionSpace;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

typedef HeapVector<NGPositionedFloat, 8> NGPositionedFloatVector;

// Calculate and return the inline size of the unpositioned float.
LayoutUnit ComputeMarginBoxInlineSizeForUnpositionedFloat(NGUnpositionedFloat*);

// Position and lay out a float.
NGPositionedFloat PositionFloat(NGUnpositionedFloat*, ExclusionSpace*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
