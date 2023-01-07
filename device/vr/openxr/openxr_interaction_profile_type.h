// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_TYPE_H_
#define DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_TYPE_H_

namespace device {
enum class OpenXrInteractionProfileType {
  kMicrosoftMotion = 0,
  kKHRSimple = 1,
  kOculusTouch = 2,
  kValveIndex = 3,
  kHTCVive = 4,
  kSamsungOdyssey = 5,
  kHPReverbG2 = 6,
  kHandSelectGrasp = 7,
  kViveCosmos = 8,
  kCount,
};
}
#endif  // DEVICE_VR_OPENXR_OPENXR_INTERACTION_PROFILE_TYPE_H_
