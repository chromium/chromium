// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/webrtc_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Override the default minimum starting volume of the Automatic Gain Control
// algorithm in WebRTC used with audio tracks from getUserMedia.
// The valid range is 12-255. Values outside that range will be clamped
// to the lowest or highest valid value inside WebRTC.
// TODO(tommi): Remove this switch when crbug.com/555577 is fixed.
const char kAgcStartupMinVolume[] = "agc-startup-min-volume";

}  // namespace switches

namespace features {

// Enables multi channel capture audio to be processed without
// downmixing in the WebRTC audio processing module when running in the renderer
// process.
const base::Feature kWebRtcEnableCaptureMultiChannelApm{
    "WebRtcEnableCaptureMultiChannelApm", base::FEATURE_DISABLED_BY_DEFAULT};

// Kill-switch allowing deactivation of the support for 48 kHz internal
// processing in the WebRTC audio processing module when running on an ARM
// platform.
const base::Feature kWebRtcAllow48kHzProcessingOnArm{
    "WebRtcAllow48kHzProcessingOnArm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the WebRTC Agc2 digital adaptation with WebRTC Agc1 analog
// adaptation. Feature for http://crbug.com/873650. Is sent to WebRTC.
const base::Feature kWebRtcHybridAgc{"WebRtcHybridAgc",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
