// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_READER_H_
#define REMOTING_PROTOCOL_AUDIO_READER_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/channel_dispatcher_base.h"

namespace remoting::protocol {

class AudioStub;

class AudioReader : public ChannelDispatcherBase {
 public:
  explicit AudioReader(AudioStub* audio_stub);

  AudioReader(const AudioReader&) = delete;
  AudioReader& operator=(const AudioReader&) = delete;

  ~AudioReader() override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  raw_ptr<AudioStub> audio_stub_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUDIO_READER_H_
