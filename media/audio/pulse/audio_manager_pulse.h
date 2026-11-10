// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
#define MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_

#include <pulse/pulseaudio.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/audio/audio_manager_base.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {

class PulseLoopbackManager;

// Controls AudioManagerPulse::ReconnectIfDisconnected().
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPulseReconnectOnServerRestart);

class MEDIA_EXPORT AudioManagerPulse : public AudioManagerBase {
 public:
  AudioManagerPulse(std::unique_ptr<AudioThread> audio_thread,
                    AudioLogFactory* audio_log_factory,
                    pa_threaded_mainloop* pa_mainloop,
                    pa_context* pa_context);

  AudioManagerPulse(const AudioManagerPulse&) = delete;
  AudioManagerPulse& operator=(const AudioManagerPulse&) = delete;

  ~AudioManagerPulse() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  bool GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  bool GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  const std::string_view GetName() override;

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
  void ReleaseInputStream(AudioInputStream* stream) override;

  bool DefaultSourceIsMonitor() const { return default_source_is_monitor_; }

  // Disconnects from the sound server, as if the server had gone away.
  void DisconnectForTesting();
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 protected:
  void ShutdownOnAudioThread() override;
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  bool GetAudioDeviceNames(bool input, media::AudioDeviceNames* device_names);

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
                                      const std::string& device_id,
                                      LogCallback log_callback);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id,
                                    LogCallback log_callback);

  // Updates |native_input_sample_rate_| and |native_channel_count_|.
  void UpdateNativeAudioHardwareInfo();

  // Replaces `input_mainloop_` and `input_context_` with a new connection if
  // the connection to the sound server has been lost, e.g. because the server
  // was restarted. Does nothing while `pulse_input_streams_` still use the old
  // one. An attempt blocks the calling thread for up to kReconnectTimeout, and
  // after a failed attempt the next one waits for `reconnect_backoff_`.
  void ReconnectIfDisconnected();

  raw_ptr<pa_threaded_mainloop> input_mainloop_;
  raw_ptr<pa_context> input_context_;
  raw_ptr<AudioDeviceNames> devices_;
  std::unique_ptr<PulseLoopbackManager> loopback_manager_;
  int native_input_sample_rate_;
  int native_channel_count_;
  std::string default_source_name_;
  bool default_source_is_monitor_;

  // Input streams made by MakeInputStream(), loopback ones included. They use
  // `input_mainloop_` and `input_context_`; fake input streams use neither.
  absl::flat_hash_set<raw_ptr<AudioInputStream>> pulse_input_streams_;

  raw_ptr<const base::TickClock> tick_clock_;

  // Earliest time of the next reconnect attempt.
  base::TimeTicks next_reconnect_time_;

  // Delay before the next attempt after a failed one. Doubles after each
  // failure up to kMaxReconnectBackoff, and starts over after a success.
  base::TimeDelta reconnect_backoff_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
