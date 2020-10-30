/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/transforms/rotation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "ui/gfx/geometry/quaternion.h"

namespace blink {

using gfx::Quaternion;

namespace {

const double kEpsilon = 1e-5;
const double kAngleEpsilon = 1e-4;

Quaternion ComputeQuaternion(const Rotation& rotation) {
  return Quaternion::FromAxisAngle(rotation.axis.X(), rotation.axis.Y(),
                                   rotation.axis.Z(), deg2rad(rotation.angle));
}

FloatPoint3D NormalizeAxis(FloatPoint3D axis) {
  FloatPoint3D normalized(axis);
  double length = normalized.length();
  if (length > kEpsilon) {
    normalized.Normalize();
  } else {
    // Rotation angle is zero so the axis is arbitrary.
    normalized.Set(0, 0, 1);
  }
  return normalized;
}

Rotation ComputeRotation(Quaternion q) {
  double cos_half_angle = q.w();
  double interpolated_angle = rad2deg(2 * std::acos(cos_half_angle));
  FloatPoint3D interpolated_axis =
      NormalizeAxis(FloatPoint3D(q.x(), q.y(), q.z()));
  return Rotation(interpolated_axis, interpolated_angle);
}

}  // namespace

bool Rotation::GetCommonAxis(const Rotation& a,
                             const Rotation& b,
                             FloatPoint3D& result_axis,
                             double& result_angle_a,
                             double& result_angle_b) {
  result_axis = FloatPoint3D(0, 0, 1);
  result_angle_a = 0;
  result_angle_b = 0;

  bool is_zero_a = a.axis.IsZero() || fabs(a.angle) < kAngleEpsilon;
  bool is_zero_b = b.axis.IsZero() || fabs(b.angle) < kAngleEpsilon;

  if (is_zero_a && is_zero_b)
    return true;

  if (is_zero_a) {
    result_axis = NormalizeAxis(b.axis);
    result_angle_b = b.angle;
    return true;
  }

  if (is_zero_b) {
    result_axis = NormalizeAxis(a.axis);
    result_angle_a = a.angle;
    return true;
  }

  double dot = a.axis.Dot(b.axis);
  if (dot < 0)
    return false;

  double a_squared = a.axis.LengthSquared();
  double b_squared = b.axis.LengthSquared();
  double error = std::abs(1 - (dot * dot) / (a_squared * b_squared));
  if (error > kAngleEpsilon)
    return false;

  result_axis = NormalizeAxis(a.axis);
  result_angle_a = a.angle;
  result_angle_b = b.angle;
  return true;
}

Rotation Rotation::Slerp(const Rotation& from,
                         const Rotation& to,
                         double progress) {
  double from_angle;
  double to_angle;
  FloatPoint3D axis;
  if (GetCommonAxis(from, to, axis, from_angle, to_angle))
    return Rotation(axis, blink::Blend(from_angle, to_angle, progress));

  Quaternion qa = ComputeQuaternion(from);
  Quaternion qb = ComputeQuaternion(to);
  Quaternion qc = qa.Slerp(qb, progress);

  return ComputeRotation(qc);
}

Rotation Rotation::Add(const Rotation& a, const Rotation& b) {
  double angle_a;
  double angle_b;
  FloatPoint3D axis;
  if (GetCommonAxis(a, b, axis, angle_a, angle_b))
    return Rotation(axis, angle_a + angle_b);

  Quaternion qa = ComputeQuaternion(a);
  Quaternion qb = ComputeQuaternion(b);
  Quaternion qc = qa * qb;
  if (qc.w() < 0) {
    // Choose the equivalent rotation with the smaller angle.
    qc = qc.flip();
  }

  return ComputeRotation(qc);
}

}  // namespace blink
