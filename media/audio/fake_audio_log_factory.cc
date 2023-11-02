// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_log_factory.h"

#include <string>

namespace media {

class FakeAudioLogImpl : public AudioLog {
 public:
  FakeAudioLogImpl() = default;
  ~FakeAudioLogImpl() override = default;
  void OnCreated(const media::AudioParameters& params,
                 const std::string& device_id) override {}
  void OnStarted() override {}
  void OnStopped() override {}
  void OnClosed() override {}
  void OnError() override {}
  void OnSetVolume(double volume) override {}
  void OnProcessingStateChanged(const std::string& message) override {}
  void OnLogMessage(const std::string& message) override {}
};

FakeAudioLogFactory::FakeAudioLogFactory() = default;
FakeAudioLogFactory::~FakeAudioLogFactory() = default;

std::unique_ptr<AudioLog> FakeAudioLogFactory::CreateAudioLog(
    AudioComponent component,
    int component_id) {
  return std::make_unique<FakeAudioLogImpl>();
}

}  // namespace media
