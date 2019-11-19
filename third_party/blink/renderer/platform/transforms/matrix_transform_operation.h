/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_MATRIX_TRANSFORM_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_MATRIX_TRANSFORM_OPERATION_H_

#include "third_party/blink/renderer/platform/transforms/transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class PLATFORM_EXPORT MatrixTransformOperation final
    : public TransformOperation {
 public:
  static scoped_refptr<MatrixTransformOperation> Create(double a,
                                                        double b,
                                                        double c,
                                                        double d,
                                                        double e,
                                                        double f) {
    return base::AdoptRef(new MatrixTransformOperation(a, b, c, d, e, f));
  }

  static scoped_refptr<MatrixTransformOperation> Create(
      const TransformationMatrix& t) {
    return base::AdoptRef(new MatrixTransformOperation(t));
  }

  TransformationMatrix Matrix() const {
    return TransformationMatrix(a_, b_, c_, d_, e_, f_);
  }

  bool CanBlendWith(const TransformOperation& other) const override {
    return false;
  }

  static bool IsMatchingOperationType(OperationType type) {
    return type == kMatrix;
  }

 private:
  OperationType GetType() const override { return kMatrix; }

  bool operator==(const TransformOperation& o) const override {
    if (!IsSameType(o))
      return false;

    const MatrixTransformOperation* m =
        static_cast<const MatrixTransformOperation*>(&o);
    return a_ == m->a_ && b_ == m->b_ && c_ == m->c_ && d_ == m->d_ &&
           e_ == m->e_ && f_ == m->f_;
  }

  void Apply(TransformationMatrix& transform, const FloatSize&) const override {
    TransformationMatrix matrix(a_, b_, c_, d_, e_, f_);
    transform.Multiply(matrix);
  }

  scoped_refptr<TransformOperation> Accumulate(
      const TransformOperation&) override;

  scoped_refptr<TransformOperation> Blend(
      const TransformOperation* from,
      double progress,
      bool blend_to_identity = false) override;
  scoped_refptr<TransformOperation> Zoom(double factor) final;

  bool PreservesAxisAlignment() const final {
    return Matrix().Preserves2dAxisAlignment();
  }

  MatrixTransformOperation(double a,
                           double b,
                           double c,
                           double d,
                           double e,
                           double f)
      : a_(a), b_(b), c_(c), d_(d), e_(e), f_(f) {}

  MatrixTransformOperation(const TransformationMatrix& t)
      : a_(t.A()), b_(t.B()), c_(t.C()), d_(t.D()), e_(t.E()), f_(t.F()) {}

  double a_;
  double b_;
  double c_;
  double d_;
  double e_;
  double f_;
};

DEFINE_TRANSFORM_TYPE_CASTS(MatrixTransformOperation);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_MATRIX_TRANSFORM_OPERATION_H_
