// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
#define MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_

#include <pulse/pulseaudio.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerPulse : public AudioManagerBase {
 public:
  AudioManagerPulse(std::unique_ptr<AudioThread> audio_thread,
                    AudioLogFactory* audio_log_factory,
                    pa_threaded_mainloop* pa_mainloop,
                    pa_context* pa_context);
  ~AudioManagerPulse() override;

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
  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;

  bool DefaultSourceIsMonitor() const { return default_source_is_monitor_; }

 protected:
  void ShutdownOnAudioThread() override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  void GetAudioDeviceNames(bool input, media::AudioDeviceNames* device_names);

  // Callback to get the devices' info like names, used by GetInputDevices().
  static void InputDevicesInfoCallback(pa_context* context,
                                       const pa_source_info* info,
                                       int eol,
                                       void* user_data);
  static void OutputDevicesInfoCallback(pa_context* context,
                                        const pa_sink_info* info,
                                        int eol,
                                        void* user_data);

  // Callback to get the native sample rate of PulseAudio, used by
  // UpdateNativeAudioHardwareInfo().
  static void AudioHardwareInfoCallback(pa_context* context,
                                        const pa_server_info* info,
                                        void* user_data);

  static void DefaultSourceInfoCallback(pa_context* context,
                                        const pa_source_info* info,
                                        int eol,
                                        void* user_data);

  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params,
                                      const std::string& device_id);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  // Updates |native_input_sample_rate_| and |native_channel_count_|.
  void UpdateNativeAudioHardwareInfo();

  pa_threaded_mainloop* input_mainloop_;
  pa_context* input_context_;
  AudioDeviceNames* devices_;
  int native_input_sample_rate_;
  int native_channel_count_;
  std::string default_source_name_;
  bool default_source_is_monitor_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerPulse);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
