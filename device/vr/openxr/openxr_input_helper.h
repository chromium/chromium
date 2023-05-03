// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_

#include <array>
#include <memory>
#include <vector>

#include "device/vr/openxr/openxr_controller.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

class OpenXRInputHelper {
 public:
  static XrResult CreateOpenXRInputHelper(
      XrInstance instance,
      XrSystemId system,
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace local_space,
      std::unique_ptr<OpenXRInputHelper>* helper);

  OpenXRInputHelper(XrSession session, XrSpace local_space);

  OpenXRInputHelper(const OpenXRInputHelper&) = delete;
  OpenXRInputHelper& operator=(const OpenXRInputHelper&) = delete;

  ~OpenXRInputHelper();

  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      bool hand_input_enabled,
      XrTime predicted_display_time);

  XrResult OnInteractionProfileChanged();

 private:
  absl::optional<Gamepad> GetWebXRGamepad(const OpenXrController& controller);

  XrResult Initialize(XrInstance instance,
                      XrSystemId system,
                      const OpenXrExtensionHelper& extension_helper);

  XrResult SyncActions(XrTime predicted_display_time);

  XrSpace GetMojomSpace() const {
    return local_space_;  // Mojom space is currently defined as local space
  }

  XrSession session_;
  XrSpace local_space_;

  struct OpenXrControllerState {
    OpenXrController controller;
    bool primary_button_pressed;
    bool squeeze_button_pressed;
  };
  std::array<OpenXrControllerState,
             static_cast<size_t>(OpenXrHandednessType::kCount)>
      controller_states_;

  std::unique_ptr<OpenXRPathHelper> path_helper_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INPUT_HELPER_H_
