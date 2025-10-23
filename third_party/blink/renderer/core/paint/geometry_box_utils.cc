// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/geometry_box_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"

namespace blink::GeometryBoxUtils {

// TODO(crbug.com/1473440): Convert this to take a PhysicalBoxFragment
// instead of a LayoutBoxModelObject.
PhysicalBoxStrut ReferenceBoxBorderBoxOutsets(
    GeometryBox geometry_box,
    const LayoutBoxModelObject& object) {
  // It is complex to map from an SVG border box to a reference box (for
  // example, `GeometryBox::kViewBox` is independent of the border box) so we
  // use `SVGResources::ReferenceBoxForEffects` for SVG reference boxes.
  CHECK(!object.IsSVGChild());

  switch (geometry_box) {
    case GeometryBox::kPaddingBox:
      return -object.BorderOutsets();
    case GeometryBox::kContentBox:
    case GeometryBox::kFillBox:
      return -(object.BorderOutsets() + object.PaddingOutsets());
    case GeometryBox::kMarginBox:
      return object.MarginOutsets();
    case GeometryBox::kHalfBorderBox: {
      // Half-border-box is halfway between border-box (offset 0) and
      // padding-box (offset -border), so the offset is -border/2
      PhysicalBoxStrut border_outsets = object.BorderOutsets();
      return PhysicalBoxStrut(
          -border_outsets.top / 2, -border_outsets.right / 2,
          -border_outsets.bottom / 2, -border_outsets.left / 2);
    }
    case GeometryBox::kBorderBox:
    case GeometryBox::kStrokeBox:
    case GeometryBox::kViewBox:
      return PhysicalBoxStrut();
  }
}

}  // namespace blink::GeometryBoxUtils
