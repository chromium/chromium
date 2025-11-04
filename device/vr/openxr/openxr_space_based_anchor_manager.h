// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPACE_BASED_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPACE_BASED_ANCHOR_MANAGER_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrSpaceBasedAnchorManager : public OpenXrAnchorManager {
 public:
  OpenXrSpaceBasedAnchorManager();
  ~OpenXrSpaceBasedAnchorManager() override;

  // OpenXrAnchorManager
  AnchorId CreateAnchor(XrPosef pose,
                        XrSpace space,
                        XrTime predicted_display_time,
                        std::optional<PlaneId> plane_id) override;
  void DetachAnchor(AnchorId anchor_id) override;
  std::optional<XrLocation> GetXrLocationFromAnchor(
      AnchorId anchor_id,
      const gfx::Transform& anchor_id_from_new_anchor) const override;
  mojom::XRAnchorsDataPtr GetCurrentAnchorsData(
      XrTime predicted_display_time) override;

 protected:

  virtual XrSpace CreateAnchorInternal(XrPosef pose,
                                       XrSpace space,
                                       XrTime predicted_display_time) = 0;
  virtual void OnDetachAnchor(const XrSpace& anchor_space) = 0;
  virtual base::expected<device::Pose, AnchorTrackingErrorType>
  GetAnchorFromMojom(XrSpace anchor_space,
                     XrTime predicted_display_time) const = 0;

 private:
  XrSpace GetAnchorSpace(AnchorId anchor_id) const;

  AnchorId::Generator anchor_id_generator_;
  absl::flat_hash_map<AnchorId, XrSpace> openxr_anchors_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPACE_BASED_ANCHOR_MANAGER_H_
