// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_manager_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scheduler.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/audio/fuchsia/audio_input_stream_fuchsia.h"
#include "media/audio/fuchsia/audio_output_stream_fuchsia.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_switches.h"

namespace media {

constexpr base::TimeDelta kMinBufferPeriod = base::kAudioSchedulingPeriod;
constexpr base::TimeDelta kMaxBufferPeriod = base::Seconds(1);

AudioManagerFuchsia::AudioManagerFuchsia(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioManagerFuchsia::InitOnAudioThread,
                                base::Unretained(this)));
}

AudioManagerFuchsia::~AudioManagerFuchsia() = default;

void AudioManagerFuchsia::ShutdownOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  AudioManagerBase::ShutdownOnAudioThread();

  // Teardown the AudioDeviceEnumerator channel before the audio
  // thread, which it is bound to, stops.
  enumerator_ = nullptr;
}

bool AudioManagerFuchsia::HasAudioOutputDevices() {
  return HasAudioDevice(false);
}

bool AudioManagerFuchsia::HasAudioInputDevices() {
  return HasAudioDevice(true);
}

void AudioManagerFuchsia::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioInput)) {
    return;
  }

  GetAudioDevices(device_names, true);
}

void AudioManagerFuchsia::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    return;
  }

  GetAudioDevices(device_names, false);
}

AudioParameters AudioManagerFuchsia::GetInputStreamParameters(
    const std::string& device_id) {
  // TODO(crbug.com/42050621): Fuchsia currently doesn't provide an API to get
  // device configuration and supported effects. Update this method when that
  // functionality is implemented.
  //
  // Use 16kHz sample rate with 10ms buffer, which is consistent with
  // the default configuration used in the AudioCapturer implementation.
  const size_t kSampleRate = 16000;
  const size_t kPeriodSamples = AudioTimestampHelper::TimeToFrames(
      base::kAudioSchedulingPeriod, kSampleRate);
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Mono(), kSampleRate,
                         kPeriodSamples);

  // Some AudioCapturer implementations support echo cancellation, noise
  // suppression and automatic gain control, but currently there is no way to
  // detect it. For now the corresponding effect flags are set based on a
  // command line switch.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAudioCapturerWithEchoCancellation)) {
    params.set_effects(AudioParameters::ECHO_CANCELLER |
                       AudioParameters::NOISE_SUPPRESSION |
                       AudioParameters::AUTOMATIC_GAIN_CONTROL);
  }

  return params;
}

AudioParameters AudioManagerFuchsia::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  if (input_params.IsValid()) {
    AudioParameters params = input_params;

    base::TimeDelta period = AudioTimestampHelper::FramesToTime(
        input_params.frames_per_buffer(), input_params.sample_rate());

    // Round period to a whole number of the CPU scheduling periods.
    period = round(period / base::kAudioSchedulingPeriod) *
             base::kAudioSchedulingPeriod;
    period = std::min(kMaxBufferPeriod, std::max(period, kMinBufferPeriod));

    params.set_frames_per_buffer(
        AudioTimestampHelper::TimeToFrames(period, params.sample_rate()));

    return params;
  }

  // TODO(crbug.com/42050621): Fuchsia currently doesn't provide an API to get
  // device configuration. Update this method when that functionality is
  // implemented.
  const int kSampleRate = 48000;
  const int kMinPeriodFrames =
      AudioTimestampHelper::TimeToFrames(kMinBufferPeriod, kSampleRate);
  const int kMaxPeriodFrames =
      AudioTimestampHelper::TimeToFrames(kMaxBufferPeriod, kSampleRate);
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), kSampleRate,
                         kMinPeriodFrames,
                         AudioParameters::HardwareCapabilities(
                             kMinPeriodFrames, kMaxPeriodFrames));
}

const char* AudioManagerFuchsia::GetName() {
  return "Fuchsia";
}

AudioOutputStream* AudioManagerFuchsia::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
}

AudioOutputStream* AudioManagerFuchsia::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    // TODO(crbug.com/42050621): Fuchsia currently doesn't provide an API to
    // specify a device to use.
    LOG(ERROR) << "Specifying not default output device (" << device_id
               << ") is not implemented.";
    return nullptr;
  }

  return new AudioOutputStreamFuchsia(this, params);
}

AudioInputStream* AudioManagerFuchsia::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerFuchsia::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerFuchsia>(std::move(audio_thread),
                                               audio_log_factory);
}

AudioInputStream* AudioManagerFuchsia::MakeInputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId &&
      device_id != AudioDeviceDescription::kLoopbackInputDeviceId) {
    // TODO(crbug.com/42050621): Fuchsia currently doesn't provide an API to
    // specify a device to use.
    LOG(ERROR) << "Specifying not default input device (" << device_id
               << ") is not implemented.";
    return nullptr;
  }

  return new AudioInputStreamFuchsia(this, params, device_id);
}

void AudioManagerFuchsia::InitOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  enumerator_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "AudioDeviceEnumerator disconnected. Audio "
                             "devices will be disabled";
    audio_devices_.clear();
  });
  base::ComponentContextForProcess()->svc()->Connect(enumerator_.NewRequest());
  enumerator_.events().OnDeviceAdded =
      fit::bind_member(this, &AudioManagerFuchsia::OnDeviceAdded);
  enumerator_.events().OnDeviceRemoved =
      fit::bind_member(this, &AudioManagerFuchsia::OnDeviceRemoved);

  // Initialize the state synchronously so that tests get correct information.
  ::fuchsia::media::AudioDeviceEnumeratorSyncPtr sync_enumerator;
  base::ComponentContextForProcess()->svc()->Connect(
      sync_enumerator.NewRequest());
  std::vector<fuchsia::media::AudioDeviceInfo> devices;
  zx_status_t status = sync_enumerator->GetDevices(&devices);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status)
        << "Unable to retrieve audio devices from AudioDeviceEnumerator. Audio "
           "devices will be disabled";
    return;
  }
  for (auto& info : devices) {
    audio_devices_[info.token_id] = std::move(info);
  }
}

void AudioManagerFuchsia::OnDeviceAdded(
    ::fuchsia::media::AudioDeviceInfo device_info) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  audio_devices_[device_info.token_id] = std::move(device_info);
}

void AudioManagerFuchsia::OnDeviceRemoved(uint64_t device_token) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  audio_devices_.erase(device_token);
}

bool AudioManagerFuchsia::HasAudioDevice(bool is_input) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  return base::Contains(audio_devices_, is_input, [](const auto& device) {
    return device.second.is_input;
  });
}

void AudioManagerFuchsia::GetAudioDevices(AudioDeviceNames* device_names,
                                          bool is_input) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // TODO(crbug.com/42050621): Fuchsia currently doesn't provide an API to
  // specify a device to use. Until then only return the default device.
  device_names->clear();
  if (HasAudioDevice(is_input)) {
    *device_names = {AudioDeviceName::CreateDefault()};
  }
}

}  // namespace media
