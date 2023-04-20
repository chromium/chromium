// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"

namespace blink {

LayoutRectOutsets LineOrientationLayoutRectOutsets(
    const LayoutRectOutsets& outsets,
    WritingMode writing_mode) {
  if (!IsHorizontalWritingMode(writing_mode)) {
    return LayoutRectOutsets(outsets.Left(), outsets.Bottom(), outsets.Right(),
                             outsets.Top());
  }
  return outsets;
}

}  // namespace blink
