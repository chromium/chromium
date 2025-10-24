// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_features.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace features {

// When enabled we will tell WebRTC that we want to use the
// Windows.Graphics.Capture API based screen capturer, if it is available.
BASE_FEATURE(kWebRtcAllowWgcScreenCapturer,
             "AllowWgcScreenCapturer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled we will tell WebRTC that we want to use the 0Hz mode of the
// Windows.Graphics.Capture API based {Screen/Window} capturer, if the WGC
// capturer is available and enabled. In this mode, only frames with new content
// will be sent to the client leading to to 0fps for a static {Screen/Window}
// source.
// This flag only has an effect if kWebRtcAllowWgcScreenCapturer is enabled.
BASE_FEATURE(kWebRtcAllowWgcScreenZeroHz,
             "AllowWgcScreenZeroHz",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// When enabled, instruct WGC to draw a border around the captured
// window or screen.
BASE_FEATURE(kWebRtcWgcRequireBorder, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// TODO(crbug.com/40872787): Deactivate the flag gradually before deleting it.
// When disabled, any WebRTC Audio Processing Module input volume recommendation
// is ignored and no adjustment takes place.
BASE_FEATURE(kWebRtcAllowInputVolumeAdjustment,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, overrides the Audio Processing Module parameter that controls
// how multi-channel capture audio is downmixed to mono (when downmixing is
// needed).
BASE_FEATURE(kWebRtcApmDownmixCaptureAudioMethod,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allow H.265 codec to be used for sending WebRTC streams.
// Platform hardware H.265 encoder needs to be supported and enabled in order to
// negotiate usage of H.265 in SDP in the direction of sending.
BASE_FEATURE(kWebRtcAllowH265Send, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, allow H.265 codec to be used for receiving WebRTC streams.
// Platform hardware H.265 decoder needs to be supported and enabled in order to
// negotiate usage of H.265 in SDP in the direction of receiving.
BASE_FEATURE(kWebRtcAllowH265Receive, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, allows H.265 L1T2 to be used for sending WebRTC streams, if the
// accelerator reports support of encoding in L1T2.
BASE_FEATURE(kWebRtcH265L1T2, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows H.265 L1T3 to be used for sending WebRTC streams, if the
// accelerator reports support of encoding in L1T3. If this is enabled, L1T2 is
// also implied to be enabled.
BASE_FEATURE(kWebRtcH265L1T3, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, allows AV1 HW encoding to be used for WebRTC streams, if the
// platform accelerator supports encoding of AV1.
BASE_FEATURE(kWebRtcAV1HWEncode,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);

bool IsOpenH264SoftwareEncoderEnabledForWebRTC() {
// TODO(crbug.com/355256378): OpenH264 for encoding and FFmpeg for H264 decoding
// should be detangled such that software decoding can be enabled without
// software encoding.
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && BUILDFLAG(ENABLE_OPENH264)
  return base::FeatureList::IsEnabled(media::kOpenH264SoftwareEncoder);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && BUILDFLAG(ENABLE_OPENH264)
}

}  // namespace features
