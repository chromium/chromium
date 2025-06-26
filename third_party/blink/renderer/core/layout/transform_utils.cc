// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/transform_utils.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
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
      NOTREACHED();
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

std::optional<gfx::Transform> GetTransformForChildFragment(
    const PhysicalBoxFragment& child_fragment,
    const LayoutObject& container_object,
    PhysicalSize container_size) {
  const auto* child_layout_object = child_fragment.GetLayoutObject();
  DCHECK(child_layout_object);

  if (!child_layout_object->ShouldUseTransformFromContainer(
          &container_object)) {
    return std::nullopt;
  }

  std::optional<gfx::Transform> fragment_transform;
  if (!child_fragment.IsOnlyForNode()) {
    // If we're fragmented, there's no correct transform stored for
    // us. Calculate it now.
    fragment_transform.emplace();
    fragment_transform->MakeIdentity();
    const PhysicalRect reference_box = ComputeReferenceBox(child_fragment);
    child_fragment.Style().ApplyTransform(
        *fragment_transform, &To<LayoutBox>(container_object), reference_box,
        ComputedStyle::kIncludeTransformOperations,
        ComputedStyle::kIncludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
  }

  gfx::Transform transform;
  child_layout_object->GetTransformFromContainer(
      &container_object, PhysicalOffset(), transform, &container_size,
      base::OptionalToPtr(fragment_transform));

  return transform;
}

void UpdateTransformState(const PhysicalFragment& child_fragment,
                          PhysicalOffset child_offset,
                          const LayoutObject& container_object,
                          PhysicalSize container_size,
                          TransformState* transform_state) {
  TransformState::TransformAccumulation accumulation =
      container_object.StyleRef().Preserves3D()
          ? TransformState::kAccumulateTransform
          : TransformState::kFlattenTransform;

  if (child_fragment.IsCSSBox()) {
    if (std::optional<gfx::Transform> transform = GetTransformForChildFragment(
            To<PhysicalBoxFragment>(child_fragment), container_object,
            container_size)) {
      if (const LayoutObject* child_object = child_fragment.GetLayoutObject()) {
        if (child_object->ShouldUseTransformFromContainer(&container_object)) {
          transform_state->ApplyTransform(*transform, accumulation);
        }
      }
    }
  }
  transform_state->Move(child_offset, accumulation);
}

}  // namespace blink
