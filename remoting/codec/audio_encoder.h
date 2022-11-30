// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_H_
#define REMOTING_CODEC_AUDIO_ENCODER_H_

#include <memory>

namespace remoting {

class AudioPacket;

class AudioEncoder {
 public:
  virtual ~AudioEncoder() {}

  virtual std::unique_ptr<AudioPacket> Encode(
      std::unique_ptr<AudioPacket> packet) = 0;

  // Returns average bitrate for the stream in bits per second.
  virtual int GetBitrate() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_H_
