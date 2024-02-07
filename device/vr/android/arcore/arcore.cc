// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore.h"

#include "device/vr/android/arcore/arcore_math_utils.h"

namespace device {

gfx::Transform ArCore::GetCameraUvFromScreenUvTransform() const {
  //
  // Observe how kInputCoordinatesForTransform are transformed by ArCore,
  // compute a matrix based on that and post-multiply with a matrix that
  // performs a Y-flip.
  //
  // We need to add a Y flip because ArCore's
  // AR_COORDINATES_2D_TEXTURE_NORMALIZED coordinates have the origin at the top
  // left to match 2D Android APIs, so it needs a Y flip to get an origin at
  // bottom left as used for textures.
  // The post-multiplied matrix is performing a mapping: (x, y) -> (x, 1 - y).
  //
  gfx::Transform transform = GetDepthUvFromScreenUvTransform();
  transform.Translate(0, 1);
  transform.Scale(1, -1);
  return transform;
}

gfx::Transform ArCore::GetDepthUvFromScreenUvTransform() const {
  //
  // Observe how kInputCoordinatesForTransform are transformed by ArCore &
  // compute a matrix based on that. This is different than the camera UV
  // transform in that it does not perform the Y-flip - the depth buffer's
  // coordinate system is defined the same way as ArCore's
  // AR_COORDINATES_2D_TEXTURE_NORMALIZED.
  //
  return MatrixFromTransformedPoints(
      TransformDisplayUvCoords(kInputCoordinatesForTransform));
}

ArCore::InitializeResult::InitializeResult(
    const std::unordered_set<device::mojom::XRSessionFeature>& enabled_features,
    std::optional<device::mojom::XRDepthConfig> depth_configuration)
    : enabled_features(enabled_features),
      depth_configuration(std::move(depth_configuration)) {}

ArCore::InitializeResult::InitializeResult(const InitializeResult& other) =
    default;
ArCore::InitializeResult::~InitializeResult() = default;

ArCore::DepthSensingConfiguration::DepthSensingConfiguration(
    std::vector<device::mojom::XRDepthUsage> depth_usage_preference,
    std::vector<device::mojom::XRDepthDataFormat> depth_data_format_preference)
    : depth_usage_preference(depth_usage_preference),
      depth_data_format_preference(depth_data_format_preference) {}

ArCore::DepthSensingConfiguration::DepthSensingConfiguration(
    DepthSensingConfiguration&& other) = default;
ArCore::DepthSensingConfiguration::DepthSensingConfiguration(
    const DepthSensingConfiguration& other) = default;
ArCore::DepthSensingConfiguration::~DepthSensingConfiguration() = default;
ArCore::DepthSensingConfiguration& ArCore::DepthSensingConfiguration::operator=(
    const DepthSensingConfiguration& other) = default;
ArCore::DepthSensingConfiguration& ArCore::DepthSensingConfiguration::operator=(
    DepthSensingConfiguration&& other) = default;

}  // namespace device
