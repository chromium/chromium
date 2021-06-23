// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_
#define DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_

#include "base/stl_util.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_defs.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

constexpr size_t kMaxNumActionMaps = 3;
constexpr size_t kMaxInputProfiles = 5;

enum class OpenXrHandednessType {
  kLeft = 0,
  kRight = 1,
  kCount = 2,
};

enum class OpenXrInteractionProfileType {
  kMicrosoftMotion = 0,
  kKHRSimple = 1,
  kOculusTouch = 2,
  kValveIndex = 3,
  kHTCVive = 4,
  kSamsungOdyssey = 5,
  kHPReverbG2 = 6,
  kHandSelectGrasp = 7,
  kCount = 8,
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
  kMaxValue = 7,
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
  const char* const path;
};

struct OpenXrButtonPathMap {
  OpenXrButtonType type;
  OpenXrButtonActionPathMap action_maps[kMaxNumActionMaps];
  size_t action_map_size;
};

struct OpenXrAxisPathMap {
  OpenXrAxisType type;
  const char* const path;
};

struct OpenXrSystemInputProfiles {
  // The system_name is matched against the OpenXR XrSystemProperties systemName
  // so that different hardware revisions can return a more exact input profile.
  // A nullptr system_name indicates that this set of input profiles matches any
  // system that doesn't have an explicit match. Each interaction profile should
  // have one OpenXrSystemInputProfiles with a system_name of nullptr.
  const char* const system_name;
  const char* const input_profiles[kMaxInputProfiles];
  size_t profile_size;
};

struct OpenXrControllerInteractionProfile {
  OpenXrInteractionProfileType type;
  const char* const path;
  const char* const required_extension;
  GamepadMapping mapping;
  const OpenXrSystemInputProfiles* const system_input_profiles;
  const size_t input_profile_size;
  const OpenXrButtonPathMap* left_button_maps;
  size_t left_button_map_size;
  const OpenXrButtonPathMap* right_button_maps;
  size_t right_button_map_size;
  const OpenXrAxisPathMap* axis_maps;
  size_t axis_map_size;
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
constexpr OpenXrSystemInputProfiles kMicrosoftMotionInputProfiles[] = {
    {nullptr,
     {"windows-mixed-reality", "generic-trigger-squeeze-touchpad-thumbstick"},
     2}};

constexpr OpenXrSystemInputProfiles kGenericButtonInputProfiles[] = {
    {nullptr, {"generic-button"}, 1}};

constexpr OpenXrSystemInputProfiles kOculusTouchInputProfiles[] = {
    {nullptr, {"oculus-touch", "generic-trigger-squeeze-thumbstick"}, 2},
    {"Oculus Rift S",
     {"oculus-touch-v2", "oculus-touch", "generic-trigger-squeeze-thumbstick"},
     3},
    {"Quest",
     {"oculus-touch-v2", "oculus-touch", "generic-trigger-squeeze-thumbstick"},
     3},
    // Name currently reported by OpenXR for the Quest 2
    {"Miramar",
     {"oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
      "generic-trigger-squeeze-thumbstick"},
     4},
    // Oculus says this will soon be the name OpenXR reports
    {"Oculus Quest2",
     {"oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
      "generic-trigger-squeeze-thumbstick"},
     4}};

constexpr OpenXrSystemInputProfiles kValveIndexInputProfiles[] = {
    {nullptr,
     {"valve-index", "generic-trigger-squeeze-touchpad-thumbstick"},
     2}};

constexpr OpenXrSystemInputProfiles kHTCViveInputProfiles[] = {
    {nullptr, {"htc-vive", "generic-trigger-squeeze-touchpad"}, 2}};

constexpr OpenXrSystemInputProfiles kSamsungOdysseyInputProfiles[] = {
    {nullptr,
     {"samsung-odyssey", "windows-mixed-reality",
      "generic-trigger-squeeze-touchpad-thumbstick"},
     3}};

constexpr OpenXrSystemInputProfiles kHPReverbG2InputProfiles[] = {
    {nullptr,
     {"hp-mixed-reality", "oculus-touch", "generic-trigger-squeeze"},
     3}};

constexpr OpenXrSystemInputProfiles kGenericHandSelectGraspInputProfile[] = {
    {nullptr, {"generic-hand-select-grasp", "generic-hand-select"}, 2}};

constexpr OpenXrButtonPathMap kMicrosoftMotionControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {
         {OpenXrButtonActionType::kPress, "/input/trigger/value"},
         {OpenXrButtonActionType::kValue, "/input/trigger/value"},
     },
     2},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/click"}},
     1},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"}},
     1},
    {OpenXrButtonType::kTrackpad,
     {{OpenXrButtonActionType::kPress, "/input/trackpad/click"},
      {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"}},
     2}};

constexpr OpenXrButtonPathMap kKronosSimpleControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/select/click"}},
     1},
};

