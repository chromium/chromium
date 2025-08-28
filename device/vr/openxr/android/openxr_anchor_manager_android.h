// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_space_based_anchor_manager.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

class OpenXrAnchorManagerAndroid : public OpenXrSpaceBasedAnchorManager {
 public:
  OpenXrAnchorManagerAndroid(const OpenXrExtensionHelper& extension_helper,
                             XrSession session,
                             XrSpace mojo_space);

  ~OpenXrAnchorManagerAndroid() override;

 private:
  XrSpace CreateAnchorInternal(XrPosef pose,
                               XrSpace space,
                               XrTime predicted_display_time) override;
  void OnDetachAnchor(const XrSpace& anchor_data) override;
  base::expected<device::Pose, OpenXrAnchorManager::AnchorTrackingErrorType>
  GetAnchorFromMojom(XrSpace anchor_space,
                     XrTime predicted_display_time) const override;

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_ANCHOR_MANAGER_ANDROID_H_
