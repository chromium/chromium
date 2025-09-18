// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/cli/logging_audio_stream_consumer.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "remoting/client/common/logging.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

LoggingAudioStreamConsumer::LoggingAudioStreamConsumer() = default;

LoggingAudioStreamConsumer::~LoggingAudioStreamConsumer() = default;

void LoggingAudioStreamConsumer::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet,
    base::OnceClosure done) {
  CLIENT_LOG << "ProcessAudioPacket encoding: " << packet->encoding()
             << ", sampling_rate: " << packet->sampling_rate()
             << ", channels: " << packet->channels()
             << ", bytes per sample: " << packet->bytes_per_sample();
  std::move(done).Run();
}

base::WeakPtr<LoggingAudioStreamConsumer>
LoggingAudioStreamConsumer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