constexpr OpenXrButtonPathMap kOculusTouchLeftControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/trigger/value"},
      {OpenXrButtonActionType::kValue, "/input/trigger/value"},
      {OpenXrButtonActionType::kTouch, "/input/trigger/touch"}},
     3},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/value"}},
     2},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
      {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"}},
     2},
    {OpenXrButtonType::kThumbrest,
     {{OpenXrButtonActionType::kTouch, "/input/thumbrest/touch"}},
     1},
    {OpenXrButtonType::kButton1,
     {{OpenXrButtonActionType::kPress, "/input/x/click"},
      {OpenXrButtonActionType::kTouch, "/input/x/touch"}},
     2},
    {OpenXrButtonType::kButton2,
     {{OpenXrButtonActionType::kPress, "/input/y/click"},
      {OpenXrButtonActionType::kTouch, "/input/y/touch"}},
     2},
};

constexpr OpenXrButtonPathMap kOculusTouchRightControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/trigger/value"},
      {OpenXrButtonActionType::kValue, "/input/trigger/value"},
      {OpenXrButtonActionType::kTouch, "/input/trigger/touch"}},
     3},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/value"}},
     2},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
      {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"}},
     2},
    {OpenXrButtonType::kThumbrest,
     {{OpenXrButtonActionType::kTouch, "/input/thumbrest/touch"}},
     1},
    {OpenXrButtonType::kButton1,
     {{OpenXrButtonActionType::kPress, "/input/a/click"},
      {OpenXrButtonActionType::kTouch, "/input/a/touch"}},
     2},
    {OpenXrButtonType::kButton2,
     {{OpenXrButtonActionType::kPress, "/input/b/click"},
      {OpenXrButtonActionType::kTouch, "/input/b/touch"}},
     2},
};

constexpr OpenXrButtonPathMap kValveIndexControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/trigger/click"},
      {OpenXrButtonActionType::kValue, "/input/trigger/value"},
      {OpenXrButtonActionType::kTouch, "/input/trigger/touch"}},
     3},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/force"}},
     2},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
      {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"}},
     2},
    {OpenXrButtonType::kTrackpad,
     {{OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
      {OpenXrButtonActionType::kValue, "/input/trackpad/force"}},
     2},
    {OpenXrButtonType::kButton1,
     {{OpenXrButtonActionType::kPress, "/input/a/click"},
      {OpenXrButtonActionType::kTouch, "/input/a/touch"}},
     2},
};

constexpr OpenXrButtonPathMap kHTCViveControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {
         {OpenXrButtonActionType::kPress, "/input/trigger/click"},
         {OpenXrButtonActionType::kValue, "/input/trigger/value"},
     },
     2},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/click"}},
     1},
    {OpenXrButtonType::kTrackpad,
     {{OpenXrButtonActionType::kPress, "/input/trackpad/click"},
      {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"}},
     2}};

constexpr OpenXrButtonPathMap kHPReverbG2LeftControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/trigger/value"},
      {OpenXrButtonActionType::kValue, "/input/trigger/value"}},
     2},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/value"}},
     2},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"}},
     1},
    {OpenXrButtonType::kButton1,
     {{OpenXrButtonActionType::kPress, "/input/x/click"}},
     1},
    {OpenXrButtonType::kButton2,
     {{OpenXrButtonActionType::kPress, "/input/y/click"}},
     1},
};

