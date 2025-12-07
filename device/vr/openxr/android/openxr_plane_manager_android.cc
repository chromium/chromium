// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_plane_manager_android.h"

#include <vector>

#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace {}  // namespace

namespace device {

OpenXrPlaneManagerAndroid::OpenXrPlaneManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session)
    : extension_helper_(extension_helper), session_(session) {
  XrTrackableTrackerCreateInfoANDROID create_info{
      XR_TYPE_TRACKABLE_TRACKER_CREATE_INFO_ANDROID};
  create_info.trackableType = XR_TRACKABLE_TYPE_PLANE_ANDROID;

  extension_helper_->ExtensionMethods().xrCreateTrackableTrackerANDROID(
      session_, &create_info, &plane_tracker_);
}

OpenXrPlaneManagerAndroid::~OpenXrPlaneManagerAndroid() {
  if (plane_tracker_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroyTrackableTrackerANDROID(
        plane_tracker_);
  }
}

}  // namespace device
