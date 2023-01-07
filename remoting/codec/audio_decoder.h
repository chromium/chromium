// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_H_
#define REMOTING_CODEC_AUDIO_DECODER_H_

#include <memory>

namespace remoting {

namespace protocol {
class SessionConfig;
}  // namespace protocol

class AudioPacket;

class AudioDecoder {
 public:
  static std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      const protocol::SessionConfig& config);

  virtual ~AudioDecoder() {}

  // Returns the decoded packet. If the packet is invalid, then a NULL
  // std::unique_ptr is returned.
  virtual std::unique_ptr<AudioPacket> Decode(
      std::unique_ptr<AudioPacket> packet) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_H_