constexpr OpenXrButtonPathMap kHPReverbG2RightControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/trigger/value"},
      {OpenXrButtonActionType::kValue, "/input/trigger/value"}},
     2},
    {OpenXrButtonType::kSqueeze,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/value"}},
     2},
    {OpenXrButtonType::kThumbstick,
     {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"}},
     1},
    {OpenXrButtonType::kButton1,
     {{OpenXrButtonActionType::kPress, "/input/a/click"}},
     1},
    {OpenXrButtonType::kButton2,
     {{OpenXrButtonActionType::kPress, "/input/b/click"}},
     1},
};

constexpr OpenXrButtonPathMap kGenericHandSelectGraspButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger,
     {{OpenXrButtonActionType::kPress, "/input/select/value"},
      {OpenXrButtonActionType::kValue, "/input/select/value"}},
     2},
    {OpenXrButtonType::kGrasp,
     {{OpenXrButtonActionType::kPress, "/input/squeeze/value"},
      {OpenXrButtonActionType::kValue, "/input/squeeze/value"}},
     2},
};

constexpr OpenXrAxisPathMap kMicrosoftMotionControllerAxisPathMaps[] = {
    {OpenXrAxisType::kTrackpad, "/input/trackpad"},
    {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
};

constexpr OpenXrAxisPathMap kOculusTouchControllerAxisPathMaps[] = {
    {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
};

constexpr OpenXrAxisPathMap kValveIndexControllerAxisPathMaps[] = {
    {OpenXrAxisType::kTrackpad, "/input/trackpad"},
    {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
};

constexpr OpenXrAxisPathMap kHTCViveControllerAxisPathMaps[] = {
    {OpenXrAxisType::kTrackpad, "/input/trackpad"},
};

constexpr OpenXrAxisPathMap kHPReverbG2ControllerAxisPathMaps[] = {
    {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
};

constexpr OpenXrControllerInteractionProfile
    kMicrosoftMotionInteractionProfile = {
        OpenXrInteractionProfileType::kMicrosoftMotion,
        "/interaction_profiles/microsoft/motion_controller",
        nullptr,
        GamepadMapping::kXrStandard,
        kMicrosoftMotionInputProfiles,
        base::size(kMicrosoftMotionInputProfiles),
        kMicrosoftMotionControllerButtonPathMaps,
        base::size(kMicrosoftMotionControllerButtonPathMaps),
        kMicrosoftMotionControllerButtonPathMaps,
        base::size(kMicrosoftMotionControllerButtonPathMaps),
        kMicrosoftMotionControllerAxisPathMaps,
        base::size(kMicrosoftMotionControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile kKHRSimpleInteractionProfile = {
    OpenXrInteractionProfileType::kKHRSimple,
    "/interaction_profiles/khr/simple_controller",
    nullptr,
    GamepadMapping::kNone,
    kGenericButtonInputProfiles,
    base::size(kGenericButtonInputProfiles),
    kKronosSimpleControllerButtonPathMaps,
    base::size(kKronosSimpleControllerButtonPathMaps),
    kKronosSimpleControllerButtonPathMaps,
    base::size(kKronosSimpleControllerButtonPathMaps),
    nullptr,
    0};

constexpr OpenXrControllerInteractionProfile kOculusTouchInteractionProfile = {
    OpenXrInteractionProfileType::kOculusTouch,
    "/interaction_profiles/oculus/touch_controller",
    nullptr,
    GamepadMapping::kXrStandard,
    kOculusTouchInputProfiles,
    base::size(kOculusTouchInputProfiles),
    kOculusTouchLeftControllerButtonPathMaps,
    base::size(kOculusTouchLeftControllerButtonPathMaps),
    kOculusTouchRightControllerButtonPathMaps,
    base::size(kOculusTouchRightControllerButtonPathMaps),
    kOculusTouchControllerAxisPathMaps,
    base::size(kOculusTouchControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile kValveIndexInteractionProfile = {
    OpenXrInteractionProfileType::kValveIndex,
    "/interaction_profiles/valve/index_controller",
    nullptr,
    GamepadMapping::kXrStandard,
    kValveIndexInputProfiles,
    base::size(kValveIndexInputProfiles),
    kValveIndexControllerButtonPathMaps,
    base::size(kValveIndexControllerButtonPathMaps),
    kValveIndexControllerButtonPathMaps,
    base::size(kValveIndexControllerButtonPathMaps),
    kValveIndexControllerAxisPathMaps,
    base::size(kValveIndexControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile kHTCViveInteractionProfile = {
    OpenXrInteractionProfileType::kHTCVive,
    "/interaction_profiles/htc/vive_controller",
    nullptr,
    GamepadMapping::kXrStandard,
    kHTCViveInputProfiles,
    base::size(kHTCViveInputProfiles),
    kHTCViveControllerButtonPathMaps,
    base::size(kHTCViveControllerButtonPathMaps),
    kHTCViveControllerButtonPathMaps,
    base::size(kHTCViveControllerButtonPathMaps),
    kHTCViveControllerAxisPathMaps,
    base::size(kHTCViveControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile kSamsungOdysseyInteractionProfile =
    {OpenXrInteractionProfileType::kSamsungOdyssey,
     "/interaction_profiles/samsung/odyssey_controller",
     kExtSamsungOdysseyControllerExtensionName,
     GamepadMapping::kXrStandard,
     kSamsungOdysseyInputProfiles,
     base::size(kSamsungOdysseyInputProfiles),
     kMicrosoftMotionControllerButtonPathMaps,
     base::size(kMicrosoftMotionControllerButtonPathMaps),
     kMicrosoftMotionControllerButtonPathMaps,
     base::size(kMicrosoftMotionControllerButtonPathMaps),
     kMicrosoftMotionControllerAxisPathMaps,
     base::size(kMicrosoftMotionControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile kHPReverbG2InteractionProfile = {
    OpenXrInteractionProfileType::kHPReverbG2,
    "/interaction_profiles/hp/mixed_reality_controller",
    kExtHPMixedRealityControllerExtensionName,
    GamepadMapping::kXrStandard,
    kHPReverbG2InputProfiles,
    base::size(kHPReverbG2InputProfiles),
    kHPReverbG2LeftControllerButtonPathMaps,
    base::size(kHPReverbG2LeftControllerButtonPathMaps),
    kHPReverbG2RightControllerButtonPathMaps,
    base::size(kHPReverbG2RightControllerButtonPathMaps),
    kHPReverbG2ControllerAxisPathMaps,
    base::size(kHPReverbG2ControllerAxisPathMaps)};

constexpr OpenXrControllerInteractionProfile
    kHandInteractionMSFTInteractionProfile = {
        OpenXrInteractionProfileType::kHandSelectGrasp,
        "/interaction_profiles/microsoft/hand_interaction",
        kMSFTHandInteractionExtensionName,
        GamepadMapping::kXrStandard,
        kGenericHandSelectGraspInputProfile,
        base::size(kGenericHandSelectGraspInputProfile),
        kGenericHandSelectGraspButtonPathMaps,
        base::size(kGenericHandSelectGraspButtonPathMaps),
        kGenericHandSelectGraspButtonPathMaps,
        base::size(kGenericHandSelectGraspButtonPathMaps),
        nullptr,
        0};

constexpr OpenXrControllerInteractionProfile
    kOpenXrControllerInteractionProfiles[] = {
        kMicrosoftMotionInteractionProfile,
        kKHRSimpleInteractionProfile,
        kOculusTouchInteractionProfile,
        kValveIndexInteractionProfile,
        kHTCViveInteractionProfile,
        kSamsungOdysseyInteractionProfile,
        kHPReverbG2InteractionProfile,
        kHandInteractionMSFTInteractionProfile};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_
