// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/pulse/audio_manager_pulse.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "build/chromeos_buildflags.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/pulse/pulse_input.h"
#include "media/audio/pulse/pulse_loopback_manager.h"
#include "media/audio/pulse/pulse_output.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

// Maximum number of output streams that can be open simultaneously.
constexpr int kMaxOutputStreams = 50;

constexpr int kMinimumOutputBufferSize = 512;
constexpr int kMaximumOutputBufferSize = 8192;
constexpr int kDefaultInputBufferSize = 1024;
constexpr int kDefaultSampleRate = 48000;
constexpr int kDefaultChannelCount = 2;

AudioManagerPulse::AudioManagerPulse(std::unique_ptr<AudioThread> audio_thread,
                                     AudioLogFactory* audio_log_factory,
                                     pa_threaded_mainloop* pa_mainloop,
                                     pa_context* pa_context)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      input_mainloop_(pa_mainloop),
      input_context_(pa_context),
      devices_(nullptr),
      native_input_sample_rate_(kDefaultSampleRate),
      native_channel_count_(kDefaultChannelCount),
      default_source_is_monitor_(false) {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerPulse::~AudioManagerPulse() = default;

void AudioManagerPulse::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();
  // The Pulse objects are the last things to be destroyed since
  // AudioManagerBase::ShutdownOnAudioThread() needs them.
  pulse::DestroyPulse(input_mainloop_.ExtractAsDangling(),
                      input_context_.ExtractAsDangling());
}

bool AudioManagerPulse::HasAudioOutputDevices() {
  AudioDeviceNames devices;
  GetAudioOutputDeviceNames(&devices);
  return !devices.empty();
}

bool AudioManagerPulse::HasAudioInputDevices() {
  AudioDeviceNames devices;
  GetAudioInputDeviceNames(&devices);
  return !devices.empty();
}

void AudioManagerPulse::GetAudioDeviceNames(
    bool input, media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  devices_ = device_names;
  pa_operation* operation = NULL;
  if (input) {
    operation = pa_context_get_source_info_list(
      input_context_, InputDevicesInfoCallback, this);
  } else {
    operation = pa_context_get_sink_info_list(
        input_context_, OutputDevicesInfoCallback, this);
  }
  WaitForOperationCompletion(input_mainloop_, operation, input_context_);

  // Prepend the default device if the list is not empty.
  if (!device_names->empty())
    device_names->push_front(AudioDeviceName::CreateDefault());
}

void AudioManagerPulse::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(true, device_names);
}

void AudioManagerPulse::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNames(false, device_names);
}

AudioParameters AudioManagerPulse::GetInputStreamParameters(
    const std::string& device_id) {
  UpdateNativeAudioHardwareInfo();

  {
    AutoPulseLock auto_lock(input_mainloop_);
    auto* operation = pa_context_get_source_info_by_name(
        input_context_, default_source_name_.c_str(), DefaultSourceInfoCallback,
        this);
    WaitForOperationCompletion(input_mainloop_, operation, input_context_);
  }

  // We don't want to accidentally open a monitor device, so return invalid
  // parameters for those. Note: The value of |default_source_is_monitor_|
  // depends on the the call to pa_context_get_source_info_by_name() above.
  if (device_id == AudioDeviceDescription::kDefaultDeviceId &&
      default_source_is_monitor_) {
    return AudioParameters();
  }

  const int user_buffer_size = GetUserBufferSize();
  const int buffer_size =
      user_buffer_size ? user_buffer_size : kDefaultInputBufferSize;
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(),
                         native_input_sample_rate_ ? native_input_sample_rate_
                                                   : kDefaultSampleRate,
                         buffer_size);
}

const char* AudioManagerPulse::GetName() {
  return "PulseAudio";
}

AudioOutputStream* AudioManagerPulse::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId,
                          log_callback);
}

AudioOutputStream* AudioManagerPulse::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(
      params,
      device_id.empty() ? AudioDeviceDescription::kDefaultDeviceId : device_id,
      log_callback);
}

AudioInputStream* AudioManagerPulse::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id, log_callback);
}

AudioInputStream* AudioManagerPulse::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id, log_callback);
}

std::string AudioManagerPulse::GetDefaultInputDeviceID() {
  // Do not use the real default input device since it is a fallback
  // device rather than a default device. Using the default input device
  // reported by Pulse Audio prevents, for example, input redirection
  // using the PULSE_SOURCE environment variable.
  return AudioManagerBase::GetDefaultInputDeviceID();
}

std::string AudioManagerPulse::GetDefaultOutputDeviceID() {
  // Do not use the real default output device since it is a fallback
  // device rather than a default device. Using the default output device
  // reported by Pulse Audio prevents, for example, output redirection
  // using the PULSE_SINK environment variable.
  return AudioManagerBase::GetDefaultOutputDeviceID();
}

