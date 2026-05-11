// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_SAMPLE_INFO_H_
#define REMOTING_PROTOCOL_AUDIO_SAMPLE_INFO_H_

#include <cstdint>

namespace remoting::protocol {

// Describes the audio sampling configuration (rate and channels) of an SPSC
// audio stream.
struct AudioSampleInfo {
  bool operator==(const AudioSampleInfo& other) const = default;

  // The sampling rate in Hz (e.g. 48000).
  uint32_t sampling_rate = 0;

  // The number of audio channels (e.g. 2 for stereo).
  uint8_t channels = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUDIO_SAMPLE_INFO_H_
