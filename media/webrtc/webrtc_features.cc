// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_features.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace features {

// When enabled we will tell WebRTC that we want to use the 0Hz mode of the
// DirectX and GDI based DesktopCapturers in combination with sharing the screen
// or a separate window. In this mode, only frames with new content will be sent
// to the client leading to to 0fps for a static source. A special flag called
// kWebRtcAllowWgcZeroHz is used to specify the support when using the WGC
// capture API.
// This flag has no effect if kWebRtcAllowWgcDesktopCapturer is enabled. Use
// kWebRtcAllowWgcZeroHz for WGC.
BASE_FEATURE(kWebRtcAllowDxgiGdiZeroHz,
             "AllowDxgiGdiZeroHz",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based screen capturer, if it is available.
BASE_FEATURE(kWebRtcAllowWgcScreenCapturer,
             "AllowWgcScreenCapturer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based window capturer, if it is available.
BASE_FEATURE(kWebRtcAllowWgcWindowCapturer,
             "AllowWgcWindowCapturer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled we will tell WebRTC that we want to use the 0Hz mode of the
// Windows.Graphics.Capture API based DesktopCapturer, if the WGC capturer is
// available and enabled. In this mode, only frames with new content will be
// sent to the client leading to to 0fps for a static source.
// This flag has no effect if kWebRtcAllowWgcScreenCapturer and
// kWebRtcAllowWgcWindowCapturer are both disabled.
BASE_FEATURE(kWebRtcAllowWgcZeroHz,
             "AllowWgcZeroHz",
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

// When enabled, the Audio Processing Module is used to determine whether the
// playout reference is needed. Otherwise the decision is based on
// `media::AudioProcessingSettings`.
BASE_FEATURE(kWebRtcApmTellsIfPlayoutReferenceIsNeeded,
             "WebRtcApmTellsIfPlayoutReferenceIsNeeded",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
