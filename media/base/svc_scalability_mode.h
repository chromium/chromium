// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SVC_SCALABILITY_MODE_H_
#define MEDIA_BASE_SVC_SCALABILITY_MODE_H_

#include "media/base/media_export.h"

namespace media {

// This enum class is the corresponding implementation with WebRTC-SVC.
// See https://www.w3.org/TR/webrtc-svc/#scalabilitymodes* for the detail.
enum class SVCScalabilityMode {
  kL1T2,
  kL1T3,
  kL2T1,
  kL2T2,
  kL2T3,
  kL3T1,
  kL3T2,
  kL3T3,
  kL2T1h,
  kL2T2h,
  kL2T3h,
  kS2T1,
  kS2T2,
  kS2T3,
  kS2T1h,
  kS2T2h,
  kS2T3h,
  kS3T1,
  kS3T2,
  kS3T3,
  kS3T1h,
  kS3T2h,
  kS3T3h,
  kL2T2Key,
  kL2T2KeyShift,
  kL2T3Key,
  kL2T3KeyShift,
  kL3T2Key,
  kL3T2KeyShift,
  kL3T3Key,
  kL3T3KeyShift,
};

// Gets the WebRTC-SVC Spec defined scalability mode name.
MEDIA_EXPORT const char* GetScalabilityModeName(
    SVCScalabilityMode scalability_mode);

}  // namespace media

#endif  // MEDIA_BASE_SVC_SCALABILITY_MODE_H_
