// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/ios/audio_private_api.h"
#include "media/audio/mac/audio_auhal_mac.h"

namespace media {

class AUHALStream;
class AudioSessionManagerIOS;

// iOS implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
// TODO(crbug.com/1413450): Fill this implementation out.
class MEDIA_EXPORT AudioManagerIOS : public AudioManagerBase,
                                     public AudioIOStreamClient {
 public:
  AudioManagerIOS(std::unique_ptr<AudioThread> audio_thread,
                  AudioLogFactory* audio_log_factory);

  AudioManagerIOS(const AudioManagerIOS&) = delete;
  AudioManagerIOS& operator=(const AudioManagerIOS&) = delete;

  ~AudioManagerIOS() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& input_device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
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

  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;

  // Implementation of AudioIOStreamClient.
  void ReleaseOutputStreamUsingRealDevice(AudioOutputStream* stream,
                                          AudioDeviceID device_id) override;
  void ReleaseInputStreamUsingRealDevice(AudioInputStream* stream) override;
  bool MaybeChangeBufferSize(AudioDeviceID device_id,
                             AudioUnit audio_unit,
                             AudioUnitElement element,
                             size_t desired_buffer_size) override;

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  std::unique_ptr<AudioSessionManagerIOS> audio_session_manager_;

  // Tracks all constructed input and output streams.
  std::list<AUHALStream*> output_streams_;
  std::list<AudioInputStream*> basic_input_streams_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_
