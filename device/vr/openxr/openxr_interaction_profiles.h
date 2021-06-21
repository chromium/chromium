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
  const OpenXrButtonActionPathMap* action_maps;
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
  const char* const* input_profiles;
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
const char* const kMicrosftMotionDefaultInputProfiles[] = {
    "windows-mixed-reality", "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr OpenXrSystemInputProfiles kMicrosoftMotionInputProfiles[] = {
    {nullptr, kMicrosftMotionDefaultInputProfiles,
     base::size(kMicrosftMotionDefaultInputProfiles)}};

const char* const kGenericButtonDefaultInputProfiles[] = {"generic-button"};

constexpr OpenXrSystemInputProfiles kGenericButtonInputProfiles[] = {
    {nullptr, kGenericButtonDefaultInputProfiles,
     base::size(kGenericButtonDefaultInputProfiles)}};

const char* const kOculusTouchV2InputProfiles[] = {
    "oculus-touch-v2", "oculus-touch", "generic-trigger-squeeze-thumbstick"};

const char* const kOculusTouchV3InputProfiles[] = {
    "oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
    "generic-trigger-squeeze-thumbstick"};

const char* const kOculusTouchDefaultInputProfiles[] = {
    "oculus-touch", "generic-trigger-squeeze-thumbstick"};

constexpr OpenXrSystemInputProfiles kOculusTouchInputProfiles[] = {
    {nullptr, kOculusTouchDefaultInputProfiles,
     base::size(kOculusTouchDefaultInputProfiles)},
    {"Oculus Rift S", kOculusTouchV2InputProfiles,
     base::size(kOculusTouchV2InputProfiles)},
    {"Quest", kOculusTouchV2InputProfiles,
     base::size(kOculusTouchV2InputProfiles)},
    // Name currently reported by OpenXR for the Quest 2
    {"Miramar", kOculusTouchV3InputProfiles,
     base::size(kOculusTouchV3InputProfiles)},
    // Oculus says this will soon be the name OpenXR reports
    {"Oculus Quest2", kOculusTouchV3InputProfiles,
     base::size(kOculusTouchV3InputProfiles)}};

const char* const kValveIndexDefaultInputProfiles[] = {
    "valve-index", "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr OpenXrSystemInputProfiles kValveIndexInputProfiles[] = {
    {nullptr, kValveIndexDefaultInputProfiles,
     base::size(kValveIndexDefaultInputProfiles)}};

const char* const kHTCViveDefaultInputProfiles[] = {
    "htc-vive", "generic-trigger-squeeze-touchpad"};

constexpr OpenXrSystemInputProfiles kHTCViveInputProfiles[] = {
    {nullptr, kHTCViveDefaultInputProfiles,
     base::size(kHTCViveDefaultInputProfiles)}};

const char* const kSamsungOdysseyDefaultInputProfiles[] = {
    "samsung-odyssey", "windows-mixed-reality",
    "generic-trigger-squeeze-touchpad-thumbstick"};

constexpr OpenXrSystemInputProfiles kSamsungOdysseyInputProfiles[] = {
    {nullptr, kSamsungOdysseyDefaultInputProfiles,
     base::size(kSamsungOdysseyDefaultInputProfiles)}};

const char* const kHPReverbG2DefaultInputProfiles[] = {
    "hp-mixed-reality", "oculus-touch", "generic-trigger-squeeze"};

constexpr OpenXrSystemInputProfiles kHPReverbG2InputProfiles[] = {
    {nullptr, kHPReverbG2DefaultInputProfiles,
     base::size(kHPReverbG2DefaultInputProfiles)}};

const char* const kGenericHandSelectGraspDefaultInputProfiles[] = {
    "generic-hand-select-grasp", "generic-hand-select"};

constexpr OpenXrSystemInputProfiles kGenericHandSelectGraspInputProfile[] = {
    {nullptr, kGenericHandSelectGraspDefaultInputProfiles,
     base::size(kGenericHandSelectGraspDefaultInputProfiles)}};

constexpr OpenXrButtonActionPathMap kMicrosoftMotionControllerTriggerMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
};

constexpr OpenXrButtonActionPathMap kMicrosoftMotionControllerSqueezeMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
};

constexpr OpenXrButtonActionPathMap kMicrosoftMotionControllerThumbstickMaps[] =
    {{OpenXrButtonActionType::kPress, "/input/thumbstick/click"}};

constexpr OpenXrButtonActionPathMap kMicrosoftMotionControllerTrackpadMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trackpad/click"},
    {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
};

constexpr OpenXrButtonPathMap kMicrosoftMotionControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kMicrosoftMotionControllerTriggerMaps,
     base::size(kMicrosoftMotionControllerTriggerMaps)},
    {OpenXrButtonType::kSqueeze, kMicrosoftMotionControllerSqueezeMaps,
     base::size(kMicrosoftMotionControllerSqueezeMaps)},
    {OpenXrButtonType::kThumbstick, kMicrosoftMotionControllerThumbstickMaps,
     base::size(kMicrosoftMotionControllerThumbstickMaps)},
    {OpenXrButtonType::kTrackpad, kMicrosoftMotionControllerTrackpadMaps,
     base::size(kMicrosoftMotionControllerTrackpadMaps)}};

