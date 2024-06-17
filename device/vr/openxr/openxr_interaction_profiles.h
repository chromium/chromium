// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_
#define DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_

#include "base/containers/flat_map.h"
#include "base/stl_util.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// A special system name used for hand tracking profiles to help differentiate
// between the set of profiles to use when hand joint data is exposed (this one)
// or the hand joint data is not exposed (the default one).
inline constexpr char kOpenXrHandJointSystem[] = "hand-joints";

enum class OpenXrHandednessType {
  kLeft = 0,
  kRight = 1,
  kCount,
};

enum class OpenXrButtonType {
  kTrigger = 0,
  kSqueeze = 1,
  kTrackpad = 2,
  kThumbstick = 3,
  kThumbrest = 4,
  kButton1 = 5,
  kButton2 = 6,
  kGrasp = 7,
  kShoulder = 8,
  kMenu = 9,
  kMaxValue = 9,
};

enum class OpenXrAxisType {
  kTrackpad = 0,
  kThumbstick = 1,
  kMaxValue = 1,
};

enum class OpenXrButtonActionType {
  kPress = 0,
  kTouch = 1,
  kValue = 2,
  kCount = 3,
};

struct OpenXrButtonActionPathMap {
  OpenXrButtonActionType type;
  std::string path;
};

struct OpenXrButtonPathMap {
  OpenXrButtonType type;
  std::vector<OpenXrButtonActionPathMap> action_maps;
  OpenXrButtonPathMap(OpenXrButtonType type,
                      std::vector<OpenXrButtonActionPathMap> action_maps);
  ~OpenXrButtonPathMap();
  OpenXrButtonPathMap(const OpenXrButtonPathMap& other);
  OpenXrButtonPathMap& operator=(const OpenXrButtonPathMap& other);
};

struct OpenXrAxisPathMap {
  OpenXrAxisType type;
  std::string path;
};

struct OpenXrSystemInputProfiles {
  // The system_name is matched against the OpenXR XrSystemProperties systemName
  // so that different hardware revisions can return a more exact input profile.
  // A nullptr system_name indicates that this set of input profiles matches any
  // system that doesn't have an explicit match. Each interaction profile should
  // have one OpenXrSystemInputProfiles with a system_name of nullptr.
  std::string system_name;
  std::vector<std::string> input_profiles;

  OpenXrSystemInputProfiles(std::string system_name,
                            std::vector<std::string> input_profiles);
  ~OpenXrSystemInputProfiles();
  OpenXrSystemInputProfiles(const OpenXrSystemInputProfiles& other);
  OpenXrSystemInputProfiles& operator=(const OpenXrSystemInputProfiles& other);
};

struct OpenXrControllerInteractionProfile {
  mojom::OpenXrInteractionProfileType type;
  std::string path;
  std::string required_extension;
  std::vector<OpenXrButtonPathMap> common_button_maps;
  std::vector<OpenXrButtonPathMap> left_button_maps;
  std::vector<OpenXrButtonPathMap> right_button_maps;
  std::vector<OpenXrAxisPathMap> axis_maps;

  OpenXrControllerInteractionProfile(
      mojom::OpenXrInteractionProfileType type,
      std::string path,
      std::string required_extension,
      std::vector<OpenXrButtonPathMap> common_button_maps,
      std::vector<OpenXrButtonPathMap> left_button_maps,
      std::vector<OpenXrButtonPathMap> right_button_maps,
      std::vector<OpenXrAxisPathMap> axis_maps);
  ~OpenXrControllerInteractionProfile();
  OpenXrControllerInteractionProfile(
      const OpenXrControllerInteractionProfile& other);
  OpenXrControllerInteractionProfile& operator=(
      const OpenXrControllerInteractionProfile& other);
};

// Currently Supports:
// Microsoft motion controller.
// Samsung Odyssey controller
// Khronos simple controller.
// Oculus touch controller.
// Valve index controller.
// HTC vive controller
// HP Reverb G2 controller
// MSFT Hand Interaction
// Declare OpenXR input profile bindings for other runtimes when they become
// available.
const std::vector<OpenXrControllerInteractionProfile>&
GetOpenXrControllerInteractionProfiles();
const base::flat_map<device::mojom::OpenXrInteractionProfileType,
                     std::vector<OpenXrSystemInputProfiles>>&
GetOpenXrInputProfilesMap();
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_
