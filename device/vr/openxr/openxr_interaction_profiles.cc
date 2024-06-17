// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_interaction_profiles.h"

#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_interaction_profile_paths.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"

namespace device {

using device::mojom::OpenXrInteractionProfileType;

OpenXrSystemInputProfiles::OpenXrSystemInputProfiles(
    std::string system_name,
    std::vector<std::string> input_profiles)
    : system_name(system_name), input_profiles(input_profiles) {}
OpenXrSystemInputProfiles::~OpenXrSystemInputProfiles() = default;
OpenXrButtonPathMap::OpenXrButtonPathMap(const OpenXrButtonPathMap& other) =
    default;
OpenXrButtonPathMap& OpenXrButtonPathMap::operator=(
    const OpenXrButtonPathMap& other) = default;

OpenXrButtonPathMap::OpenXrButtonPathMap(
    OpenXrButtonType type,
    std::vector<OpenXrButtonActionPathMap> action_maps)
    : type(type), action_maps(action_maps) {}
OpenXrButtonPathMap::~OpenXrButtonPathMap() = default;
OpenXrSystemInputProfiles::OpenXrSystemInputProfiles(
    const OpenXrSystemInputProfiles& other) = default;
OpenXrSystemInputProfiles& OpenXrSystemInputProfiles::operator=(
    const OpenXrSystemInputProfiles& other) = default;

OpenXrControllerInteractionProfile::OpenXrControllerInteractionProfile(
    OpenXrInteractionProfileType type,
    std::string path,
    std::string required_extension,
    std::vector<OpenXrButtonPathMap> common_button_maps,
    std::vector<OpenXrButtonPathMap> left_button_maps,
    std::vector<OpenXrButtonPathMap> right_button_maps,
    std::vector<OpenXrAxisPathMap> axis_maps)
    : type(type),
      path(path),
      required_extension(required_extension),
      common_button_maps(common_button_maps),
      left_button_maps(left_button_maps),
      right_button_maps(right_button_maps),
      axis_maps(axis_maps) {}
OpenXrControllerInteractionProfile::~OpenXrControllerInteractionProfile() =
    default;
OpenXrControllerInteractionProfile::OpenXrControllerInteractionProfile(
    const OpenXrControllerInteractionProfile& other) = default;
OpenXrControllerInteractionProfile&
OpenXrControllerInteractionProfile::operator=(
    const OpenXrControllerInteractionProfile& other) = default;

const base::flat_map<OpenXrInteractionProfileType,
                     std::vector<OpenXrSystemInputProfiles>>&
GetOpenXrInputProfilesMap() {
  static base::NoDestructor<base::flat_map<
      OpenXrInteractionProfileType, std::vector<OpenXrSystemInputProfiles>>>
      kInputProfilesMap(base::flat_map<OpenXrInteractionProfileType,
                                       std::vector<OpenXrSystemInputProfiles>>{
          // Microsoft Motion Controller
          {OpenXrInteractionProfileType::kMicrosoftMotion,
           {{"",
             {"microsoft-mixed-reality", "windows-mixed-reality",
              "generic-trigger-squeeze-touchpad-thumbstick"}}}},

          // Khronos Simple Controller
          {OpenXrInteractionProfileType::kKHRSimple,
           {{"", {"generic-button"}}}},

          // Oculus Touch Controller
          {OpenXrInteractionProfileType::kOculusTouch,
           {{"", {"oculus-touch", "generic-trigger-squeeze-thumbstick"}},
            {"Oculus Rift S",
             {"oculus-touch-v2", "oculus-touch",
              "generic-trigger-squeeze-thumbstick"}},
            {"Oculus Quest",
             {"oculus-touch-v2", "oculus-touch",
              "generic-trigger-squeeze-thumbstick"}},
            // Oculus says this will soon be the name OpenXR reports
            {"Oculus Quest2",
             {"oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
              "generic-trigger-squeeze-thumbstick"}}}},

          // Valve Index
          {OpenXrInteractionProfileType::kValveIndex,
           {{"",
             {"valve-index", "generic-trigger-squeeze-touchpad-thumbstick"}}}},

          // Vive
          {OpenXrInteractionProfileType::kHTCVive,
           {{"", {"htc-vive", "generic-trigger-squeeze-touchpad"}}}},

          // Samsung Odyssey
          {OpenXrInteractionProfileType::kSamsungOdyssey,
           {{"",
             {"samsung-odyssey", "microsoft-mixed-reality",
              "windows-mixed-reality",
              "generic-trigger-squeeze-touchpad-thumbstick"}}}},

          // HP Reverb G2
          {OpenXrInteractionProfileType::kHPReverbG2,
           {{"",
             {"hp-mixed-reality", "oculus-touch", "generic-trigger-squeeze"}}}},

          // Microsoft Hand Interaction
          {OpenXrInteractionProfileType::kHandSelectGrasp,
           {{"",
             {"generic-hand-select-grasp", "generic-hand-select",
              "generic-fixed-hand", "generic-trigger-squeeze"}},
            {kOpenXrHandJointSystem,
             {"generic-hand-select-grasp", "generic-hand-select",
              "generic-hand", "generic-trigger-squeeze"}}}},

          // Vive Cosmos
          {OpenXrInteractionProfileType::kViveCosmos,
           {{"", {"htc-vive-cosmos", "generic-trigger-squeeze-thumbstick"}}}},

          // EXT Hand Interaction
          {OpenXrInteractionProfileType::kExtHand,
           {
               {"",
                {"generic-hand-select-grasp", "generic-hand-select",
                 "generic-fixed-hand", "generic-trigger-squeeze"}},
               {kOpenXrHandJointSystem,
                {"generic-hand-select-grasp", "generic-hand-select",
                 "generic-hand", "generic-trigger-squeeze"}},
           }},

          // XR_FB_hand_tracking_aim
          {OpenXrInteractionProfileType::kMetaHandAim,
           {{"",
             {"generic-hand-select", "generic-fixed-hand", "generic-button"}},
            {kOpenXrHandJointSystem,
             {"generic-hand-select", "generic-hand", "generic-button"}}}},
      });
  return *kInputProfilesMap;
}

const std::vector<OpenXrControllerInteractionProfile>&
GetOpenXrControllerInteractionProfiles() {
  static base::NoDestructor<std::vector<OpenXrControllerInteractionProfile>>
      kOpenXrControllerInteractionProfiles(std::vector<
                                           OpenXrControllerInteractionProfile>{
          // Microsoft Motion Controller
          {OpenXrInteractionProfileType::kMicrosoftMotion,
           kMicrosoftMotionInteractionProfilePath,
           /*required_extension=*/"",
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                }},
               {OpenXrButtonType::kTrackpad,
                {{OpenXrButtonActionType::kPress, "/input/trackpad/click"},
                 {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"}}},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/
           {{OpenXrAxisType::kTrackpad, "/input/trackpad"},
            {OpenXrAxisType::kThumbstick, "/input/thumbstick"}}},
          // Microsoft Motion Controller

          // Samsung Odyssey
          {OpenXrInteractionProfileType::kSamsungOdyssey,
           kSamsungOdysseyInteractionProfilePath,
           /*required_extension=*/
           XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME,
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                }},
               {OpenXrButtonType::kTrackpad,
                {{OpenXrButtonActionType::kPress, "/input/trackpad/click"},
                 {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"}}},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/
           {{OpenXrAxisType::kTrackpad, "/input/trackpad"},
            {OpenXrAxisType::kThumbstick, "/input/thumbstick"}}},
          // Samsung Odyssey

          // Khronos Simple Controller
          {OpenXrInteractionProfileType::kKHRSimple,
           kKHRSimpleInteractionProfilePath,
           /*required_extension=*/"",
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {{OpenXrButtonActionType::kPress, "/input/select/click"}}},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/{}},
          // Khronos Simple Controller

          // Oculus Touch Controller
          {OpenXrInteractionProfileType::kOculusTouch,
           kOculusTouchInteractionProfilePath,
           /*required_extension=*/"",
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {{OpenXrButtonActionType::kPress, "/input/trigger/value"},
                 {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                 {OpenXrButtonActionType::kTouch, "/input/trigger/touch"}}},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
                    {OpenXrButtonActionType::kValue, "/input/squeeze/value"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                    {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"},
                }},
               {OpenXrButtonType::kThumbrest,
                {
                    {OpenXrButtonActionType::kTouch, "/input/thumbrest/touch"},
                }},
           },
           /*left_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/x/click"},
                    {OpenXrButtonActionType::kTouch, "/input/x/touch"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/y/click"},
                    {OpenXrButtonActionType::kTouch, "/input/y/touch"},
                }},
               {OpenXrButtonType::kMenu,
                {{OpenXrButtonActionType::kPress, "/input/menu/click"}}},
           },
           /*right_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/a/click"},
                    {OpenXrButtonActionType::kTouch, "/input/a/touch"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/b/click"},
                    {OpenXrButtonActionType::kTouch, "/input/b/touch"},
                }},
           },
           /*axis_maps=*/
           {
               {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
           }},
          // Oculus Touch Controller

          // Valve Index
          {OpenXrInteractionProfileType::kValveIndex,
           kValveIndexInteractionProfilePath,
           /*required_extension=*/"",
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/click"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                    {OpenXrButtonActionType::kTouch, "/input/trigger/touch"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
                    {OpenXrButtonActionType::kValue, "/input/squeeze/force"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                    {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"},
                }},
               {OpenXrButtonType::kTrackpad,
                {
                    {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
                    {OpenXrButtonActionType::kValue, "/input/trackpad/force"},
                }},
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/a/click"},
                    {OpenXrButtonActionType::kTouch, "/input/a/touch"},
                }},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/
           {
               {OpenXrAxisType::kTrackpad, "/input/trackpad"},
               {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
           }},
          // Valve Index

          // HTC Vive
          {OpenXrInteractionProfileType::kHTCVive,
           kHTCViveInteractionProfilePath,
           /*required_extension=*/"",
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/click"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
                }},
               {OpenXrButtonType::kTrackpad,
                {
                    {OpenXrButtonActionType::kPress, "/input/trackpad/click"},
                    {OpenXrButtonActionType::kTouch, "/input/trackpad/touch"},
                }},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/
           {
               {OpenXrAxisType::kTrackpad, "/input/trackpad"},
           }},
          // HTC Vive

          // HP Reverb G2
          {OpenXrInteractionProfileType::kHPReverbG2,
           kHPReverbG2InteractionProfilePath,
           /*required_extension=*/
           XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME,
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
                    {OpenXrButtonActionType::kValue, "/input/squeeze/value"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                }},
           },
           /*left_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/x/click"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/y/click"},
                }},
           },
           /*right_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/a/click"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/b/click"},
                }},
           },
           /*axis_maps=*/
           {
               {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
           }},
          // HP Reverb G2

          // Microsoft Hands Profile
          {OpenXrInteractionProfileType::kHandSelectGrasp,
           kHandSelectGraspInteractionProfilePath,
           /*required_extension=*/XR_MSFT_HAND_INTERACTION_EXTENSION_NAME,
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/select/value"},
                    {OpenXrButtonActionType::kValue, "/input/select/value"},
                }},
               {OpenXrButtonType::kGrasp,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/value"},
                    {OpenXrButtonActionType::kValue, "/input/squeeze/value"},
                }},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/{}},
          // Microsoft Hands Profile

          // Vive Cosmos
          {OpenXrInteractionProfileType::kViveCosmos,
           kHTCViveCosmosInteractionProfilePath,
           /*required_extension=*/
           XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME,
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/trigger/value"},
                    {OpenXrButtonActionType::kValue, "/input/trigger/value"},
                }},
               {OpenXrButtonType::kSqueeze,
                {
                    {OpenXrButtonActionType::kPress, "/input/squeeze/click"},
                }},
               {OpenXrButtonType::kThumbstick,
                {
                    {OpenXrButtonActionType::kPress, "/input/thumbstick/click"},
                    {OpenXrButtonActionType::kTouch, "/input/thumbstick/touch"},
                }},
               {OpenXrButtonType::kShoulder,
                {
                    {OpenXrButtonActionType::kPress, "/input/shoulder/click"},
                }},
           },
           /*left_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/x/click"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/y/click"},
                }},
           },
           /*right_button_maps=*/
           {
               {OpenXrButtonType::kButton1,
                {
                    {OpenXrButtonActionType::kPress, "/input/a/click"},
                }},
               {OpenXrButtonType::kButton2,
                {
                    {OpenXrButtonActionType::kPress, "/input/b/click"},
                }},
           },
           /*axis_maps=*/
           {
               {OpenXrAxisType::kThumbstick, "/input/thumbstick"},
           }},
          // Vive Cosmos

          // EXT Hands Profile
          {OpenXrInteractionProfileType::kExtHand,
           kExtHandInteractionProfilePath,
           /*required_extension=*/XR_EXT_HAND_INTERACTION_EXTENSION_NAME,
           /*common_button_maps=*/
           {
               {OpenXrButtonType::kTrigger,
                {
                    {OpenXrButtonActionType::kPress, "/input/pinch_ext/value"},
                    {OpenXrButtonActionType::kValue, "/input/pinch_ext/value"},
                }},
               {OpenXrButtonType::kGrasp,
                {
                    {OpenXrButtonActionType::kPress, "/input/grasp_ext/value"},
                    {OpenXrButtonActionType::kValue, "/input/grasp_ext/value"},
                }},
           },
           /*left_button_maps=*/{},
           /*right_button_maps=*/{},
           /*axis_maps=*/{}},
          // EXT Hands Profile
      });
  return *kOpenXrControllerInteractionProfiles;
}

}  // namespace device
