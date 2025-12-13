// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutBox;
class PhysicalBoxFragment;

// The inline-size of the first fragment.
LayoutUnit BoxInlineSize(const LayoutBox& box);

// The total block-size of all fragments.
LayoutUnit BoxTotalBlockSize(const LayoutBox& box);

// Set the LayoutBox location for direct children of the specified fragment, or,
// if the specified fragment establishes a root fragmentation context (i.e. when
// it does not participate in any outer fragmentation context), do this for the
// entire fragmented subtree. This function is called after layout of each
// node. For fragmented content, we need to have laid out the entire
// fragmentation context before we can tell where boxes are relatively to each
// other.
void UpdateChildLayoutBoxLocations(const PhysicalBoxFragment&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BOX_UTILS_H_
