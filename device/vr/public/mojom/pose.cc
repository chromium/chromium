// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/public/mojom/pose.h"

#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

Pose::Pose() = default;

Pose::Pose(const gfx::Point3F& position, const gfx::Quaternion& orientation)
    : position_(position), orientation_(orientation) {
  gfx::DecomposedTransform decomposed_pose;
  decomposed_pose.translate[0] = position.x();
  decomposed_pose.translate[1] = position.y();
  decomposed_pose.translate[2] = position.z();
  decomposed_pose.quaternion = orientation;

  other_from_this_ = gfx::Transform::Compose(decomposed_pose);
}

std::optional<Pose> Pose::Create(const gfx::Transform& other_from_this) {
  std::optional<gfx::DecomposedTransform> decomposed_other_from_this =
      other_from_this.Decompose();
  if (!decomposed_other_from_this)
    return std::nullopt;

  return Pose(gfx::Point3F(decomposed_other_from_this->translate[0],
                           decomposed_other_from_this->translate[1],
                           decomposed_other_from_this->translate[2]),
              decomposed_other_from_this->quaternion);
}

const gfx::Transform& Pose::ToTransform() const {
  return other_from_this_;
}

}  // namespace device
