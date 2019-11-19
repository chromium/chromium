// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_utils.h"

#include <cmath>

#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

DOMFloat32Array* transformationMatrixToDOMFloat32Array(
    const TransformationMatrix& matrix) {
  float array[] = {
      static_cast<float>(matrix.M11()), static_cast<float>(matrix.M12()),
      static_cast<float>(matrix.M13()), static_cast<float>(matrix.M14()),
      static_cast<float>(matrix.M21()), static_cast<float>(matrix.M22()),
      static_cast<float>(matrix.M23()), static_cast<float>(matrix.M24()),
      static_cast<float>(matrix.M31()), static_cast<float>(matrix.M32()),
      static_cast<float>(matrix.M33()), static_cast<float>(matrix.M34()),
      static_cast<float>(matrix.M41()), static_cast<float>(matrix.M42()),
      static_cast<float>(matrix.M43()), static_cast<float>(matrix.M44())};

  return DOMFloat32Array::Create(array, 16);
}

TransformationMatrix DOMFloat32ArrayToTransformationMatrix(DOMFloat32Array* m) {
  DCHECK_EQ(m->length(), 16u);

  auto* data = m->Data();

  return TransformationMatrix(
      static_cast<double>(data[0]), static_cast<double>(data[1]),
      static_cast<double>(data[2]), static_cast<double>(data[3]),
      static_cast<double>(data[4]), static_cast<double>(data[5]),
      static_cast<double>(data[6]), static_cast<double>(data[7]),
      static_cast<double>(data[8]), static_cast<double>(data[9]),
      static_cast<double>(data[10]), static_cast<double>(data[11]),
      static_cast<double>(data[12]), static_cast<double>(data[13]),
      static_cast<double>(data[14]), static_cast<double>(data[15]));
}

TransformationMatrix WTFFloatVectorToTransformationMatrix(
    const WTF::Vector<float>& m) {
  return TransformationMatrix(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                              m[8], m[9], m[10], m[11], m[12], m[13], m[14],
                              m[15]);
}

// Normalize to have length = 1.0
DOMPointReadOnly* makeNormalizedQuaternion(double x,
                                           double y,
                                           double z,
                                           double w) {
  double length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
  if (length == 0.0) {
    // Return a default value instead of crashing.
    return DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }
  return DOMPointReadOnly::Create(x / length, y / length, z / length,
                                  w / length);
}

}  // namespace blink
