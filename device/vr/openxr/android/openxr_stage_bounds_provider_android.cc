// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/android/openxr_stage_bounds_provider_android.h"

#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
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

OpenXrStageBoundsProviderAndroidFactory::
    OpenXrStageBoundsProviderAndroidFactory() = default;
OpenXrStageBoundsProviderAndroidFactory::
    ~OpenXrStageBoundsProviderAndroidFactory() = default;

const base::flat_set<std::string_view>&
OpenXrStageBoundsProviderAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_REFERENCE_SPACE_BOUNDS_POLYGON_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrStageBoundsProviderAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR};
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrStageBoundsProviderAndroidFactory::CreateStageBoundsProvider(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrStageBoundsProviderAndroid>(extension_helper,
                                                              session);
  }

  return nullptr;
}

}  // namespace device
