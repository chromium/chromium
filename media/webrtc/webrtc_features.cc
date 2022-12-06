// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_features.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace features {

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based DesktopCapturer, if it is available.
BASE_FEATURE(kWebRtcAllowWgcDesktopCapturer,
             "AllowWgcDesktopCapturer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1375239): Inactivate the flag gradually before deleting it.
// When disabled, any WebRTC Audio Processing Module input volume recommendation
// is ignored and no adjustment takes place.
BASE_FEATURE(kWebRtcAllowInputVolumeAdjustment,
             "WebRtcAllowInputVolumeAdjustment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, overrides the Audio Processing Module parameter that controls
// how multi-channel capture audio is downmixed to mono (when downmixing is
// needed).
BASE_FEATURE(kWebRtcApmDownmixCaptureAudioMethod,
             "WebRtcApmDownmixCaptureAudioMethod",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
