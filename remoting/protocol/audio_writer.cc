// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_writer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"

namespace remoting::protocol {

AudioWriter::AudioWriter() : ChannelDispatcherBase(kAudioChannelName) {}
AudioWriter::~AudioWriter() = default;

void AudioWriter::ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                                     base::OnceClosure done) {
  message_pipe()->Send(packet.get(), std::move(done));
}

// static
std::unique_ptr<AudioWriter> AudioWriter::Create(const SessionConfig& config) {
  if (!config.is_audio_enabled()) {
    return nullptr;
  }
  return base::WrapUnique(new AudioWriter());
}

void AudioWriter::OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) {
  LOG(ERROR) << "Received unexpected message on the audio channel.";
}

}  // namespace remoting::protocol
