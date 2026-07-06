// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_

#include <optional>

#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

struct ShapeReferenceBox {
  std::optional<GeometryBox> geometry;
  std::optional<CoordBox> coord;
  std::optional<ShapeBox> shape;

  bool operator==(const ShapeReferenceBox&) const = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_
