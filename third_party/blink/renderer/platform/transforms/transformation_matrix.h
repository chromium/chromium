/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_

#include <string.h>  // for memcpy

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkM44.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

class AffineTransform;
class LayoutRect;

class PLATFORM_EXPORT TransformationMatrix : public gfx::Transform {
 public:
  using gfx::Transform::Transform;

  explicit TransformationMatrix(const AffineTransform&);

  // This is intentionally not explicit. It's temporary. The whole class
  // will be removed soon.
  // NOLINTNEXTLINE(google-explicit-constructor)
  TransformationMatrix(const gfx::Transform& t) {
    static_cast<gfx::Transform&>(*this) = t;
  }

  explicit TransformationMatrix(const SkM44& matrix);

  [[nodiscard]] static TransformationMatrix Affine(double a,
                                                   double b,
                                                   double c,
                                                   double d,
                                                   double e,
                                                   double f) {
    return TransformationMatrix(gfx::Transform::Affine(a, b, c, d, e, f));
  }

  [[nodiscard]] static TransformationMatrix ColMajor(double r0c0,
                                                     double r1c0,
                                                     double r2c0,
                                                     double r3c0,
                                                     double r0c1,
                                                     double r1c1,
                                                     double r2c1,
                                                     double r3c1,
                                                     double r0c2,
                                                     double r1c2,
                                                     double r2c2,
                                                     double r3c2,
                                                     double r0c3,
                                                     double r1c3,
                                                     double r2c3,
                                                     double r3c3) {
    return TransformationMatrix(gfx::Transform::ColMajor(
        r0c0, r1c0, r2c0, r3c0, r0c1, r1c1, r2c1, r3c1, r0c2, r1c2, r2c2, r3c2,
        r0c3, r1c3, r2c3, r3c3));
  }

  [[nodiscard]] static TransformationMatrix ColMajor(const double v[16]) {
    return TransformationMatrix(gfx::Transform::ColMajor(v));
  }

  [[nodiscard]] static TransformationMatrix ColMajorF(const float v[16]) {
    return TransformationMatrix(gfx::Transform::ColMajorF(v));
  }

  [[nodiscard]] static TransformationMatrix MakeTranslation(double tx,
                                                            double ty) {
    return TransformationMatrix(gfx::Transform::MakeTranslation(tx, ty));
  }
  [[nodiscard]] static TransformationMatrix MakeScale(double scale) {
    return MakeScale(scale, scale);
  }
  [[nodiscard]] static TransformationMatrix MakeScale(double sx, double sy) {
    return TransformationMatrix(gfx::Transform::MakeScale(sx, sy));
  }

  using gfx::Transform::MapRect;
  [[nodiscard]] LayoutRect MapRect(const LayoutRect&) const;

  // Decompose 2-D transform matrix into its component parts.
  typedef struct {
    double scale_x, scale_y;
    double skew_xy;
    double translate_x, translate_y;
    double angle;
  } Decomposed2dType;

  [[nodiscard]] bool Decompose2D(Decomposed2dType&) const;
  void Recompose(const gfx::DecomposedTransform&);
  void Recompose2D(const Decomposed2dType&);
  void Blend(const TransformationMatrix& from, double progress);
  void Blend2D(const TransformationMatrix& from, double progress);

  bool IsAffine() const { return Is2dTransform(); }

  [[nodiscard]] AffineTransform ToAffineTransform() const;

  // *this = *this * t
  TransformationMatrix& operator*=(const TransformationMatrix& t) {
    PreConcat(t);
    return *this;
  }

  // result = *this * t
  TransformationMatrix operator*(const TransformationMatrix& t) const {
    TransformationMatrix result = *this;
    result.PreConcat(t);
    return result;
  }

  // This method converts double to float using ClampToFloat() which converts
  // NaN to 0 and +-infinity to minimum/maximum value of float.
  SkM44 ToSkM44() const;

  const gfx::Transform& ToTransform() const { return *this; }

 private:
  static float ClampToFloat(double value) {
    return ClampToWithNaNTo0<float>(value);
  }
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const TransformationMatrix&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_TRANSFORMATION_MATRIX_H_
