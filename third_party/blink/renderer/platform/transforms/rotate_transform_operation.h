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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_ROTATE_TRANSFORM_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_ROTATE_TRANSFORM_OPERATION_H_

#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "third_party/blink/renderer/platform/transforms/transform_operation.h"

namespace blink {

class PLATFORM_EXPORT RotateTransformOperation : public TransformOperation {
 public:
  static scoped_refptr<RotateTransformOperation> Create(double angle,
                                                        OperationType type) {
    return Create(Rotation(FloatPoint3D(0, 0, 1), angle), type);
  }

  static scoped_refptr<RotateTransformOperation> Create(double x,
                                                        double y,
                                                        double z,
                                                        double angle,
                                                        OperationType type) {
    return Create(Rotation(FloatPoint3D(x, y, z), angle), type);
  }

  static scoped_refptr<RotateTransformOperation> Create(
      const Rotation& rotation,
      OperationType type) {
    DCHECK(IsMatchingOperationType(type));
    return base::AdoptRef(new RotateTransformOperation(rotation, type));
  }

  bool operator==(const RotateTransformOperation& other) const {
    return *this == static_cast<const TransformOperation&>(other);
  }

  double X() const { return rotation_.axis.X(); }
  double Y() const { return rotation_.axis.Y(); }
  double Z() const { return rotation_.axis.Z(); }
  double Angle() const { return rotation_.angle; }
  const FloatPoint3D& Axis() const { return rotation_.axis; }

  static bool GetCommonAxis(const RotateTransformOperation*,
                            const RotateTransformOperation*,
                            FloatPoint3D& result_axis,
                            double& result_angle_a,
                            double& result_angle_b);

  bool CanBlendWith(const TransformOperation& other) const override;
  OperationType GetType() const override { return type_; }
  OperationType PrimitiveType() const final { return kRotate3D; }

  void Apply(TransformationMatrix& transform,
             const FloatSize& /*borderBoxSize*/) const override {
    transform.Rotate3d(rotation_);
  }

  static bool IsMatchingOperationType(OperationType type) {
    return type == kRotate || type == kRotateX || type == kRotateY ||
           type == kRotateZ || type == kRotate3D;
  }

 protected:
  bool operator==(const TransformOperation&) const override;

  bool HasNonTrivial3DComponent() const override {
    return Angle() && (X() || Y());
  }

  scoped_refptr<TransformOperation> Accumulate(
      const TransformOperation& other) override;
  scoped_refptr<TransformOperation> Blend(
      const TransformOperation* from,
      double progress,
      bool blend_to_identity = false) override;
  scoped_refptr<TransformOperation> Zoom(double factor) override {
    return this;
  }

  RotateTransformOperation(const Rotation& rotation, OperationType type)
      : rotation_(rotation), type_(type) {}

  const Rotation rotation_;
  const OperationType type_;
};

DEFINE_TRANSFORM_TYPE_CASTS(RotateTransformOperation);

class PLATFORM_EXPORT RotateAroundOriginTransformOperation final
    : public RotateTransformOperation {
 public:
  static scoped_refptr<RotateAroundOriginTransformOperation>
  Create(double angle, double origin_x, double origin_y) {
    return base::AdoptRef(
        new RotateAroundOriginTransformOperation(angle, origin_x, origin_y));
  }

  void Apply(TransformationMatrix&, const FloatSize&) const override;

  static bool IsMatchingOperationType(OperationType type) {
    return type == kRotateAroundOrigin;
  }

 private:
  RotateAroundOriginTransformOperation(double angle,
                                       double origin_x,
                                       double origin_y);

  bool operator==(const TransformOperation&) const override;

  scoped_refptr<TransformOperation> Blend(
      const TransformOperation* from,
      double progress,
      bool blend_to_identity = false) override;
  scoped_refptr<TransformOperation> Zoom(double factor) override;

  double origin_x_;
  double origin_y_;
};

DEFINE_TRANSFORM_TYPE_CASTS(RotateAroundOriginTransformOperation);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRANSFORMS_ROTATE_TRANSFORM_OPERATION_H_
