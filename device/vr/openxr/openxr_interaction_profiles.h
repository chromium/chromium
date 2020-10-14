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
  kCount = 7,
};

enum class OpenXrButtonType {
  kTrigger = 0,
  kSqueeze = 1,
  kTrackpad = 2,
  kThumbstick = 3,
  kThumbrest = 4,
  kButton1 = 5,
  kButton2 = 6,
  kMaxValue = 6,
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

struct OpenXrControllerInteractionProfile {
  OpenXrInteractionProfileType type;
  const char* const path;
  const char* const required_extension;
  GamepadMapping mapping;
  const char* const* const input_profiles;
  const size_t profile_size;
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
// Declare OpenXR input profile bindings for other runtimes when they become
// available.
constexpr const char* kMicrosoftMotionInputProfiles[] = {
    "windows-mixed-reality", "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr const char* kGenericButtonInputProfiles[] = {"generic-button"};

constexpr const char* kOculusTouchInputProfiles[] = {
    "oculus-touch", "generic-trigger-squeeze-thumbstick"};

constexpr const char* kValveIndexInputProfiles[] = {
    "valve-index", "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr const char* kHTCViveInputProfiles[] = {
    "htc-vive", "generic-trigger-squeeze-touchpad"};

constexpr const char* kSamsungOdysseyInputProfiles[] = {
    "samsung-odyssey", "windows-mixed-reality",
    "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr const char* kHPReverbG2InputProfiles[] = {
    "hp-mixed-reality", "oculus-touch", "generic-trigger-squeeze"};

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
    kOpenXrControllerInteractionProfiles[] = {
        kMicrosoftMotionInteractionProfile, kKHRSimpleInteractionProfile,
        kOculusTouchInteractionProfile,     kValveIndexInteractionProfile,
        kHTCViveInteractionProfile,         kSamsungOdysseyInteractionProfile,
        kHPReverbG2InteractionProfile};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILES_H_
