// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_

#include <map>

#include "base/numerics/checked_math.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

class OpenXrAnchorManager {
 public:
  OpenXrAnchorManager(const OpenXrExtensionHelper& extension_helper,
                      XrSession session,
                      XrSpace mojo_space);
  ~OpenXrAnchorManager();

  AnchorId CreateAnchor(XrPosef pose,
                        XrSpace space,
                        XrTime predicted_display_time);
  XrSpace GetAnchorSpace(AnchorId anchor_id) const;
  void DetachAnchor(AnchorId anchor_id);
  device::mojom::XRAnchorsDataPtr GetCurrentAnchorsData(
      XrTime predicted_display_time) const;

 private:
  const OpenXrExtensionHelper& extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;  // The intermediate space that mojom poses are
                        // represented in (currently defined as local space)

  // Each OpenXR anchor produces a space handle which tracks the location of the
  // anchor. We create and cache this space here in order to avoid complex
  // resource tracking.
  struct AnchorData {
    XrSpatialAnchorMSFT anchor;
    XrSpace
        space;  // The XrSpace tracking this anchor relative to other XrSpaces
  };

  void DestroyAnchorData(const AnchorData& anchor_data);

  AnchorId::Generator anchor_id_generator_;  // 0 is not a valid anchor ID
  std::map<AnchorId, AnchorData> openxr_anchors_;
  DISALLOW_COPY_AND_ASSIGN(OpenXrAnchorManager);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_ANCHOR_MANAGER_H_
