// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GEOMETRY_BOX_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GEOMETRY_BOX_UTILS_H_

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class LayoutBoxModelObject;

namespace GeometryBoxUtils {

// Returns the outsets to apply to a border box to get the reference box for a
// given GeometryBox.
PhysicalBoxStrut ReferenceBoxBorderBoxOutsets(
    GeometryBox geometry_box,
    const LayoutBoxModelObject& object);

}  // namespace GeometryBoxUtils

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_GEOMETRY_BOX_UTILS_H_