constexpr OpenXrButtonActionPathMap kKronosSimpleControllerTriggerMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/select/click"},
};

constexpr OpenXrButtonPathMap kKronosSimpleControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kKronosSimpleControllerTriggerMaps,
     base::size(kKronosSimpleControllerTriggerMaps)},
};

constexpr OpenXrButtonActionPathMap kOculusTouchControllerTriggerMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
    {OpenXrButtonActionType::kTouch, "/input/trigger/touch"}};

constexpr OpenXrButtonActionPathMap kOculusTouchControllerSqueezeMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
    {OpenXrButtonActionType::kValue, "/input/squeeze/value"}};

constexpr OpenXrButtonActionPathMap kOculusTouchControllerThumbstickMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
    {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"}};

constexpr OpenXrButtonActionPathMap kOculusTouchControllerThumbrestMaps[] = {
    {OpenXrButtonActionType::kTouch, "/input/thumbrest/touch"}};

constexpr OpenXrButtonActionPathMap
    kOculusTouchLeftControllerButton1PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/x/click"},
        {OpenXrButtonActionType::kTouch, "/input/x/touch"},
};

constexpr OpenXrButtonActionPathMap
    kOculusTouchLeftControllerButton2PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/y/click"},
        {OpenXrButtonActionType::kTouch, "/input/y/touch"},
};

constexpr OpenXrButtonPathMap kOculusTouchLeftControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kOculusTouchControllerTriggerMaps,
     base::size(kOculusTouchControllerTriggerMaps)},
    {OpenXrButtonType::kSqueeze, kOculusTouchControllerSqueezeMaps,
     base::size(kOculusTouchControllerSqueezeMaps)},
    {OpenXrButtonType::kThumbstick, kOculusTouchControllerThumbstickMaps,
     base::size(kOculusTouchControllerThumbstickMaps)},
    {OpenXrButtonType::kThumbrest, kOculusTouchControllerThumbrestMaps,
     base::size(kOculusTouchControllerThumbrestMaps)},
    {OpenXrButtonType::kButton1, kOculusTouchLeftControllerButton1PathMaps,
     base::size(kOculusTouchLeftControllerButton1PathMaps)},
    {OpenXrButtonType::kButton2, kOculusTouchLeftControllerButton2PathMaps,
     base::size(kOculusTouchLeftControllerButton2PathMaps)},
};

constexpr OpenXrButtonActionPathMap
    kOculusTouchRightControllerButton1PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/a/click"},
        {OpenXrButtonActionType::kTouch, "/input/a/touch"},
};

constexpr OpenXrButtonActionPathMap
    kOculusTouchRightControllerButton2PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/b/click"},
        {OpenXrButtonActionType::kTouch, "/input/b/touch"},
};

constexpr OpenXrButtonPathMap kOculusTouchRightControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kOculusTouchControllerTriggerMaps,
     base::size(kOculusTouchControllerTriggerMaps)},
    {OpenXrButtonType::kSqueeze, kOculusTouchControllerSqueezeMaps,
     base::size(kOculusTouchControllerSqueezeMaps)},
    {OpenXrButtonType::kThumbstick, kOculusTouchControllerThumbstickMaps,
     base::size(kOculusTouchControllerThumbstickMaps)},
    {OpenXrButtonType::kThumbrest, kOculusTouchControllerThumbrestMaps,
     base::size(kOculusTouchControllerThumbrestMaps)},
    {OpenXrButtonType::kButton1, kOculusTouchRightControllerButton1PathMaps,
     base::size(kOculusTouchRightControllerButton1PathMaps)},
    {OpenXrButtonType::kButton2, kOculusTouchRightControllerButton2PathMaps,
     base::size(kOculusTouchRightControllerButton2PathMaps)},
};

constexpr OpenXrButtonActionPathMap kValveIndexControllerTriggerPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trigger/click"},
    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
    {OpenXrButtonActionType::kTouch, "/input/trigger/touch"},
};

constexpr OpenXrButtonActionPathMap kValveIndexControllerSqueezePathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
    {OpenXrButtonActionType::kValue, "/input/squeeze/force"},
};

constexpr OpenXrButtonActionPathMap kValveIndexControllerThumbstickPathMaps[] =
    {
        {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
        {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"},
};

constexpr OpenXrButtonActionPathMap kValveIndexControllerTrackpadPathMaps[] = {
    {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
    {OpenXrButtonActionType::kValue, "/input/trackpad/force"},
};

constexpr OpenXrButtonActionPathMap kValveIndexControllerButton1PathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/a/click"},
    {OpenXrButtonActionType::kTouch, "/input/a/touch"},
};

constexpr OpenXrButtonPathMap kValveIndexControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kValveIndexControllerTriggerPathMaps,
     base::size(kValveIndexControllerTriggerPathMaps)},
    {OpenXrButtonType::kSqueeze, kValveIndexControllerSqueezePathMaps,
     base::size(kValveIndexControllerSqueezePathMaps)},
    {OpenXrButtonType::kThumbstick, kValveIndexControllerThumbstickPathMaps,
     base::size(kValveIndexControllerThumbstickPathMaps)},
    {OpenXrButtonType::kTrackpad, kValveIndexControllerTrackpadPathMaps,
     base::size(kValveIndexControllerTrackpadPathMaps)},
    {OpenXrButtonType::kButton1, kValveIndexControllerButton1PathMaps,
     base::size(kValveIndexControllerButton1PathMaps)},
};

