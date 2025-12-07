// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/constants.h"

#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

int WebRtcAudioProcessingSampleRateHz() {
  return webrtc::AudioProcessing::kSampleRate48kHz;
}

}  // namespace media
