// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SVC_SCALABILITY_MODE_H_
#define MEDIA_BASE_SVC_SCALABILITY_MODE_H_

#include <cstddef>
#include <vector>

#include "media/base/media_export.h"

namespace media {

// This enum class is the corresponding implementation with WebRTC-SVC.
// See https://www.w3.org/TR/webrtc-svc/#scalabilitymodes* for the detail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep the consistency with
// VideoEncoderUseCase in tools/metrics/histograms/enums.xml.
enum class SVCScalabilityMode : int {
  kL1T1 = 0,
  kL1T2 = 1,
  kL1T3 = 2,
  kL2T1 = 3,
  kL2T2 = 4,
  kL2T3 = 5,
  kL3T1 = 6,
  kL3T2 = 7,
  kL3T3 = 8,
  kL2T1h = 9,
  kL2T2h = 10,
  kL2T3h = 11,
  kS2T1 = 12,
  kS2T2 = 13,
  kS2T3 = 14,
  kS2T1h = 15,
  kS2T2h = 16,
  kS2T3h = 17,
  kS3T1 = 18,
  kS3T2 = 19,
  kS3T3 = 20,
  kS3T1h = 21,
  kS3T2h = 22,
  kS3T3h = 23,
  kL2T1Key = 24,
  kL2T2Key = 25,
  kL2T2KeyShift = 26,
  kL2T3Key = 27,
  kL2T3KeyShift = 28,
  kL3T1Key = 29,
  kL3T2Key = 30,
  kL3T2KeyShift = 31,
  kL3T3Key = 32,
  kL3T3KeyShift = 33,
  kL3T1h = 34,
  kL3T2h = 35,
  kL3T3h = 36,

  kMaxValue = kL3T3h,
};

enum class SVCInterLayerPredMode : int {
  kOff = 0,      // Inter-layer prediction is disabled.
  kOn = 1,       // Inter-layer prediction is enabled.
  kOnKeyPic = 2  // Inter-layer prediction is enabled for key picture.
};

// Gets the WebRTC-SVC Spec defined scalability mode name.
MEDIA_EXPORT const char* GetScalabilityModeName(
    SVCScalabilityMode scalability_mode);

// Gets the SVCScalabilityMode from |num_spatial_layers|,
// |num_temporal_layers| and |inter_layer_pred|.
MEDIA_EXPORT SVCScalabilityMode
GetSVCScalabilityMode(const size_t num_spatial_layers,
                      const size_t num_temporal_layers,
                      SVCInterLayerPredMode inter_layer_pred);

// Gets the supported SVCScalabilityModes by hardware encoders.
MEDIA_EXPORT std::vector<SVCScalabilityMode>
GetSupportedScalabilityModesByHWEncoderForTesting();
}  // namespace media

#endif  // MEDIA_BASE_SVC_SCALABILITY_MODE_H_
