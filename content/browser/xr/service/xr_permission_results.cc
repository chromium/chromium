// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_permission_results.h"

#include <optional>

#include "base/containers/contains.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace {

// Helper, creates a map from permission type to its permission status based
// on |permissions| and |permission_statuses|. Those 2 vectors must have equal
// length, and the permission status for permission at `permissions[i]` is
// assumed to be in `permission_statuses[i]`.
base::flat_map<blink::PermissionType, blink::mojom::PermissionStatus>
CreatePermissionTypeToStatusMap(
    const std::vector<blink::PermissionType>& permissions,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses) {
  DCHECK_EQ(permissions.size(), permission_statuses.size());

  base::flat_map<blink::PermissionType, blink::mojom::PermissionStatus> result;
  for (size_t i = 0; i < permissions.size(); ++i) {
    result[permissions[i]] = permission_statuses[i];
  }
  return result;
}

}  // namespace

namespace content {

XrPermissionResults::XrPermissionResults(
    const std::vector<blink::PermissionType>& permission_types,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses)
    : permission_type_to_status_(
          CreatePermissionTypeToStatusMap(permission_types,
                                          permission_statuses)) {}

XrPermissionResults::~XrPermissionResults() = default;

bool XrPermissionResults::HasPermissionsFor(
    device::mojom::XRSessionMode mode) const {
  auto mode_permission = GetPermissionFor(mode);
  if (!mode_permission) {
    return true;
  }

  return HasPermissionsFor(*mode_permission);
}

bool XrPermissionResults::HasPermissionsFor(
    device::mojom::XRSessionFeature feature) const {
  auto feature_permission = GetPermissionFor(feature);
  if (!feature_permission) {
    return true;
  }

  return HasPermissionsFor(*feature_permission);
}

bool XrPermissionResults::HasPermissionsFor(
    blink::PermissionType permission_type) const {
  if (!base::Contains(permission_type_to_status_, permission_type)) {
    return false;
  }

  return permission_type_to_status_.at(permission_type) ==
         blink::mojom::PermissionStatus::GRANTED;
}

// static
std::optional<blink::PermissionType> XrPermissionResults::GetPermissionFor(
    device::mojom::XRSessionMode mode) {
  switch (mode) {
    case device::mojom::XRSessionMode::kInline:
      return blink::PermissionType::SENSORS;
    case device::mojom::XRSessionMode::kImmersiveVr:
      return blink::PermissionType::VR;
    case device::mojom::XRSessionMode::kImmersiveAr:
      return blink::PermissionType::AR;
  }
}

// static
std::optional<blink::PermissionType> XrPermissionResults::GetPermissionFor(
    device::mojom::XRSessionFeature feature) {
  if (feature == device::mojom::XRSessionFeature::CAMERA_ACCESS) {
    return blink::PermissionType::VIDEO_CAPTURE;
  }
  if (feature == device::mojom::XRSessionFeature::HAND_INPUT) {
    return blink::PermissionType::HAND_TRACKING;
  }

  return std::nullopt;
}

}  // namespace content
