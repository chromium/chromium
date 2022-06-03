// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/audio/cras/cras_util.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"

namespace media {
namespace {

// Default sample rate for input and output streams.
const int kDefaultSampleRate = 48000;

// Default input buffer size.
const int kDefaultInputBufferSize = 1024;

// Default output buffer size.
const int kDefaultOutputBufferSize = 512;

}  // namespace

bool AudioManagerCras::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerCras::HasAudioInputDevices() {
  return !cras_util_->CrasGetAudioDevices(DeviceType::kInput).empty();
}

AudioManagerCras::AudioManagerCras(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerCrasBase(std::move(audio_thread), audio_log_factory),
      cras_util_(std::make_unique<CrasUtil>()),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

AudioManagerCras::~AudioManagerCras() = default;

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->push_back(AudioDeviceName::CreateDefault());
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kInput)) {
    device_names->emplace_back(device.name, base::NumberToString(device.id));
  }
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->push_back(AudioDeviceName::CreateDefault());
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    device_names->emplace_back(device.name, base::NumberToString(device.id));
  }
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  int buffer_size =
      user_buffer_size ? user_buffer_size : kDefaultInputBufferSize;

  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));

  if (cras_util_->CrasHasKeyboardMic())
    params.set_effects(AudioParameters::KEYBOARD_MIC);

  // Allow experimentation with system echo cancellation with all devices,
  // but enable it by default on devices that actually support it.
  params.set_effects(params.effects() |
                     AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);
  if (base::FeatureList::IsEnabled(features::kCrOSSystemAEC)) {
    if (cras_util_->CrasGetAecSupported()) {
      const int32_t aec_group_id = cras_util_->CrasGetAecGroupId();

      // Check if the system AEC has a group ID which is flagged to be
      // deactivated by the field trial.
      const bool system_aec_deactivated =
          base::GetFieldTrialParamByFeatureAsBool(
              features::kCrOSSystemAECDeactivatedGroups,
              base::NumberToString(aec_group_id), false);

      if (!system_aec_deactivated) {
        params.set_effects(params.effects() | AudioParameters::ECHO_CANCELLER);
      }
    }
  }

  return params;
}

std::string AudioManagerCras::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveInputNode());
}

std::string AudioManagerCras::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveOutputNode());
}

std::string AudioManagerCras::GetGroupIDInput(const std::string& device_id) {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kInput)) {
    if (base::NumberToString(device.id) == device_id ||
        (AudioDeviceDescription::IsDefaultDevice(device_id) && device.active)) {
      return device.dev_name;
    }
  }
  return "";
}

std::string AudioManagerCras::GetGroupIDOutput(const std::string& device_id) {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (base::NumberToString(device.id) == device_id ||
        (AudioDeviceDescription::IsDefaultDevice(device_id) && device.active)) {
      return device.dev_name;
    }
  }
  return "";
}

std::string AudioManagerCras::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  if (AudioDeviceDescription::IsDefaultDevice(input_device_id)) {
    // Note: the default input should not be associated to any output, as this
    // may lead to accidental uses of a pinned stream.
    return "";
  }

  std::string device_name = GetGroupIDInput(input_device_id);

  if (device_name.empty())
    return "";

  // Now search for an output device with the same device name.
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.dev_name == device_name)
      return base::NumberToString(device.id);
  }
  return "";
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = GetUserBufferSize();
  if (input_params.IsValid()) {
    channel_layout = input_params.channel_layout();
    sample_rate = input_params.sample_rate();
    if (!buffer_size)  // Not user-provided.
      buffer_size =
          std::min(static_cast<int>(limits::kMaxAudioBufferSize),
                   std::max(static_cast<int>(limits::kMinAudioBufferSize),
                            input_params.frames_per_buffer()));
    return AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
        buffer_size,
        AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                              limits::kMaxAudioBufferSize));
  }

  // Get max supported channels from |output_device_id| or the primary active
  // one if |output_device_id| is the default device.
  uint64_t preferred_device_id;
  if (AudioDeviceDescription::IsDefaultDevice(output_device_id)) {
    preferred_device_id = GetPrimaryActiveOutputNode();
  } else {
    if (!base::StringToUint64(output_device_id, &preferred_device_id))
      preferred_device_id = 0;  // 0 represents invalid |output_device_id|.
  }

  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.id == preferred_device_id) {
      channel_layout =
          GuessChannelLayout(static_cast<int>(device.max_supported_channels));
      // Fall-back to old fashion: always fixed to STEREO layout.
      if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
        channel_layout = CHANNEL_LAYOUT_STEREO;
      }
      break;
    }
  }

  if (!buffer_size)  // Not user-provided.
    buffer_size = cras_util_->CrasGetDefaultOutputBufferSize();

  if (buffer_size <= 0)
    buffer_size = kDefaultOutputBufferSize;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
      buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
}

uint64_t AudioManagerCras::GetPrimaryActiveInputNode() {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kInput)) {
    if (device.active)
      return device.id;
  }
  return 0;
}

uint64_t AudioManagerCras::GetPrimaryActiveOutputNode() {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.active)
      return device.id;
  }
  return 0;
}

bool AudioManagerCras::IsDefault(const std::string& device_id, bool is_input) {
  return AudioDeviceDescription::IsDefaultDevice(device_id);
}

enum CRAS_CLIENT_TYPE AudioManagerCras::GetClientType() {
  return CRAS_CLIENT_TYPE_LACROS;
}

}  // namespace media
