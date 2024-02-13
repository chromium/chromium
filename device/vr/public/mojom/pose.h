// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_POSE_H_
#define DEVICE_VR_PUBLIC_MOJOM_POSE_H_

#include <optional>

#include "base/component_export.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

// Pose represents some entity's position and orientation and is always
// expressed relative to some coordinate system. Alternatively, the pose can be
// viewed as a rigid transform that performs a coordinate system change from the
// coordinate system represented by the entity (i.e. coordinate system whose
// origin is equal to entity's position and whose orientation is equal to
// entity's orientation) to the coordinate system relative to which the pose is
// supposed to be expressed.
//
// If the pose represents a position of entity |entity| in coordinate space
// |coord|, it is recommended to name such a variable as |coord_from_entity|.
// In order to obtain the matrix that performs the coordinate system
// transformation, the callers can use |GetOtherFromThis()| method. The
// resulting matrix will encode coordinate system change from |entity| space to
// |coord| space.
//
// The source for the naming convention can be found here:
// https://www.sebastiansylvan.com/post/matrix_naming_convention/
class COMPONENT_EXPORT(VR_PUBLIC_TYPEMAPS) Pose {
 public:
  explicit Pose();
  explicit Pose(const gfx::Point3F& position,
                const gfx::Quaternion& orientation);

  // Creates a pose from transform by decomposing it. The method assumes that
  // the passed in matrix represents a rigid transformation (i.e. only the
  // orientation and translation components of the decomposed matrix will affect
  // the result). If the matrix could not be decomposed, the method will return
  // a std::nullopt.
  static std::optional<Pose> Create(const gfx::Transform& other_from_this);

  const gfx::Point3F& position() const { return position_; }

  const gfx::Quaternion& orientation() const { return orientation_; }

  // Returns the underlying matrix representation of the pose.
  const gfx::Transform& ToTransform() const;

 private:
  gfx::Point3F position_;
  gfx::Quaternion orientation_;

  // Transformation that can be used to switch from coordinate system
  // represented by this pose to other coordinate system (i.e. the coordinate
  // system relative to which this pose is supposed to be expressed).
  gfx::Transform other_from_this_;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_POSE_H_
