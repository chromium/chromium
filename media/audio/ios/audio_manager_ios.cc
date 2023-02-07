// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_manager_ios.h"

#include <memory>

#include "base/logging.h"
#include "media/audio/mac/audio_auhal_mac.h"

namespace media {

std::unique_ptr<media::AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerIOS>(std::move(audio_thread),
                                           audio_log_factory);
}

AudioManagerIOS::AudioManagerIOS(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : FakeAudioManager(std::move(audio_thread), audio_log_factory) {}

AudioManagerIOS::~AudioManagerIOS() = default;

void AudioManagerIOS::ReleaseOutputStreamUsingRealDevice(
    AudioOutputStream* stream,
    AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

const char* media::AudioManagerIOS::GetName() {
  return "iOS";
}

bool AudioManagerIOS::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size) {
  return true;
}

AudioOutputStream* AudioManagerIOS::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return MakeLowLatencyOutputStream(params, std::string(), log_callback);
}

AudioOutputStream* AudioManagerIOS::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  AUHALStream* stream =
      new AUHALStream(this, params, kAudioObjectUnknown, log_callback);
  output_streams_.push_back(stream);
  return stream;
}

}  // namespace media
