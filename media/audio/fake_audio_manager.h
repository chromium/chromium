// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_
#define MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_

#include <string>
#include "base/compiler_specific.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"

namespace media {

class MEDIA_EXPORT FakeAudioManager : public AudioManagerBase {
 public:
  FakeAudioManager(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);

  FakeAudioManager(const FakeAudioManager&) = delete;
  FakeAudioManager& operator=(const FakeAudioManager&) = delete;

  ~FakeAudioManager() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  const char* GetName() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_MANAGER_H_
