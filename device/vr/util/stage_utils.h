// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_UTIL_STAGE_UTILS_H_
#define DEVICE_VR_UTIL_STAGE_UTILS_H_

#include <vector>

#include "base/component_export.h"

namespace gfx {
class Point3F;
}  // namespace gfx

namespace device {
namespace vr_utils {

// convenience method for runtimes that only support rectangular play areas to
// convert to an array of bounds (which the mojom/WebXr API expects). Returns
// a vector of points representing the four corners of a rectangle centered at
// 0,0 with length and width determined by size_x and size_z respectively.
std::vector<gfx::Point3F> COMPONENT_EXPORT(DEVICE_VR_UTIL)
    GetStageBoundsFromSize(float size_x, float size_z);

}  // namespace vr_utils
}  // namespace device

#endif  // DEVICE_VR_UTIL_STAGE_UTILS_H_