constexpr OpenXrButtonActionPathMap kHTCViveControllerTriggerPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trigger/click"},
    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
};

constexpr OpenXrButtonActionPathMap kHTCViveControllerSqueezePathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
};

constexpr OpenXrButtonActionPathMap kHTCViveControllerTrackpadPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trackpad/click"},
    {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
};

constexpr OpenXrButtonPathMap kHTCViveControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kHTCViveControllerTriggerPathMaps,
     base::size(kHTCViveControllerTriggerPathMaps)},
    {OpenXrButtonType::kSqueeze, kHTCViveControllerSqueezePathMaps,
     base::size(kHTCViveControllerSqueezePathMaps)},
    {OpenXrButtonType::kTrackpad, kHTCViveControllerTrackpadPathMaps,
     base::size(kHTCViveControllerTrackpadPathMaps)},
};

constexpr OpenXrButtonActionPathMap kHPReverbG2ControllerTriggerPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
};

constexpr OpenXrButtonActionPathMap kHPReverbG2ControllerSqueezePathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
    {OpenXrButtonActionType::kValue, "/input/squeeze/value"},
};

constexpr OpenXrButtonActionPathMap kHPReverbG2ControllerThumbstickPathMaps[] =
    {
        {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
};

constexpr OpenXrButtonActionPathMap kHPReverbG2LeftControllerButton1PathMaps[] =
    {
        {OpenXrButtonActionType::kPress, "/input/x/click"},
};

constexpr OpenXrButtonActionPathMap kHPReverbG2LeftControllerButton2PathMaps[] =
    {
        {OpenXrButtonActionType::kPress, "/input/y/click"},
};

constexpr OpenXrButtonPathMap kHPReverbG2LeftControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kHPReverbG2ControllerTriggerPathMaps,
     base::size(kHPReverbG2ControllerTriggerPathMaps)},
    {OpenXrButtonType::kSqueeze, kHPReverbG2ControllerSqueezePathMaps,
     base::size(kHPReverbG2ControllerSqueezePathMaps)},
    {OpenXrButtonType::kThumbstick, kHPReverbG2ControllerThumbstickPathMaps,
     base::size(kHPReverbG2ControllerThumbstickPathMaps)},
    {OpenXrButtonType::kButton1, kHPReverbG2LeftControllerButton1PathMaps,
     base::size(kHPReverbG2LeftControllerButton1PathMaps)},
    {OpenXrButtonType::kButton2, kHPReverbG2LeftControllerButton2PathMaps,
     base::size(kHPReverbG2LeftControllerButton2PathMaps)},
};

constexpr OpenXrButtonActionPathMap
    kHPReverbG2RightControllerButton1PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/a/click"},
};

constexpr OpenXrButtonActionPathMap
    kHPReverbG2RightControllerButton2PathMaps[] = {
        {OpenXrButtonActionType::kPress, "/input/b/click"},
};

constexpr OpenXrButtonPathMap kHPReverbG2RightControllerButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kHPReverbG2ControllerTriggerPathMaps,
     base::size(kHPReverbG2ControllerTriggerPathMaps)},
    {OpenXrButtonType::kSqueeze, kHPReverbG2ControllerSqueezePathMaps,
     base::size(kHPReverbG2ControllerSqueezePathMaps)},
    {OpenXrButtonType::kThumbstick, kHPReverbG2ControllerThumbstickPathMaps,
     base::size(kHPReverbG2ControllerThumbstickPathMaps)},
    {OpenXrButtonType::kButton1, kHPReverbG2RightControllerButton1PathMaps,
     base::size(kHPReverbG2RightControllerButton1PathMaps)},
    {OpenXrButtonType::kButton2, kHPReverbG2RightControllerButton2PathMaps,
     base::size(kHPReverbG2RightControllerButton2PathMaps)},
};

constexpr OpenXrButtonActionPathMap kGenericHandSelectTriggerPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/select/value"},
    {OpenXrButtonActionType::kValue, "/input/select/value"},
};

constexpr OpenXrButtonActionPathMap kGenericHandSelectGraspPathMaps[] = {
    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
    {OpenXrButtonActionType::kValue, "/input/squeeze/value"},
};

constexpr OpenXrButtonPathMap kGenericHandSelectGraspButtonPathMaps[] = {
    {OpenXrButtonType::kTrigger, kGenericHandSelectTriggerPathMaps,
     base::size(kGenericHandSelectTriggerPathMaps)},
    {OpenXrButtonType::kGrasp, kGenericHandSelectGraspPathMaps,
     base::size(kGenericHandSelectGraspPathMaps)},
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
