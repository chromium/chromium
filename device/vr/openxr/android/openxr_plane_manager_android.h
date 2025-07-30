// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_plane_manager.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// A simple manager for handling planes on Android. Note that due to the way
// the trackables system works, this is really just a thin wrapper around the
// plane_tracker.
class OpenXrPlaneManagerAndroid : public OpenXrPlaneManager {
 public:
  OpenXrPlaneManagerAndroid(const OpenXrExtensionHelper& extension_helper,
                            XrSession session);
  ~OpenXrPlaneManagerAndroid() override;

  XrTrackableTrackerANDROID plane_tracker() const { return plane_tracker_; }

 private:
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrTrackableTrackerANDROID plane_tracker_ = XR_NULL_HANDLE;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_PLANE_MANAGER_ANDROID_H_
