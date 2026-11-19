// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_OUTPUT_FRAME_METRICS_H_
#define MEDIA_GPU_ANDROID_OUTPUT_FRAME_METRICS_H_

#include <string_view>

#include "media/gpu/media_gpu_export.h"

namespace media {

// Whether a MediaCodec video decoder instance can use OutputFrames (block
// model output), or the first reason it can't. Recorded to
// Media.Android.MediaCodec.OutputFrame.Eligibility.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OutputFrameEligibility)
enum class OutputFrameEligibility {
  kEligible = 0,
  kFeatureDisabled = 1,
  kNoSurfaceControl = 2,
  kSecureCodec = 3,
  kThreadedTextureMailboxes = 4,
  kNotTextureOwner = 5,
  kSoftwareCodec = 6,
  kNoBlockModel = 7,
  kMaxValue = kNoBlockModel,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaCodecOutputFrameEligibility)

// Returns a short lowercase name for |eligibility|, e.g. "secure_codec", for
// MediaLog output.
MEDIA_GPU_EXPORT std::string_view OutputFrameEligibilityToString(
    OutputFrameEligibility eligibility);

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_OUTPUT_FRAME_METRICS_H_
