// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

DOMPointReadOnly* DOMPointReadOnly::Create(double x,
                                           double y,
                                           double z,
                                           double w) {
  return MakeGarbageCollected<DOMPointReadOnly>(x, y, z, w);
}

ScriptValue DOMPointReadOnly::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("x", x());
  result.AddNumber("y", y());
  result.AddNumber("z", z());
  result.AddNumber("w", w());
  return result.GetScriptValue();
}

DOMPointReadOnly* DOMPointReadOnly::fromPoint(const DOMPointInit* other) {
  return MakeGarbageCollected<DOMPointReadOnly>(other->x(), other->y(),
                                                other->z(), other->w());
}

DOMPoint* DOMPointReadOnly::matrixTransform(DOMMatrixInit* other,
                                            ExceptionState& exception_state) {
  DOMMatrixReadOnly* matrix =
      DOMMatrixReadOnly::fromMatrix(other, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (matrix->is2D() && z() == 0 && w() == 1) {
    double transformed_x =
        x() * matrix->m11() + y() * matrix->m21() + matrix->m41();
    double transformed_y =
        x() * matrix->m12() + y() * matrix->m22() + matrix->m42();
    return DOMPoint::Create(transformed_x, transformed_y, 0, 1);
  }

  double transformed_x = x() * matrix->m11() + y() * matrix->m21() +
                         z() * matrix->m31() + w() * matrix->m41();
  double transformed_y = x() * matrix->m12() + y() * matrix->m22() +
                         z() * matrix->m32() + w() * matrix->m42();
  double transformed_z = x() * matrix->m13() + y() * matrix->m23() +
                         z() * matrix->m33() + w() * matrix->m43();
  double transformed_w = x() * matrix->m14() + y() * matrix->m24() +
                         z() * matrix->m34() + w() * matrix->m44();
  return DOMPoint::Create(transformed_x, transformed_y, transformed_z,
                          transformed_w);
}

DOMPointReadOnly::DOMPointReadOnly(double x, double y, double z, double w)
    : x_(x), y_(y), z_(z), w_(w) {}

}  // namespace blink
