// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_WRITER_H_
#define REMOTING_PROTOCOL_AUDIO_WRITER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/channel_dispatcher_base.h"

namespace remoting::protocol {

class SessionConfig;

class AudioWriter : public ChannelDispatcherBase, public AudioStub {
 public:
  // Once AudioWriter is created, the Init() method of ChannelDispatcherBase
  // should be used to initialize it for the session.
  static std::unique_ptr<AudioWriter> Create(const SessionConfig& config);

  AudioWriter(const AudioWriter&) = delete;
  AudioWriter& operator=(const AudioWriter&) = delete;

  ~AudioWriter() override;

  // AudioStub interface.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

 private:
  AudioWriter();

  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUDIO_WRITER_H_
