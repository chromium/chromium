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
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

using gfx::Quaternion;

namespace {

const double kAngleEpsilon = 1e-4;

Quaternion ComputeQuaternion(const Rotation& rotation) {
  return Quaternion::FromAxisAngle(rotation.axis.x(), rotation.axis.y(),
                                   rotation.axis.z(), Deg2rad(rotation.angle));
}

gfx::Vector3dF NormalizeAxis(gfx::Vector3dF axis) {
  gfx::Vector3dF normalized;
  if (axis.GetNormalized(&normalized))
    return normalized;
  // Rotation angle is zero so the axis is arbitrary.
  return gfx::Vector3dF(0, 0, 1);
}

Rotation ComputeRotation(Quaternion q) {
  double cos_half_angle = q.w();
  double interpolated_angle = Rad2deg(2 * std::acos(cos_half_angle));
  gfx::Vector3dF interpolated_axis =
      NormalizeAxis(gfx::Vector3dF(q.x(), q.y(), q.z()));
  return Rotation(interpolated_axis, interpolated_angle);
}

}  // namespace

bool Rotation::GetCommonAxis(const Rotation& a,
                             const Rotation& b,
                             gfx::Vector3dF& result_axis,
                             double& result_angle_a,
                             double& result_angle_b) {
  result_axis = gfx::Vector3dF(0, 0, 1);
  result_angle_a = 0;
  result_angle_b = 0;

  // We have to consider two definitions of "is zero" here, because we
  // sometimes need to preserve (as an interpolation result) and expose
  // to web content an axis that is associated with a zero angle.  Thus
  // we consider having a zero axis stronger than having a zero angle.
  bool a_has_zero_axis = a.axis.IsZero();
  bool b_has_zero_axis = b.axis.IsZero();
  bool is_zero_a, is_zero_b;
  if (a_has_zero_axis || b_has_zero_axis) {
    is_zero_a = a_has_zero_axis;
    is_zero_b = b_has_zero_axis;
  } else {
    is_zero_a = fabs(a.angle) < kAngleEpsilon;
    is_zero_b = fabs(b.angle) < kAngleEpsilon;
  }

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

  double dot = gfx::DotProduct(a.axis, b.axis);
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
  gfx::Vector3dF axis;
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
  gfx::Vector3dF axis;
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
