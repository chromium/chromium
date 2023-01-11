// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_SOURCE_H_
#define REMOTING_PROTOCOL_AUDIO_SOURCE_H_

#include <memory>

#include "base/functional/callback.h"

namespace remoting {

class AudioPacket;

namespace protocol {

class AudioSource {
 public:
  typedef base::RepeatingCallback<void(std::unique_ptr<AudioPacket> packet)>
      PacketCapturedCallback;

  virtual ~AudioSource() {}

  // Capturers should sample at a 44.1 or 48 kHz sampling rate, in uncompressed
  // PCM stereo format. Capturers may choose the number of frames per packet.
  // Returns true on success.
  virtual bool Start(const PacketCapturedCallback& callback) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_SOURCE_H_
