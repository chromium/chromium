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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/transforms/transform_operations.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/transforms/interpolated_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/box_f.h"

namespace blink {

namespace {
using ApplyCallback =
    base::RepeatingCallback<TransformOperation*(TransformOperation* from,
                                                TransformOperation* to)>;

// Applies a given function (|ApplyCallback|) to matching pairs of operations.
TransformOperations ApplyFunctionToMatchingPrefix(
    ApplyCallback apply_cb,
    const TransformOperations& from,
    const TransformOperations& to,
    wtf_size_t matching_prefix_length,
    bool* success) {
  TransformOperations result;
  wtf_size_t from_size = from.Operations().size();
  wtf_size_t to_size = to.Operations().size();

  // If the lists matched entirely but one was shorter, |matching_prefix_length|
  // will be the length of the longer list and we implicitly consider the
  // missing functions to be matching identity operations.
  DCHECK(matching_prefix_length <= std::max(from_size, to_size));

  for (wtf_size_t i = 0; i < matching_prefix_length; i++) {
    TransformOperation* from_operation =
        (i < from_size) ? from.Operations()[i].Get() : nullptr;
    TransformOperation* to_operation =
        (i < to_size) ? to.Operations()[i].Get() : nullptr;

    TransformOperation* result_operation =
        apply_cb.Run(from_operation, to_operation);

    if (result_operation) {
      result.Operations().push_back(result_operation);
    } else {
      *success = false;
      return result;
    }
  }
  return result;
}
}  // namespace

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

void TransformOperations::ApplyRemaining(const gfx::SizeF& border_box_size,
                                         wtf_size_t start,
                                         gfx::Transform& t) const {
  for (wtf_size_t i = start; i < operations_.size(); i++) {
    operations_[i]->Apply(t, border_box_size);
  }
}

TransformOperation::BoxSizeDependency TransformOperations::BoxSizeDependencies(
    wtf_size_t start) const {
  TransformOperation::BoxSizeDependency deps = TransformOperation::kDependsNone;
  for (wtf_size_t i = start; i < operations_.size(); i++) {
    deps = TransformOperation::CombineDependencies(
        deps, operations_[i]->BoxSizeDependencies());
  }
  return deps;
}

wtf_size_t TransformOperations::MatchingPrefixLength(
    const TransformOperations& other) const {
  wtf_size_t num_operations =
      std::min(Operations().size(), other.Operations().size());
  for (wtf_size_t i = 0; i < num_operations; ++i) {
    if (!Operations()[i]->CanBlendWith(*other.Operations()[i])) {
      // Remaining operations in each operations list require merging for
      // matrix/matrix3d interpolation.
      return i;
    }
  }
  // If the operations match to the length of the shorter list, then pad its
  // length with the matching identity operations.
  // https://drafts.csswg.org/css-transforms/#transform-function-lists
  return std::max(Operations().size(), other.Operations().size());
}

TransformOperation*
TransformOperations::BlendRemainingByUsingMatrixInterpolation(
    const TransformOperations& from,
    wtf_size_t matching_prefix_length,
    double progress,
    BoxSizeDependentMatrixBlending box_size_dependent) const {
  // Not safe to use a cached transform if any of the operations are size
  // dependent.
  if (BoxSizeDependencies(matching_prefix_length) ||
      from.BoxSizeDependencies(matching_prefix_length)) {
    if (box_size_dependent == BoxSizeDependentMatrixBlending::kDisallow) {
      return nullptr;
    }
    return MakeGarbageCollected<InterpolatedTransformOperation>(
        from, *this, matching_prefix_length, progress);
  }

  // Evaluate blended matrix here to avoid creating a nested data structure of
  // unbounded depth.
  gfx::Transform from_transform;
  gfx::Transform to_transform;
  from.ApplyRemaining(gfx::SizeF(), matching_prefix_length, from_transform);
  ApplyRemaining(gfx::SizeF(), matching_prefix_length, to_transform);

  // Fallback to discrete interpolation if either transform matrix is singular.
  if (!(from_transform.IsInvertible() && to_transform.IsInvertible())) {
    return nullptr;
  }

  if (!to_transform.Blend(from_transform, progress) && progress < 0.5)
    to_transform = from_transform;

  return MakeGarbageCollected<Matrix3DTransformOperation>(to_transform);
}

// https://drafts.csswg.org/css-transforms-1/#interpolation-of-transforms
// TODO(crbug.com/914397): Consolidate blink and cc implementations of transform
// interpolation.
TransformOperations TransformOperations::Blend(
    const TransformOperations& from,
    double progress,
    BoxSizeDependentMatrixBlending box_size_dependent) const {
  if (from == *this || (!from.size() && !size()))
    return *this;

  wtf_size_t matching_prefix_length = MatchingPrefixLength(from);
  wtf_size_t max_path_length =
      std::max(Operations().size(), from.Operations().size());

  bool success = true;
  TransformOperations result = ApplyFunctionToMatchingPrefix(
      WTF::BindRepeating(
          [](double progress, TransformOperation* from,
             TransformOperation* to) {
            // Where the lists matched but one was longer, the shorter list is
            // padded with nullptr that represent matching identity operations.
            return to ? to->Blend(from, progress)
                      : (from ? from->Blend(nullptr, progress, true) : nullptr);
          },
          progress),
      from, *this, matching_prefix_length, &success);
  if (success && matching_prefix_length < max_path_length) {
    TransformOperation* matrix_op = BlendRemainingByUsingMatrixInterpolation(
        from, matching_prefix_length, progress, box_size_dependent);
    if (matrix_op)
      result.Operations().push_back(matrix_op);
    else
      success = false;
  }
  if (!success) {
    return progress < 0.5 ? from : *this;
  }
  return result;
}

TransformOperations TransformOperations::Accumulate(
    const TransformOperations& to) const {
  if (!to.size() && !size())
    return *this;

  bool success = true;
  wtf_size_t matching_prefix_length = MatchingPrefixLength(to);
  wtf_size_t max_path_length =
      std::max(Operations().size(), to.Operations().size());

  // Accumulate matching pairs of transform functions.
  TransformOperations result = ApplyFunctionToMatchingPrefix(
      WTF::BindRepeating([](TransformOperation* from, TransformOperation* to) {
        if (to && from)
          return from->Accumulate(*to);
        // Where the lists matched but one was longer, the shorter list is
        // padded with nullptr that represent matching identity operations. For
        // any function, accumulate(f, identity) == f, so just return f.
        return to ? to : from;
      }),
      *this, to, matching_prefix_length, &success);

  // Then, if there are leftover non-matching functions, accumulate the
  // remaining matrices.
  if (success && matching_prefix_length < max_path_length) {
    gfx::Transform from_transform;
    gfx::Transform to_transform;
    ApplyRemaining(gfx::SizeF(), matching_prefix_length, from_transform);
    to.ApplyRemaining(gfx::SizeF(), matching_prefix_length, to_transform);

    TransformOperation* from_matrix =
        MakeGarbageCollected<Matrix3DTransformOperation>(from_transform);
    TransformOperation* to_matrix =
        MakeGarbageCollected<Matrix3DTransformOperation>(to_transform);
    TransformOperation* matrix_op = from_matrix->Accumulate(*to_matrix);

    if (matrix_op)
      result.Operations().push_back(matrix_op);
    else
      success = false;
  }

  // On failure, behavior is to replace.
  return success ? result : to;
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
static void BoundingBoxForArc(const gfx::Point3F& point,
                              const RotateTransformOperation& from_transform,
                              const RotateTransformOperation& to_transform,
                              double min_progress,
                              double max_progress,
                              gfx::BoxF& box) {
  double candidates[6];
  int num_candidates = 0;

  gfx::Vector3dF axis = from_transform.Axis();
  double from_degrees = from_transform.Angle();
  double to_degrees = to_transform.Angle();

  if (gfx::DotProduct(axis, to_transform.Axis()) < 0)
    to_degrees *= -1;

  from_degrees = Blend(from_degrees, to_degrees, min_progress);
  to_degrees = Blend(to_degrees, from_transform.Angle(), 1.0 - max_progress);
  if (from_degrees > to_degrees)
    std::swap(from_degrees, to_degrees);

  gfx::Transform from_matrix;
  gfx::Transform to_matrix;
  from_matrix.RotateAbout(from_transform.Axis(), from_degrees);
  to_matrix.RotateAbout(from_transform.Axis(), to_degrees);

  gfx::Point3F from_point = from_matrix.MapPoint(point);

  if (box.IsEmpty())
    box.set_origin(from_point);
  else
    box.ExpandTo(from_point);

  box.ExpandTo(to_matrix.MapPoint(point));

  switch (from_transform.GetType()) {
    case TransformOperation::kRotateX:
      FindCandidatesInPlane(point.y(), point.z(), from_transform.X(),
                            candidates, &num_candidates);
      break;
    case TransformOperation::kRotateY:
      FindCandidatesInPlane(point.z(), point.x(), from_transform.Y(),
                            candidates, &num_candidates);
      break;
    case TransformOperation::kRotateZ:
    case TransformOperation::kRotate:
      FindCandidatesInPlane(point.x(), point.y(), from_transform.Z(),
                            candidates, &num_candidates);
      break;
    default: {
      gfx::Vector3dF normal;
      if (!axis.GetNormalized(&normal))
        return;
      gfx::Vector3dF to_point = point.OffsetFromOrigin();
      gfx::Point3F center = gfx::PointAtOffsetFromOrigin(
          gfx::ScaleVector3d(normal, gfx::DotProduct(to_point, normal)));
      gfx::Vector3dF v1 = point - center;
      if (!v1.GetNormalized(&v1))
        return;

      gfx::Vector3dF v2 = gfx::CrossProduct(normal, v1);
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

      candidates[0] = atan2(v2.x(), v1.x());
      candidates[1] = candidates[0] + M_PI;
      candidates[2] = atan2(v2.y(), v1.y());
      candidates[3] = candidates[2] + M_PI;
      candidates[4] = atan2(v2.z(), v1.z());
      candidates[5] = candidates[4] + M_PI;
      num_candidates = 6;
    } break;
  }

  double min_radians = Deg2rad(from_degrees);
  double max_radians = Deg2rad(to_degrees);
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

    gfx::Transform rotation;
    rotation.RotateAbout(axis, Rad2deg(radians));
    box.ExpandTo(rotation.MapPoint(point));
  }
}

bool TransformOperations::BlendedBoundsForBox(const gfx::BoxF& box,
                                              const TransformOperations& from,
                                              const double& min_progress,
                                              const double& max_progress,
                                              gfx::BoxF* bounds) const {
  int from_size = from.Operations().size();
  int to_size = Operations().size();
  int size = std::max(from_size, to_size);

  *bounds = box;
  for (int i = size - 1; i >= 0; i--) {
    TransformOperation* from_operation =
        (i < from_size) ? from.Operations()[i] : nullptr;
    TransformOperation* to_operation =
        (i < to_size) ? Operations()[i] : nullptr;

    DCHECK(from_operation || to_operation);
    TransformOperation::OperationType interpolation_type =
        to_operation ? to_operation->GetType() : from_operation->GetType();
    if (from_operation && to_operation &&
        !from_operation->CanBlendWith(*to_operation)) {
      return false;
    }

    switch (interpolation_type) {
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
        TransformOperation* from_transform = nullptr;
        TransformOperation* to_transform = nullptr;
        if (!to_operation) {
          from_transform =
              from_operation->Blend(to_operation, 1 - min_progress, false);
          to_transform =
              from_operation->Blend(to_operation, 1 - max_progress, false);
        } else {
          from_transform =
              to_operation->Blend(from_operation, min_progress, false);
          to_transform =
              to_operation->Blend(from_operation, max_progress, false);
        }
        if (!from_transform || !to_transform)
          continue;
        gfx::Transform from_matrix;
        gfx::Transform to_matrix;
        from_transform->Apply(from_matrix, gfx::SizeF());
        to_transform->Apply(to_matrix, gfx::SizeF());
        gfx::BoxF from_box = from_matrix.MapBox(*bounds);
        gfx::BoxF to_box = to_matrix.MapBox(*bounds);
        *bounds = from_box;
        bounds->ExpandTo(to_box);
        continue;
      }
      case TransformOperation::kRotate:
      case TransformOperation::kRotate3D:
      case TransformOperation::kRotateX:
      case TransformOperation::kRotateY:
      case TransformOperation::kRotateZ: {
        RotateTransformOperation* identity_rotation = nullptr;
        const RotateTransformOperation* from_rotation = nullptr;
        const RotateTransformOperation* to_rotation = nullptr;
        if (from_operation) {
          from_rotation =
              static_cast<const RotateTransformOperation*>(from_operation);
          if (from_rotation->Axis().IsZero())
            from_rotation = nullptr;
        }

        if (to_operation) {
          to_rotation =
              static_cast<const RotateTransformOperation*>(to_operation);
          if (to_rotation->Axis().IsZero())
            to_rotation = nullptr;
        }

        double from_angle;
        double to_angle;
        gfx::Vector3dF axis;
        if (!RotateTransformOperation::GetCommonAxis(
                from_rotation, to_rotation, axis, from_angle, to_angle)) {
          return false;
        }

        if (!from_rotation) {
          identity_rotation = MakeGarbageCollected<RotateTransformOperation>(
              axis.x(), axis.y(), axis.z(), 0,
              from_operation ? from_operation->GetType()
                             : to_operation->GetType());
          from_rotation = identity_rotation;
        }

        if (!to_rotation) {
          if (!identity_rotation)
            identity_rotation = MakeGarbageCollected<RotateTransformOperation>(
                axis.x(), axis.y(), axis.z(), 0,
                from_operation ? from_operation->GetType()
                               : to_operation->GetType());
          to_rotation = identity_rotation;
        }

        gfx::BoxF from_box = *bounds;
        bool first = true;
        for (size_t j = 0; j < 2; ++j) {
          for (size_t k = 0; k < 2; ++k) {
            for (size_t m = 0; m < 2; ++m) {
              gfx::BoxF bounds_for_arc;
              gfx::Point3F corner(from_box.x(), from_box.y(), from_box.z());
              corner +=
                  gfx::Vector3dF(j * from_box.width(), k * from_box.height(),
                                 m * from_box.depth());
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
