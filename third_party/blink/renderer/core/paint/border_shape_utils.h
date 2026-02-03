// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_

#include "third_party/blink/renderer/core/paint/border_shape_painter.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
struct PhysicalRect;

std::optional<BorderShapeReferenceRects> ComputeBorderShapeReferenceRects(
    const PhysicalRect& rect,
    const ComputedStyle& style,
    const LayoutObject& layout_object);

// Computes the outer path for border-shape clipping/hit-testing.
// Uses the provided reference rect and the layout object to resolve
// reference-box-relative shapes.
Path ComputeBorderShapeOuterPath(const ComputedStyle& style,
                                 const PhysicalRect& rect,
                                 const LayoutObject* layout_object);

struct DerivedStroke {
  float thickness;
  Color color;
};

// https://drafts.csswg.org/css-borders-4/#relevant-side-for-border-shape
DerivedStroke RelevantSideForBorderShape(const ComputedStyle& style);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BORDER_SHAPE_UTILS_H_
