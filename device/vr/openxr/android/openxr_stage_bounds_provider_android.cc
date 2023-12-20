// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/android/openxr_stage_bounds_provider_android.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/dev/xr_android.h"
#include "ui/gfx/geometry/point3_f.h"

namespace device {

OpenXrStageBoundsProviderAndroid::OpenXrStageBoundsProviderAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session)
    : extension_helper_(extension_helper), session_(session) {}

OpenXrStageBoundsProviderAndroid::~OpenXrStageBoundsProviderAndroid() = default;

std::vector<gfx::Point3F> OpenXrStageBoundsProviderAndroid::GetStageBounds() {
  uint32_t vertices_count_output = 0;
  XrResult result = extension_helper_->ExtensionMethods()
                        .xrGetReferenceSpaceBoundsPolygonANDROID(
                            session_, XR_REFERENCE_SPACE_TYPE_STAGE, 0,
                            &vertices_count_output, nullptr);

  if (!XR_SUCCEEDED(result) || vertices_count_output == 0) {
    return {};
  }

  std::vector<XrVector2f> boundary_vertices(vertices_count_output);
  result =
      extension_helper_->ExtensionMethods()
          .xrGetReferenceSpaceBoundsPolygonANDROID(
              session_, XR_REFERENCE_SPACE_TYPE_STAGE, boundary_vertices.size(),
              &vertices_count_output, boundary_vertices.data());

  // In the (unlikely) event that the array is differently sized, there should
  // be a pending XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING which will
  // cause us to update the bounds. They'll be stale for at most a frame.
  if (!XR_SUCCEEDED(result) ||
      boundary_vertices.size() != vertices_count_output) {
    return {};
  }

  // `xrGetReferenceSpaceBoundsPolygonANDROID` returns the bounds in counter-
  // clockwise order, but we need them in clock-wise order, so iterate through
  // the vector backwards.
  std::vector<gfx::Point3F> stage_bounds;
  stage_bounds.reserve(vertices_count_output);
  base::ranges::transform(boundary_vertices.rbegin(), boundary_vertices.rend(),
                          std::back_inserter(stage_bounds),
                          [](const XrVector2f& xr_coord) {
                            return gfx::Point3F(xr_coord.x, 0.0, xr_coord.y);
                          });

  return stage_bounds;
}

}  // namespace device
