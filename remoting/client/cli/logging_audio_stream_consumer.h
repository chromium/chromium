// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLI_LOGGING_AUDIO_STREAM_CONSUMER_H_
#define REMOTING_CLIENT_CLI_LOGGING_AUDIO_STREAM_CONSUMER_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {

class AudioPacket;

// Implementation of the protocol::AudioStub interface which logs audio packet
// info to the console.
class LoggingAudioStreamConsumer : public protocol::AudioStub {
 public:
  LoggingAudioStreamConsumer();

  LoggingAudioStreamConsumer(const LoggingAudioStreamConsumer&) = delete;
  LoggingAudioStreamConsumer& operator=(const LoggingAudioStreamConsumer&) =
      delete;

  ~LoggingAudioStreamConsumer() override;

  // protocol::AudioStub interface.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

  base::WeakPtr<LoggingAudioStreamConsumer> GetWeakPtr();

 private:
  base::WeakPtrFactory<LoggingAudioStreamConsumer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLI_LOGGING_AUDIO_STREAM_CONSUMER_H_
