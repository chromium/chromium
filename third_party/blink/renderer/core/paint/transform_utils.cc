// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/transform_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

PhysicalRect ComputeReferenceBox(const NGPhysicalBoxFragment& fragment) {
  PhysicalRect fragment_reference_box = fragment.LocalRect();
  switch (fragment.Style().TransformBox()) {
    case ETransformBox::kFillBox:
    case ETransformBox::kContentBox:
      fragment_reference_box.Contract(fragment.Borders() + fragment.Padding());
      fragment_reference_box.size.ClampNegativeToZero();
      break;
    case ETransformBox::kStrokeBox:
    case ETransformBox::kBorderBox:
    case ETransformBox::kViewBox:
      break;
  }
  return fragment_reference_box;
}

PhysicalRect ComputeReferenceBox(const LayoutBox& box) {
  switch (box.StyleRef().TransformBox()) {
    case ETransformBox::kFillBox:
    case ETransformBox::kContentBox:
      return box.PhysicalContentBoxRect();
    case ETransformBox::kStrokeBox:
    case ETransformBox::kBorderBox:
    case ETransformBox::kViewBox:
      return box.PhysicalBorderBoxRect();
  }
}

}  // namespace blink
