// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_size.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

// Returns the child's relative position wrt the containing fragment.
NGPhysicalOffset ComputeRelativeOffset(const ComputedStyle& child_style,
                                       WritingMode container_writing_mode,
                                       TextDirection container_direction,
                                       NGPhysicalSize container_size) {
  NGPhysicalOffset offset;
  if (child_style.GetPosition() != EPosition::kRelative)
    return offset;

  base::Optional<LayoutUnit> left, right, top, bottom;

  if (!child_style.Left().IsAuto())
    left = ValueForLength(child_style.Left(), container_size.width);
  if (!child_style.Right().IsAuto())
    right = ValueForLength(child_style.Right(), container_size.width);
  if (!child_style.Top().IsAuto())
    top = ValueForLength(child_style.Top(), container_size.height);
  if (!child_style.Bottom().IsAuto())
    bottom = ValueForLength(child_style.Bottom(), container_size.height);

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
    left = -right.value();
  if (!right)
    right = -left.value();
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -bottom.value();
  if (!bottom)
    bottom = -top.value();

  if (IsHorizontalWritingMode(container_writing_mode)) {
    if (IsLtr(container_direction))
      offset.left = left.value();
    else
      offset.left = -right.value();
    offset.top = top.value();
  } else {
    if (IsLtr(container_direction))
      offset.top = top.value();
    else
      offset.top = -bottom.value();
    offset.left = left.value();
  }
  return offset;
}

}  // namespace blink
