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

// Enables running WebRTC Audio Processing in the audio service, rather than
// in the renderer process. Should be combined with running the audio service
// out of the browser process, except for when testing locally.
const base::Feature kWebRtcApmInAudioService{"WebRtcApmInAudioService",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables multi channel playout and capture audio to be processed without
// downmixing in the WebRTC audio processing module.
const base::Feature kWebRtcEnableMultiChannelApm{
    "WebRtcEnableMultiChannelApm", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the WebRTC Agc2 digital adaptation with WebRTC Agc1 analog
// adaptation. Feature for http://crbug.com/873650. Is sent to WebRTC.
const base::Feature kWebRtcHybridAgc{"WebRtcHybridAgc",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace switches {

const char kForceDisableWebRtcApmInAudioService[] =
    "disable-webrtc-apm-in-audio-service";

}  // namespace switches

namespace media {

bool IsWebRtcApmInAudioServiceEnabled() {
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  return base::FeatureList::IsEnabled(features::kWebRtcApmInAudioService) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kForceDisableWebRtcApmInAudioService);
#else
  return false;
#endif
}

}  // namespace media