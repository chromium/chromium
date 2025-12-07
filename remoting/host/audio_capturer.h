// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_H_
#define REMOTING_HOST_AUDIO_CAPTURER_H_

#include <memory>

#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/protocol/audio_source.h"

namespace remoting {

class AudioCapturer : public protocol::AudioSource {
 public:
  ~AudioCapturer() override {}

  // Returns true if audio capturing is supported on this platform. If this
  // returns true, then Create() must not return nullptr.
  static bool IsSupported();
  static std::unique_ptr<AudioCapturer> Create();

  static bool IsValidSampleRate(int sample_rate);

  // For supported platforms (currently ChromeOS), specify how audio playback
  // should be handled during the CRD session. For example, should audio be
  // played on the host/local device and/or on the client/remote device.
  virtual void SetAudioPlaybackMode(AudioPlaybackMode mode);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_H_
