// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

namespace blink {

// static
LogicalSize LogicalSize::AspectRatioFromFloatSize(const FloatSize& size) {
  // Try and preserve as much precision as possible in the LayoutUnit space.
  // For ratios with values smaller than 1.0f, pre-divide to reduce loss. E.g:
  // "0.25f/0.01f" becomes "25/1" instead of "0.25/0.015625".
  if ((size.Width() >= 1.0f && size.Height() >= 1.0f) || size.IsEmpty())
    return LogicalSize(LayoutUnit(size.Width()), LayoutUnit(size.Height()));

  if (size.Width() > size.Height())
    return LogicalSize(LayoutUnit(size.Width() / size.Height()), LayoutUnit(1));

  return LogicalSize(LayoutUnit(1), LayoutUnit(size.Height() / size.Width()));
}

std::ostream& operator<<(std::ostream& stream, const LogicalSize& value) {
  return stream << value.inline_size << "x" << value.block_size;
}

}  // namespace blink
