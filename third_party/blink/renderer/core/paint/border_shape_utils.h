// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
struct PhysicalRect;

std::optional<BorderShapeReferenceRects> ComputeBorderShapeReferenceRects(
    const PhysicalRect& rect,
    const ComputedStyle& style,
    const LayoutObject& layout_object);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_