std::string AudioManagerPulse::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return AudioManagerBase::GetAssociatedOutputDeviceID(input_device_id);
#else
  DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(input_mainloop_);
  DCHECK(input_context_);

  if (input_device_id == AudioDeviceDescription::kDefaultDeviceId)
    return std::string();

  std::string input_bus =
      pulse::GetBusOfInput(input_mainloop_, input_context_, input_device_id);
  return input_bus.empty() ? std::string()
                           : pulse::GetOutputCorrespondingTo(
                                 input_mainloop_, input_context_, input_bus);
#endif
}

AudioParameters AudioManagerPulse::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  VLOG_IF(0, !output_device_id.empty()) << "Not implemented!";

  int buffer_size = kMinimumOutputBufferSize;

  // Query native parameters where applicable; Pulse does not require these to
  // be respected though, so prefer the input parameters for channel count.
  UpdateNativeAudioHardwareInfo();
  int sample_rate = native_input_sample_rate_ ? native_input_sample_rate_
                                              : kDefaultSampleRate;
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Guess(
      native_channel_count_ ? native_channel_count_ : 2);

  if (input_params.IsValid()) {
    // Use the system's output channel count for the DISCRETE layout. This is to
    // avoid a crash due to the lack of support on the multi-channel beyond 8 in
    // the PulseAudio layer.
    if (input_params.channel_layout() != CHANNEL_LAYOUT_DISCRETE)
      channel_layout_config = input_params.channel_layout_config();

    buffer_size =
        std::min(kMaximumOutputBufferSize,
                 std::max(buffer_size, input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
}

AudioOutputStream* AudioManagerPulse::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    LogCallback log_callback) {
  DCHECK(!device_id.empty());
  return new PulseAudioOutputStream(params, device_id, this,
                                    std::move(log_callback));
}

AudioInputStream* AudioManagerPulse::MakeInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    LogCallback log_callback) {
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    // We need a loopback manager if we are opening a loopback device.
    if (!loopback_manager_) {
      // Unretained is safe as `this` outlives `loopback_manager_` and all
      // streams. See ~AudioManagerBase.
      loopback_manager_ = PulseLoopbackManager::Create(
          base::BindRepeating(&AudioManagerBase::ReleaseInputStream,
                              base::Unretained(this)),
          input_context_, input_mainloop_);
    }
    bool should_mute_system_audio =
        (device_id == AudioDeviceDescription::kLoopbackWithMuteDeviceId);
    if (loopback_manager_) {
      return loopback_manager_->MakeLoopbackStream(
          params, std::move(log_callback), should_mute_system_audio);
    }

    return nullptr;
  }

  return new PulseAudioInputStream(this, device_id, params, input_mainloop_,
                                   input_context_, std::move(log_callback));
}

void AudioManagerPulse::UpdateNativeAudioHardwareInfo() {
  DCHECK(input_mainloop_);
  DCHECK(input_context_);
  AutoPulseLock auto_lock(input_mainloop_);
  pa_operation* operation = pa_context_get_server_info(
      input_context_, AudioHardwareInfoCallback, this);
  WaitForOperationCompletion(input_mainloop_, operation, input_context_);

  // Be careful about adding OS calls to this method.
  // GetPreferredOutputStreamParameters() calls this method on a critical path.
  // If the OS calls hang they will hang all device authorizations.
}

void AudioManagerPulse::InputDevicesInfoCallback(pa_context* context,
                                                 const pa_source_info* info,
                                                 int eol,
                                                 void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (eol) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  // Exclude output monitor (i.e. loopback) devices.
  if (info->monitor_of_sink != PA_INVALID_INDEX)
    return;

  // If the device has ports, but none of them are available, skip it.
  if (info->n_ports > 0) {
    uint32_t port = 0;
    for (; port != info->n_ports; ++port) {
      if (info->ports[port]->available != PA_PORT_AVAILABLE_NO)
        break;
    }
    if (port == info->n_ports)
      return;
  }

  manager->devices_->push_back(AudioDeviceName(info->description, info->name));
}

void AudioManagerPulse::OutputDevicesInfoCallback(pa_context* context,
                                                  const pa_sink_info* info,
                                                  int eol,
                                                  void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  if (eol) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  manager->devices_->push_back(AudioDeviceName(info->description, info->name));
}

void AudioManagerPulse::AudioHardwareInfoCallback(pa_context* context,
                                                  const pa_server_info* info,
                                                  void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);

  manager->native_input_sample_rate_ = info->sample_spec.rate;
  manager->native_channel_count_ = info->sample_spec.channels;
  if (info->default_source_name)
    manager->default_source_name_ = info->default_source_name;
  pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
}

void AudioManagerPulse::DefaultSourceInfoCallback(pa_context* context,
                                                  const pa_source_info* info,
                                                  int eol,
                                                  void* user_data) {
  AudioManagerPulse* manager = reinterpret_cast<AudioManagerPulse*>(user_data);
  if (eol) {
    // Signal the pulse object that it is done.
    pa_threaded_mainloop_signal(manager->input_mainloop_, 0);
    return;
  }

  DCHECK(info);
  manager->default_source_is_monitor_ =
      info->monitor_of_sink != PA_INVALID_INDEX;
}

}  // namespace media
