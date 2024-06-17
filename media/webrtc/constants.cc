// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/constants.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

int WebRtcAudioProcessingSampleRateHz() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          ::features::kWebRtcApm48kHzSampleRateOnAndroidKillSwitch)) {
    return webrtc::AudioProcessing::kSampleRate16kHz;
  }
#endif
  return webrtc::AudioProcessing::kSampleRate48kHz;
}

}  // namespace media
