// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_

#include <array>
#include <memory>
#include <vector>

#include "base/optional.h"

#include "device/vr/openxr/openxr_controller.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"

namespace device {

class OpenXRInputHelper {
 public:
  static XrResult CreateOpenXRInputHelper(
      XrInstance instance,
      XrSession session,
      XrSpace local_space,
      std::unique_ptr<OpenXRInputHelper>* helper);

  OpenXRInputHelper(XrSession session, XrSpace local_space);

  ~OpenXRInputHelper();

  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      XrTime predicted_display_time);

  void OnInteractionProfileChanged(XrResult* xr_result);

  base::WeakPtr<OpenXRInputHelper> GetWeakPtr();

 private:
  base::Optional<Gamepad> GetWebXRGamepad(const OpenXrController& controller);

  XrResult Initialize(XrInstance instance);

  XrResult SyncActions(XrTime predicted_display_time);

  XrSession session_;
  XrSpace local_space_;

  struct OpenXrControllerState {
    OpenXrController controller;
    bool primary_button_pressed;
  };
  std::array<OpenXrControllerState,
             static_cast<size_t>(OpenXrHandednessType::kCount)>
      controller_states_;

  std::unique_ptr<OpenXRPathHelper> path_helper_;

  // This must be the last member
  base::WeakPtrFactory<OpenXRInputHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OpenXRInputHelper);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_
