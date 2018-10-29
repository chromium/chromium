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

#include "third_party/blink/renderer/platform/transforms/transform_operations.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/float_box.h"
#include "third_party/blink/renderer/platform/transforms/identity_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/interpolated_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"

namespace blink {

TransformOperations::TransformOperations(bool make_identity) {
  if (make_identity)
    operations_.push_back(IdentityTransformOperation::Create());
}

bool TransformOperations::operator==(const TransformOperations& o) const {
  if (operations_.size() != o.operations_.size())
    return false;

  wtf_size_t s = operations_.size();
  for (wtf_size_t i = 0; i < s; i++) {
    if (*operations_[i] != *o.operations_[i])
      return false;
  }

  return true;
}

bool TransformOperations::OperationsMatch(
    const TransformOperations& other) const {
  wtf_size_t num_operations = Operations().size();
  if (num_operations != other.Operations().size())
    return false;

  for (wtf_size_t i = 0; i < num_operations; ++i) {
    if (Operations()[i]->PrimitiveType() !=
        other.Operations()[i]->PrimitiveType()) {
      return false;
    }
  }
  return true;
}

TransformOperations TransformOperations::BlendByMatchingOperations(
    const TransformOperations& from,
    const double& progress) const {
  TransformOperations result;

  wtf_size_t from_size = from.Operations().size();
  wtf_size_t to_size = Operations().size();
  wtf_size_t size = std::max(from_size, to_size);
  for (wtf_size_t i = 0; i < size; i++) {
    scoped_refptr<TransformOperation> from_operation =
        (i < from_size) ? from.Operations()[i].get() : nullptr;
    scoped_refptr<TransformOperation> to_operation =
        (i < to_size) ? Operations()[i].get() : nullptr;
    scoped_refptr<TransformOperation> blended_operation =
        to_operation
            ? to_operation->Blend(from_operation.get(), progress)
            : (from_operation ? from_operation->Blend(nullptr, progress, true)
                              : nullptr);
    if (blended_operation)
      result.Operations().push_back(blended_operation);
    else {
      scoped_refptr<TransformOperation> identity_operation =
          IdentityTransformOperation::Create();
      if (progress > 0.5)
        result.Operations().push_back(to_operation ? to_operation
                                                   : identity_operation);
      else
        result.Operations().push_back(from_operation ? from_operation
                                                     : identity_operation);
    }
  }

  return result;
}

scoped_refptr<TransformOperation>
TransformOperations::BlendByUsingMatrixInterpolation(
    const TransformOperations& from,
    double progress) const {
  if (DependsOnBoxSize() || from.DependsOnBoxSize())
    return InterpolatedTransformOperation::Create(from, *this, progress);

  // Evaluate blended matrix here to avoid creating a nested data structure of
  // unbounded depth.
  TransformationMatrix from_transform;
  TransformationMatrix to_transform;
  from.Apply(FloatSize(), from_transform);
  Apply(FloatSize(), to_transform);
  to_transform.Blend(from_transform, progress);
  return Matrix3DTransformOperation::Create(to_transform);
}

// https://drafts.csswg.org/css-transforms-1/#interpolation-of-transforms
TransformOperations TransformOperations::Blend(const TransformOperations& from,
                                               double progress) const {
  if (from == *this || (!from.size() && !size()))
    return *this;

  // If either list is empty, use blendByMatchingOperations which has special
  // logic for this case.
  if (!from.size() || !size() || from.OperationsMatch(*this))
    return BlendByMatchingOperations(from, progress);

  TransformOperations result;
  result.Operations().push_back(
      BlendByUsingMatrixInterpolation(from, progress));
  return result;
}

static void FindCandidatesInPlane(double px,
                                  double py,
                                  double nz,
                                  double* candidates,
                                  int* num_candidates) {
  // The angle that this point is rotated with respect to the plane nz
  double phi = atan2(px, py);

  *num_candidates = 4;
  candidates[0] = phi;  // The element at 0deg (maximum x)

  for (int i = 1; i < *num_candidates; ++i)
    candidates[i] = candidates[i - 1] + M_PI_2;  // every 90 deg
  if (nz < 0.f) {
    for (int i = 0; i < *num_candidates; ++i)
      candidates[i] *= -1;
  }
}

// This method returns the bounding box that contains the starting point,
// the ending point, and any of the extrema (in each dimension) found across
// the circle described by the arc. These are then filtered to points that
// actually reside on the arc.
static void BoundingBoxForArc(const FloatPoint3D& point,
                              const RotateTransformOperation& from_transform,
                              const RotateTransformOperation& to_transform,
                              double min_progress,
                              double max_progress,
                              FloatBox& box) {
  double candidates[6];
  int num_candidates = 0;

  FloatPoint3D axis(from_transform.Axis());
  double from_degrees = from_transform.Angle();
  double to_degrees = to_transform.Angle();

  if (axis.Dot(to_transform.Axis()) < 0)
    to_degrees *= -1;

  from_degrees = Blend(from_degrees, to_transform.Angle(), min_progress);
  to_degrees = Blend(to_degrees, from_transform.Angle(), 1.0 - max_progress);
  if (from_degrees > to_degrees)
    std::swap(from_degrees, to_degrees);

  TransformationMatrix from_matrix;
  TransformationMatrix to_matrix;
  from_matrix.Rotate3d(from_transform.X(), from_transform.Y(),
                       from_transform.Z(), from_degrees);
  to_matrix.Rotate3d(from_transform.X(), from_transform.Y(), from_transform.Z(),
                     to_degrees);

  FloatPoint3D from_point = from_matrix.MapPoint(point);
  FloatPoint3D to_point = to_matrix.MapPoint(point);

  if (box.IsEmpty())
    box.SetOrigin(from_point);
  else
    box.ExpandTo(from_point);

  box.ExpandTo(to_point);

  switch (from_transform.GetType()) {
    case TransformOperation::kRotateX:
      FindCandidatesInPlane(point.Y(), point.Z(), from_transform.X(),
                            candidates, &num_candidates);
      break;
    case TransformOperation::kRotateY:
      FindCandidatesInPlane(point.Z(), point.X(), from_transform.Y(),
                            candidates, &num_candidates);
      break;
    case TransformOperation::kRotateZ:
      FindCandidatesInPlane(point.X(), point.Y(), from_transform.Z(),
                            candidates, &num_candidates);
      break;
    default: {
      FloatPoint3D normal = axis;
      if (normal.IsZero())
        return;
      normal.Normalize();
      FloatPoint3D origin;
      FloatPoint3D to_point = point - origin;
      FloatPoint3D center = origin + normal * to_point.Dot(normal);
      FloatPoint3D v1 = point - center;
      if (v1.IsZero())
        return;

      v1.Normalize();
      FloatPoint3D v2 = normal.Cross(v1);
      // v1 is the basis vector in the direction of the point.
      // i.e. with a rotation of 0, v1 is our +x vector.
      // v2 is a perpenticular basis vector of our plane (+y).

      // Take the parametric equation of a circle.
      // (x = r*cos(t); y = r*sin(t);
      // We can treat that as a circle on the plane v1xv2
      // From that we get the parametric equations for a circle on the
      // plane in 3d space of
      // x(t) = r*cos(t)*v1.x + r*sin(t)*v2.x + cx
      // y(t) = r*cos(t)*v1.y + r*sin(t)*v2.y + cy
      // z(t) = r*cos(t)*v1.z + r*sin(t)*v2.z + cz
      // taking the derivative of (x, y, z) and solving for 0 gives us our
      // maximum/minimum x, y, z values
      // x'(t) = r*cos(t)*v2.x - r*sin(t)*v1.x = 0
      // tan(t) = v2.x/v1.x
      // t = atan2(v2.x, v1.x) + n*M_PI;

      candidates[0] = atan2(v2.X(), v1.X());
      candidates[1] = candidates[0] + M_PI;
      candidates[2] = atan2(v2.Y(), v1.Y());
      candidates[3] = candidates[2] + M_PI;
      candidates[4] = atan2(v2.Z(), v1.Z());
      candidates[5] = candidates[4] + M_PI;
      num_candidates = 6;
    } break;
  }

  double min_radians = deg2rad(from_degrees);
  double max_radians = deg2rad(to_degrees);
  // Once we have the candidates, we now filter them down to ones that
  // actually live on the arc, rather than the entire circle.
  for (int i = 0; i < num_candidates; ++i) {
    double radians = candidates[i];

    while (radians < min_radians)
      radians += 2.0 * M_PI;
    while (radians > max_radians)
      radians -= 2.0 * M_PI;
    if (radians < min_radians)
      continue;

    TransformationMatrix rotation;
    rotation.Rotate3d(axis.X(), axis.Y(), axis.Z(), rad2deg(radians));
    box.ExpandTo(rotation.MapPoint(point));
  }
}

bool TransformOperations::BlendedBoundsForBox(const FloatBox& box,
                                              const TransformOperations& from,
                                              const double& min_progress,
                                              const double& max_progress,
                                              FloatBox* bounds) const {
  int from_size = from.Operations().size();
  int to_size = Operations().size();
  int size = std::max(from_size, to_size);

  *bounds = box;
  for (int i = size - 1; i >= 0; i--) {
    scoped_refptr<TransformOperation> from_operation =
        (i < from_size) ? from.Operations()[i] : nullptr;
    scoped_refptr<TransformOperation> to_operation =
        (i < to_size) ? Operations()[i] : nullptr;

    DCHECK(from_operation || to_operation);
    TransformOperation::OperationType interpolation_type =
        to_operation ? to_operation->GetType() : from_operation->GetType();
    if (from_operation && to_operation &&
        !from_operation->CanBlendWith(*to_operation.get()))
      return false;

    switch (interpolation_type) {
      case TransformOperation::kIdentity:
        bounds->ExpandTo(box);
        continue;
      case TransformOperation::kTranslate:
      case TransformOperation::kTranslateX:
      case TransformOperation::kTranslateY:
      case TransformOperation::kTranslateZ:
      case TransformOperation::kTranslate3D:
      case TransformOperation::kScale:
      case TransformOperation::kScaleX:
      case TransformOperation::kScaleY:
      case TransformOperation::kScaleZ:
      case TransformOperation::kScale3D:
      case TransformOperation::kSkew:
      case TransformOperation::kSkewX:
      case TransformOperation::kSkewY:
      case TransformOperation::kPerspective: {
        scoped_refptr<TransformOperation> from_transform;
        scoped_refptr<TransformOperation> to_transform;
        if (!to_operation) {
          from_transform = from_operation->Blend(to_operation.get(),
                                                 1 - min_progress, false);
          to_transform = from_operation->Blend(to_operation.get(),
                                               1 - max_progress, false);
        } else {
          from_transform =
              to_operation->Blend(from_operation.get(), min_progress, false);
          to_transform =
              to_operation->Blend(from_operation.get(), max_progress, false);
        }
        if (!from_transform || !to_transform)
          continue;
        TransformationMatrix from_matrix;
        TransformationMatrix to_matrix;
        from_transform->Apply(from_matrix, FloatSize());
        to_transform->Apply(to_matrix, FloatSize());
        FloatBox from_box = *bounds;
        FloatBox to_box = *bounds;
        from_matrix.TransformBox(from_box);
        to_matrix.TransformBox(to_box);
        *bounds = from_box;
        bounds->ExpandTo(to_box);
        continue;
      }
      case TransformOperation::kRotate:  // This is also RotateZ
      case TransformOperation::kRotate3D:
      case TransformOperation::kRotateX:
      case TransformOperation::kRotateY: {
        scoped_refptr<RotateTransformOperation> identity_rotation;
        const RotateTransformOperation* from_rotation = nullptr;
        const RotateTransformOperation* to_rotation = nullptr;
        if (from_operation) {
          from_rotation = static_cast<const RotateTransformOperation*>(
              from_operation.get());
          if (from_rotation->Axis().IsZero())
            from_rotation = nullptr;
        }

        if (to_operation) {
          to_rotation =
              static_cast<const RotateTransformOperation*>(to_operation.get());
          if (to_rotation->Axis().IsZero())
            to_rotation = nullptr;
        }

        double from_angle;
        double to_angle;
        FloatPoint3D axis;
        if (!RotateTransformOperation::GetCommonAxis(
                from_rotation, to_rotation, axis, from_angle, to_angle)) {
          return false;
        }

        if (!from_rotation) {
          identity_rotation = RotateTransformOperation::Create(
              axis.X(), axis.Y(), axis.Z(), 0,
              from_operation ? from_operation->GetType()
                             : to_operation->GetType());
          from_rotation = identity_rotation.get();
        }

        if (!to_rotation) {
          if (!identity_rotation)
            identity_rotation = RotateTransformOperation::Create(
                axis.X(), axis.Y(), axis.Z(), 0,
                from_operation ? from_operation->GetType()
                               : to_operation->GetType());
          to_rotation = identity_rotation.get();
        }

        FloatBox from_box = *bounds;
        bool first = true;
        for (size_t i = 0; i < 2; ++i) {
          for (size_t j = 0; j < 2; ++j) {
            for (size_t k = 0; k < 2; ++k) {
              FloatBox bounds_for_arc;
              FloatPoint3D corner(from_box.X(), from_box.Y(), from_box.Z());
              corner +=
                  FloatPoint3D(i * from_box.Width(), j * from_box.Height(),
                               k * from_box.Depth());
              BoundingBoxForArc(corner, *from_rotation, *to_rotation,
                                min_progress, max_progress, bounds_for_arc);
              if (first) {
                *bounds = bounds_for_arc;
                first = false;
              } else {
                bounds->ExpandTo(bounds_for_arc);
              }
            }
          }
        }
      }
        continue;
      case TransformOperation::kMatrix:
      case TransformOperation::kMatrix3D:
      case TransformOperation::kInterpolated:
      case TransformOperation::kRotateAroundOrigin:
        return false;
    }
  }

  return true;
}

TransformOperations TransformOperations::Add(
    const TransformOperations& addend) const {
  TransformOperations result;
  result.operations_ = Operations();
  result.operations_.AppendVector(addend.Operations());
  return result;
}

TransformOperations TransformOperations::Zoom(double factor) const {
  TransformOperations result;
  for (auto& transform_operation : operations_)
    result.operations_.push_back(transform_operation->Zoom(factor));
  return result;
}

}  // namespace blink
