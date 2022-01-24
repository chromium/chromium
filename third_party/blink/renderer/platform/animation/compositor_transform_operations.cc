// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_transform_operations.h"

#include "skia/ext/skia_matrix_44.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace blink {

const gfx::TransformOperations&
CompositorTransformOperations::AsGfxTransformOperations() const {
  return transform_operations_;
}

gfx::TransformOperations
CompositorTransformOperations::ReleaseGfxTransformOperations() {
  return std::move(transform_operations_);
}

bool CompositorTransformOperations::CanBlendWith(
    const blink::CompositorTransformOperations& other) const {
  return transform_operations_.CanBlendWith(other.transform_operations_);
}

void CompositorTransformOperations::AppendTranslate(double x,
                                                    double y,
                                                    double z) {
  transform_operations_.AppendTranslate(
      SkDoubleToScalar(x), SkDoubleToScalar(y), SkDoubleToScalar(z));
}

void CompositorTransformOperations::AppendRotate(double x,
                                                 double y,
                                                 double z,
                                                 double degrees) {
  transform_operations_.AppendRotate(SkDoubleToScalar(x), SkDoubleToScalar(y),
                                     SkDoubleToScalar(z),
                                     SkDoubleToScalar(degrees));
}

void CompositorTransformOperations::AppendScale(double x, double y, double z) {
  transform_operations_.AppendScale(SkDoubleToScalar(x), SkDoubleToScalar(y),
                                    SkDoubleToScalar(z));
}

void CompositorTransformOperations::AppendSkewX(double x) {
  transform_operations_.AppendSkewX(SkDoubleToScalar(x));
}

void CompositorTransformOperations::AppendSkewY(double y) {
  transform_operations_.AppendSkewY(SkDoubleToScalar(y));
}

void CompositorTransformOperations::AppendSkew(double x, double y) {
  transform_operations_.AppendSkew(SkDoubleToScalar(x), SkDoubleToScalar(y));
}

void CompositorTransformOperations::AppendPerspective(
    absl::optional<double> depth) {
  if (depth) {
    transform_operations_.AppendPerspective(
        SkDoubleToScalar(std::max(*depth, 1.0)));
  } else {
    transform_operations_.AppendPerspective(absl::nullopt);
  }
}

void CompositorTransformOperations::AppendMatrix(const skia::Matrix44& matrix) {
  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = matrix;
  transform_operations_.AppendMatrix(transform);
}

bool CompositorTransformOperations::IsIdentity() const {
  return transform_operations_.IsIdentity();
}

}  // namespace blink
