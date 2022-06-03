// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/stage_utils.h"

#include "ui/gfx/geometry/point3_f.h"

namespace device {
namespace vr_utils {

std::vector<gfx::Point3F> GetStageBoundsFromSize(float size_x, float size_z) {
  if (size_x <= 0.0 || size_z <= 0.0)
    return {};

  double hx = size_x * 0.5;
  double hz = size_z * 0.5;

  std::vector<gfx::Point3F> bounds = {
      gfx::Point3F(hx, 0.0, -hz), gfx::Point3F(hx, 0.0, hz),
      gfx::Point3F(-hx, 0.0, hz), gfx::Point3F(-hx, 0.0, -hz)};

  return bounds;
}

}  // namespace vr_utils
}  // namespace device
