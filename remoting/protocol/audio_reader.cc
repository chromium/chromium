// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_reader.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"

namespace remoting::protocol {

AudioReader::AudioReader(AudioStub* audio_stub)
    : ChannelDispatcherBase(kAudioChannelName), audio_stub_(audio_stub) {}

AudioReader::~AudioReader() = default;

void AudioReader::OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) {
  std::unique_ptr<AudioPacket> audio_packet =
      ParseMessage<AudioPacket>(message.get());
  if (audio_packet) {
    audio_stub_->ProcessAudioPacket(std::move(audio_packet), base::DoNothing());
  }
}

}  // namespace remoting::protocol
