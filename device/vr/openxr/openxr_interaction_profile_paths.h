// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_PATHS_H_
#define DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_PATHS_H_

namespace device {

inline constexpr char kMicrosoftMotionInteractionProfilePath[] =
    "/interaction_profiles/microsoft/motion_controller";
inline constexpr char kKHRSimpleInteractionProfilePath[] =
    "/interaction_profiles/khr/simple_controller";
inline constexpr char kOculusTouchInteractionProfilePath[] =
    "/interaction_profiles/oculus/touch_controller";
inline constexpr char kValveIndexInteractionProfilePath[] =
    "/interaction_profiles/valve/index_controller";
inline constexpr char kHTCViveInteractionProfilePath[] =
    "/interaction_profiles/htc/vive_controller";
inline constexpr char kSamsungOdysseyInteractionProfilePath[] =
    "/interaction_profiles/samsung/odyssey_controller";
inline constexpr char kHPReverbG2InteractionProfilePath[] =
    "/interaction_profiles/hp/mixed_reality_controller";
inline constexpr char kHandSelectGraspInteractionProfilePath[] =
    "/interaction_profiles/microsoft/hand_interaction";
inline constexpr char kHTCViveCosmosInteractionProfilePath[] =
    "/interaction_profiles/htc/vive_cosmos_controller";
inline constexpr char kExtHandInteractionProfilePath[] =
    "/interaction_profiles/ext/hand_interaction_ext";
}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_PATHS_H_
