// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/transform_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

PhysicalRect ComputeReferenceBoxInternal(const PhysicalBoxFragment& fragment,
                                         PhysicalRect border_box_rect) {
  PhysicalRect fragment_reference_box = border_box_rect;
  switch (fragment.Style().UsedTransformBox(
      ComputedStyle::TransformBoxContext::kLayoutBox)) {
    case ETransformBox::kContentBox:
      fragment_reference_box.Contract(fragment.Borders() + fragment.Padding());
      fragment_reference_box.size.ClampNegativeToZero();
      break;
    case ETransformBox::kBorderBox:
      break;
    case ETransformBox::kFillBox:
    case ETransformBox::kStrokeBox:
    case ETransformBox::kViewBox:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return fragment_reference_box;
}

}  // namespace

PhysicalRect ComputeReferenceBox(const PhysicalBoxFragment& fragment) {
  return ComputeReferenceBoxInternal(fragment, fragment.LocalRect());
}

PhysicalRect ComputeReferenceBox(const LayoutBox& box) {
  // If the box is fragment-less return an empty reference box.
  if (box.PhysicalFragmentCount() == 0u) {
    return PhysicalRect();
  }
  return ComputeReferenceBoxInternal(*box.GetPhysicalFragment(0),
                                     box.PhysicalBorderBoxRect());
}

}  // namespace blink
