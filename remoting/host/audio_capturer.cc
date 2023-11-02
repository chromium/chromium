// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer.h"

#include "remoting/proto/audio.pb.h"

namespace remoting {

// Returns true if the sampling rate is supported by Pepper.
bool AudioCapturer::IsValidSampleRate(int sample_rate) {
  switch (sample_rate) {
    case AudioPacket::SAMPLING_RATE_44100:
    case AudioPacket::SAMPLING_RATE_48000:
      return true;
    default:
      return false;
  }
}

}  // namespace remoting
