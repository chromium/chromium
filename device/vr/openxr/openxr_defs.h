// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEFS_H_
#define DEVICE_VR_OPENXR_OPENXR_DEFS_H_

namespace device {
constexpr char kExtSamsungOdysseyControllerExtensionName[] =
    "XR_EXT_samsung_odyssey_controller";
constexpr char kExtHPMixedRealityControllerExtensionName[] =
    "XR_EXT_hp_mixed_reality_controller";
constexpr char kMSFTHandInteractionExtensionName[] = "XR_MSFT_hand_interaction";

constexpr char kMicrosoftMotionInteractionProfilePath[] =
    "/interaction_profiles/microsoft/motion_controller";
constexpr char kKHRSimpleInteractionProfilePath[] =
    "/interaction_profiles/khr/simple_controller";
constexpr char kOculusTouchInteractionProfilePath[] =
    "/interaction_profiles/oculus/touch_controller";
constexpr char kValveIndexInteractionProfilePath[] =
    "/interaction_profiles/valve/index_controller";
constexpr char kHTCViveInteractionProfilePath[] =
    "/interaction_profiles/htc/vive_controller";
constexpr char kSamsungOdysseyInteractionProfilePath[] =
    "/interaction_profiles/samsung/odyssey_controller";
constexpr char kHPReverbG2InteractionProfilePath[] =
    "/interaction_profiles/hp/mixed_reality_controller";
constexpr char kHandSelectGraspInteractionProfilePath[] =
    "/interaction_profiles/microsoft/hand_interaction";
constexpr char kHTCViveCosmosInteractionProfilePath[] =
    "/interaction_profiles/htc/vive_cosmos_controller";

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEFS_H_
