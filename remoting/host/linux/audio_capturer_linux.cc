// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer.h"

#include <memory>

#include "remoting/host/linux/pipewire_audio_capturer.h"
#include "remoting/host/linux/pulse_audio_capturer.h"

namespace remoting {

bool AudioCapturer::IsSupported() {
  return PulseAudioCapturer::IsSupported() ||
         PipewireAudioCapturer::IsSupported();
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  // If PulseAudioCapturer is supported, then it means there is already a
  // default audio sink, and we can't use PipewireAudioCapturer.
  // PipewireAudioCapturer will create another audio sink, which will cause
  // confusion to users and won't be able to capture audio in most cases since
  // it is not the default audio sink.
  // TODO: yuweih - See if we can fix linux_me2me_host.py to prevent
  // pipewire-pulse from running while keeping legacy PulseAudio support.
  if (PulseAudioCapturer::IsSupported()) {
    return PulseAudioCapturer::Create();
  }
  return PipewireAudioCapturer::Create();
}

}  // namespace remoting
