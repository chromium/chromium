// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer.h"

#include <memory>

#include "remoting/host/linux/pulse_audio_capturer.h"

namespace remoting {

bool AudioCapturer::IsSupported() {
  return PulseAudioCapturer::IsSupported();
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  return PulseAudioCapturer::Create();
}

}  // namespace remoting
