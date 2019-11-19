// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

// Returns the child's relative position wrt the containing fragment.
PhysicalOffset ComputeRelativeOffset(const ComputedStyle& child_style,
                                     WritingMode container_writing_mode,
                                     TextDirection container_direction,
                                     PhysicalSize container_size) {
  PhysicalOffset offset;
  if (child_style.GetPosition() != EPosition::kRelative)
    return offset;

  base::Optional<LayoutUnit> left, right, top, bottom;

  if (!child_style.Left().IsAuto())
    left = MinimumValueForLength(child_style.Left(), container_size.width);
  if (!child_style.Right().IsAuto())
    right = MinimumValueForLength(child_style.Right(), container_size.width);
  if (!child_style.Top().IsAuto())
    top = MinimumValueForLength(child_style.Top(), container_size.height);
  if (!child_style.Bottom().IsAuto())
    bottom = MinimumValueForLength(child_style.Bottom(), container_size.height);

  // Common case optimization
  if (!left && !right && !top && !bottom)
    return offset;

  // Implements confict resolution rules from spec:
  // https://www.w3.org/TR/css-position-3/#rel-pos
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  }
  if (!left)
    left = -*right;
  if (!right)
    right = -*left;
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -*bottom;
  if (!bottom)
    bottom = -*top;

  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (IsLtr(container_direction))
      offset.left = *left;
    else
      offset.left = -*right;
    offset.top = *top;
  } else {
    if (IsLtr(container_direction))
      offset.top = *top;
    else
      offset.top = -*bottom;
    offset.left = *left;
  }
  return offset;
}

}  // namespace blink
