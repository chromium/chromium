// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef MEDIA_WEBRTC_CONSTANTS_H_
#define MEDIA_WEBRTC_CONSTANTS_H_

#include "build/build_config.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

// The sample rate used by (WebRTC) audio processing algorithms.
static constexpr int kAudioProcessingSampleRateHz =
#if BUILDFLAG(IS_ANDROID)
    webrtc::AudioProcessing::kSampleRate16kHz;
#else
    webrtc::AudioProcessing::kSampleRate48kHz;
#endif

}  // namespace media

#endif  // MEDIA_WEBRTC_CONSTANTS_H_
