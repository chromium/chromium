// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_XR_PERMISSION_RESULTS_H_
#define CONTENT_BROWSER_XR_SERVICE_XR_PERMISSION_RESULTS_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

// Helper class used to check whether permissions that have been granted satisfy
// requirements for XR session creation given session mode and features that are
// to be enabled on it.
class XrPermissionResults {
 public:
  XrPermissionResults(
      const std::vector<blink::PermissionType>& permission_types,
      const std::vector<blink::mojom::PermissionStatus>& permission_statuses);
  ~XrPermissionResults();

  // Checks if |permission_type_to_status| contains permissions necessary to
  // create an XR session with |mode|. Returns `true` if so, `false` otherwise.
  bool HasPermissionsFor(device::mojom::XRSessionMode mode) const;

  // Checks if the XR session feature has all the required permissions to be
  // enabled. Returns `true` if |feature|'s permission requirements are
  // satisfied, `false` otherwise.
  bool HasPermissionsFor(device::mojom::XRSessionFeature feature) const;

  static std::optional<blink::PermissionType> GetPermissionFor(
      device::mojom::XRSessionMode mode);
  static std::optional<blink::PermissionType> GetPermissionFor(
      device::mojom::XRSessionFeature feature);

 private:
  const base::flat_map<blink::PermissionType, blink::mojom::PermissionStatus>
      permission_type_to_status_;

  bool HasPermissionsFor(blink::PermissionType permission_type) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_XR_PERMISSION_RESULTS_H_
