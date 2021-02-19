// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGExclusionSpace;
struct NGPositionedFloat;
struct NGUnpositionedFloat;

typedef Vector<NGPositionedFloat, 8> NGPositionedFloatVector;

// Calculate and return the inline size of the unpositioned float.
LayoutUnit ComputeMarginBoxInlineSizeForUnpositionedFloat(NGUnpositionedFloat*);

// Position and lay out a float.
NGPositionedFloat PositionFloat(NGUnpositionedFloat*, NGExclusionSpace*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLOATS_UTILS_H_
