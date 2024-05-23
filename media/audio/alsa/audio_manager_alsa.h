// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_
#define MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class AlsaWrapper;

class MEDIA_EXPORT AudioManagerAlsa : public AudioManagerBase {
 public:
  AudioManagerAlsa(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);

  AudioManagerAlsa(const AudioManagerAlsa&) = delete;
  AudioManagerAlsa& operator=(const AudioManagerAlsa&) = delete;

  ~AudioManagerAlsa() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
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

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  enum StreamType {
    kStreamPlayback = 0,
    kStreamCapture,
  };

  // Gets a list of available ALSA devices.
  void GetAlsaAudioDevices(StreamType type, AudioDeviceNames* device_names);

  // Gets the ALSA devices' names and ids that support streams of the
  // given type.
  void GetAlsaDevicesInfo(StreamType type,
                          void** hint,
                          AudioDeviceNames* device_names);

  // Checks if the specific ALSA device is available.
  static bool IsAlsaDeviceAvailable(StreamType type,
                                    const char* device_name);

  // Adds the switch-specified ALSA device if not present in device list.
  static void AddAlsaDeviceFromSwitch(const char* switch_name,
                                      AudioDeviceNames* device_names);

  static const char* UnwantedDeviceTypeWhenEnumerating(
      StreamType wanted_type);

  // Returns true if a device is present for the given stream type.
  bool HasAnyAlsaAudioDevice(StreamType stream);

  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  std::unique_ptr<AlsaWrapper> wrapper_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_
