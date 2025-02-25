// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/skia_geometry_utils.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

SkMatrix AffineTransformToSkMatrix(const AffineTransform& source) {
  // SkMatrices are 3x3, so they have a concept of "perspective" in the bottom
  // row. blink::AffineTransform is a 2x3 matrix that can encode 2d rotations,
  // skew and translation, but has no perspective. Those parameters are set to
  // zero here. i.e.:

  //   INPUT           OUTPUT
  // | a c e |       | a c e |
  // | b d f | ----> | b d f |
  //                 | 0 0 1 |

  SkMatrix result;

  result.setScaleX(WebCoreDoubleToSkScalar(source.A()));
  result.setSkewX(WebCoreDoubleToSkScalar(source.C()));
  result.setTranslateX(WebCoreDoubleToSkScalar(source.E()));

  result.setScaleY(WebCoreDoubleToSkScalar(source.D()));
  result.setSkewY(WebCoreDoubleToSkScalar(source.B()));
  result.setTranslateY(WebCoreDoubleToSkScalar(source.F()));

  result.setPerspX(0);
  result.setPerspY(0);
  result.set(SkMatrix::kMPersp2, SK_Scalar1);

  return result;
}

SkM44 AffineTransformToSkM44(const AffineTransform& source) {
  //   INPUT           OUTPUT
  // | a c e |       | a c 0 e |
  // | b d f | ----> | b d 0 f |
  //                 | 0 0 1 0 |
  //                 | 0 0 0 1 |
  SkScalar a = WebCoreDoubleToSkScalar(source.A());
  SkScalar b = WebCoreDoubleToSkScalar(source.B());
  SkScalar c = WebCoreDoubleToSkScalar(source.C());
  SkScalar d = WebCoreDoubleToSkScalar(source.D());
  SkScalar e = WebCoreDoubleToSkScalar(source.E());
  SkScalar f = WebCoreDoubleToSkScalar(source.F());
  return SkM44(a, c, 0, e,   // row 0
               b, d, 0, f,   // row 1
               0, 0, 1, 0,   // row 2
               0, 0, 0, 1);  // row 3
}

}  // namespace blink
