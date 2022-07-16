// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_AUDIO_SOURCE_H_
#define REMOTING_PROTOCOL_FAKE_AUDIO_SOURCE_H_

#include "base/callback.h"
#include "remoting/protocol/audio_source.h"

namespace remoting {
namespace protocol {

class FakeAudioSource : public AudioSource {
 public:
  FakeAudioSource();

  FakeAudioSource(const FakeAudioSource&) = delete;
  FakeAudioSource& operator=(const FakeAudioSource&) = delete;

  ~FakeAudioSource() override;

  // AudioSource interface.
  bool Start(const PacketCapturedCallback& callback) override;

  const PacketCapturedCallback& callback() { return callback_; }

 private:
  PacketCapturedCallback callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_AUDIO_SOURCE_H_
