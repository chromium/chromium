// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_

#include <variant>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

class BasicShape;
class CSSProperty;
class CSSValue;
class ComputedStyle;
class ComputedStyleBuilder;

using ShapeReferenceBox =
    std::variant<std::monostate, GeometryBox, CoordBox, ShapeBox>;

struct BasicShapeInfo {
  STACK_ALLOCATED();

 public:
  const BasicShape* shape = nullptr;
  ShapeReferenceBox box;
};

struct BasicShapeCssInfo {
  STACK_ALLOCATED();

 public:
  const CSSValue* shape = nullptr;
  ShapeReferenceBox box;
};

namespace shape_property_functions {

BasicShapeInfo GetBasicShape(const CSSProperty&, const ComputedStyle&);
void SetBasicShape(const CSSProperty&,
                   const BasicShapeInfo&,
                   ComputedStyleBuilder&);

BasicShapeCssInfo GetCssBasicShape(const CSSProperty&, const CSSValue&);

}  // namespace shape_property_functions
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SHAPE_PROPERTY_FUNCTIONS_H_
