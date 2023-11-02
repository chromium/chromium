// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_ORIENTATION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_ORIENTATION_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

// Produces a new LayoutRectOutsets in line orientation
// (https://www.w3.org/TR/css-writing-modes-3/#line-orientation), whose
// - |top| is the logical 'over',
// - |right| is the logical 'line right',
// - |bottom| is the logical 'under',
// - |left| is the logical 'line left'.
CORE_EXPORT LayoutRectOutsets
LineOrientationLayoutRectOutsets(const LayoutRectOutsets&, WritingMode);

// The same as |logicalOutsets|, but also adjusting for flipped lines.
CORE_EXPORT LayoutRectOutsets
LineOrientationLayoutRectOutsetsWithFlippedLines(const LayoutRectOutsets&,
                                                 WritingMode);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_ORIENTATION_UTILS_H_
