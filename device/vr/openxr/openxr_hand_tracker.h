// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_H_
#define DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_H_

#include "base/memory/raw_ref.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

enum class OpenXrHandednessType;
class OpenXrExtensionHelper;

class OpenXrHandTracker {
 public:
  OpenXrHandTracker(const OpenXrExtensionHelper& extension_helper,
                    XrSession session,
                    OpenXrHandednessType type);
  virtual ~OpenXrHandTracker();

  XrResult Update(XrSpace base_space, XrTime predicted_display_time);

  mojom::XRHandTrackingDataPtr GetHandTrackingData() const;

 protected:
  bool IsDataValid() const;

 private:
  XrResult InitializeHandTracking();

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  OpenXrHandednessType type_;
  XrHandTrackerEXT hand_tracker_{XR_NULL_HANDLE};

  XrHandJointLocationEXT joint_locations_buffer_[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointLocationsEXT locations_{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_H_
