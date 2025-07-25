// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_plane_manager.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

class OpenXrPlaneManagerAndroid : public OpenXrPlaneManager {
 public:
  OpenXrPlaneManagerAndroid(const OpenXrExtensionHelper& extension_helper,
                            XrSession session);
  ~OpenXrPlaneManagerAndroid() override;

  void OnFrameUpdate(XrTime predicted_display_time,
                     XrSpace mojo_space) override;

  XrTrackableTrackerANDROID plane_tracker() const { return plane_tracker_; }
  XrTime predicted_display_time() const { return predicted_display_time_; }

 private:
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrTime predicted_display_time_ = 0;
  XrTrackableTrackerANDROID plane_tracker_ = XR_NULL_HANDLE;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_
