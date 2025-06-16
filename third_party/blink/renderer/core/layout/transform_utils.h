// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TRANSFORM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TRANSFORM_UTILS_H_

#include <optional>

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class PhysicalBoxFragment;

// Compute the transform reference box, based on the computed 'transform-box'
// property, for the specified entity.
PhysicalRect ComputeReferenceBox(const PhysicalBoxFragment&);
PhysicalRect ComputeReferenceBox(const LayoutBox&);

// Return the transform to apply to a child (e.g. for scrollable-overflow).
std::optional<gfx::Transform> GetTransformForChildFragment(
    const PhysicalBoxFragment& child_fragment,
    const LayoutObject& container_object,
    PhysicalSize container_size);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_TRANSFORM_UTILS_H_
