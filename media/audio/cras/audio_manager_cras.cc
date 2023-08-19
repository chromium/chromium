// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/audio/cras/cras_util.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"
#include "media/base/media_switches.h"

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
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

AudioManagerCras::~AudioManagerCras() = default;

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kInput)) {
    device_names->emplace_back(device.name, base::NumberToString(device.id));
  }
  if (!device_names->empty()) {
    device_names->push_front(AudioDeviceName::CreateDefault());
  }
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    device_names->emplace_back(device.name, base::NumberToString(device.id));
  }
  if (!device_names->empty()) {
    device_names->push_front(AudioDeviceName::CreateDefault());
  }
}

// Checks if a system AEC with a specific group ID is flagged to be deactivated
// by the field trial.
bool IsSystemAecDeactivated(int aec_group_id) {
  return base::GetFieldTrialParamByFeatureAsBool(
      media::kCrOSSystemAECDeactivatedGroups,
      base::NumberToString(aec_group_id), false);
}

// Checks if the board with `aec_group_id` is flagged by the field trial to not
// allow using DSP-based AEC effect.
bool IsDspBasedAecDeactivated(int aec_group_id) {
  return base::GetFieldTrialParamByFeatureAsBool(
             media::kCrOSDspBasedAecDeactivatedGroups,
             base::NumberToString(aec_group_id), false) ||
         !base::FeatureList::IsEnabled(media::kCrOSDspBasedAecAllowed);
}

// Checks if the board with `aec_group_id` is flagged by the field trial to not
// allow using DSP-based NS effect.
bool IsDspBasedNsDeactivated(int aec_group_id) {
  return base::GetFieldTrialParamByFeatureAsBool(
             media::kCrOSDspBasedNsDeactivatedGroups,
             base::NumberToString(aec_group_id), false) ||
         !base::FeatureList::IsEnabled(media::kCrOSDspBasedNsAllowed);
}

// Checks if the board with `aec_group_id` is flagged by the field trial to not
// allow using DSP-based AGC effect.
bool IsDspBasedAgcDeactivated(int aec_group_id) {
  return base::GetFieldTrialParamByFeatureAsBool(
             media::kCrOSDspBasedAgcDeactivatedGroups,
             base::NumberToString(aec_group_id), false) ||
         !base::FeatureList::IsEnabled(media::kCrOSDspBasedAgcAllowed);
}

// Specifies which DSP-based effects are allowed based on media constraints and
// any finch field trials.
void SetAllowedDspBasedEffects(int aec_group_id, AudioParameters& params) {
  int effects = params.effects();

  // Allow AEC to be applied by CRAS on DSP if the AEC is active in CRAS and if
  // using the AEC on DSP has not been deactivated by any field trials.
  if ((effects & AudioParameters::ECHO_CANCELLER) &&
      !IsDspBasedAecDeactivated(aec_group_id)) {
    effects = effects | AudioParameters::ALLOW_DSP_ECHO_CANCELLER;
  } else {
    effects = effects & ~AudioParameters::ALLOW_DSP_ECHO_CANCELLER;
  }

  // Allow NS to be applied by CRAS on DSP if the NS is active in CRAS and if
  // using the NS on DSP has not been deactivated by any field trials.
  if ((effects & AudioParameters::NOISE_SUPPRESSION) &&
      !IsDspBasedNsDeactivated(aec_group_id)) {
    effects = effects | AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION;
  } else {
    effects = effects & ~AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION;
  }

  // Allow AGC to be applied by CRAS on DSP if the AGC is active in CRAS and if
  // using the AGC on DSP has not been deactivated by any field trials.
  if ((effects & AudioParameters::AUTOMATIC_GAIN_CONTROL) &&
      !IsDspBasedAgcDeactivated(aec_group_id)) {
    effects = effects | AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL;
  } else {
    effects = effects & ~AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL;
  }

  params.set_effects(effects);
}

// Collects flags values for whether, and in what way, the AEC, NS or AGC
// effects should be enforced in spite of them not being flagged as supported by
// the board.
void RetrieveSystemEffectFeatures(bool& enforce_system_aec,
                                  bool& enforce_system_ns,
                                  bool& enforce_system_agc,
                                  bool& tuned_system_aec_allowed) {
  const bool enforce_system_aec_ns_agc_feature =
      base::FeatureList::IsEnabled(media::kCrOSEnforceSystemAecNsAgc);
  const bool enforce_system_aec_ns_feature =
      base::FeatureList::IsEnabled(media::kCrOSEnforceSystemAecNs);
  const bool enforce_system_aec_agc_feature =
      base::FeatureList::IsEnabled(media::kCrOSEnforceSystemAecAgc);
  const bool enforce_system_aec_feature =
      base::FeatureList::IsEnabled(media::kCrOSEnforceSystemAec);
  const bool enforce_system_aec_by_policy =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSystemAecEnabled);

  enforce_system_aec =
      enforce_system_aec_feature || enforce_system_aec_ns_agc_feature ||
      enforce_system_aec_ns_feature || enforce_system_aec_agc_feature ||
      enforce_system_aec_by_policy;
  enforce_system_ns =
      enforce_system_aec_ns_agc_feature || enforce_system_aec_ns_feature;
  enforce_system_agc =
      enforce_system_aec_ns_agc_feature || enforce_system_aec_agc_feature;

  tuned_system_aec_allowed =
      base::FeatureList::IsEnabled(media::kCrOSSystemAEC);
}

AudioParameters AudioManagerCras::GetStreamParametersForSystem(
    int user_buffer_size) {
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, ChannelLayoutConfig::Stereo(),
      kDefaultSampleRate, user_buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));

  bool enforce_system_aec;
  bool enforce_system_ns;
  bool enforce_system_agc;
  bool tuned_system_aec_allowed;
  RetrieveSystemEffectFeatures(enforce_system_aec, enforce_system_ns,
                               enforce_system_agc, tuned_system_aec_allowed);

  // Activation of the system AEC. Allow experimentation with system AEC with
  // all devices, but enable it by default on devices that actually support it.
  params.set_effects(params.effects() |
                     AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);

  // Rephrase the field aec_supported to properly reflect its meaning in this
  // context (since it currently signals whether an CrAS APM with tuned settings
  // is available).
  const bool tuned_system_apm_available = cras_util_->CrasGetAecSupported();

  // Don't use the system AEC if it is deactivated for this group ID. Also never
  // activate NS nor AGC for this board if the AEC is not activated, since this
  // will cause issues for the Browser AEC.
  bool use_system_aec =
      (tuned_system_apm_available && tuned_system_aec_allowed) ||
      enforce_system_aec;

  bool system_ns_supported = cras_util_->CrasGetNsSupported();
  bool system_agc_supported = cras_util_->CrasGetAgcSupported();

  int aec_group_id = cras_util_->CrasGetAecGroupId();
  if (!use_system_aec || IsSystemAecDeactivated(aec_group_id)) {
    SetAllowedDspBasedEffects(aec_group_id, params);
    return params;
  }

  // Activation of the system AEC.
  params.set_effects(params.effects() | AudioParameters::ECHO_CANCELLER);

  // Don't use system NS or AGC if the AEC has board-specific tunings.
  if (!tuned_system_apm_available) {
    // Activation of the system NS.
    if (system_ns_supported || enforce_system_ns) {
      params.set_effects(params.effects() | AudioParameters::NOISE_SUPPRESSION);
    }

    // Activation of the system AGC.
    if (system_agc_supported || enforce_system_agc) {
      params.set_effects(params.effects() |
                         AudioParameters::AUTOMATIC_GAIN_CONTROL);
    }
  }

  if (base::FeatureList::IsEnabled(media::kCrOSSystemVoiceIsolationOption)) {
    if (cras_util_->CrasGetVoiceIsolationSupported()) {
      params.set_effects(params.effects() |
                         AudioParameters::VOICE_ISOLATION_SUPPORTED);
    }
  }

  SetAllowedDspBasedEffects(aec_group_id, params);
  return params;
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  user_buffer_size =
      user_buffer_size ? user_buffer_size : kDefaultInputBufferSize;

  return GetStreamParametersForSystem(user_buffer_size);
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

  if (device_name.empty()) {
    return "";
  }

  // Now search for an output device with the same device name.
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.dev_name == device_name) {
      return base::NumberToString(device.id);
    }
  }
  return "";
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = kDefaultSampleRate;
  int buffer_size = GetUserBufferSize();
  if (input_params.IsValid()) {
    channel_layout_config = input_params.channel_layout_config();
    sample_rate = input_params.sample_rate();
    if (!buffer_size) {  // Not user-provided.
      buffer_size =
          std::min(static_cast<int>(limits::kMaxAudioBufferSize),
                   std::max(static_cast<int>(limits::kMinAudioBufferSize),
                            input_params.frames_per_buffer()));
    }
    return AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
        sample_rate, buffer_size,
        AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                              limits::kMaxAudioBufferSize));
  }

  // Get max supported channels from |output_device_id| or the primary active
  // one if |output_device_id| is the default device.
  uint64_t preferred_device_id;
  if (AudioDeviceDescription::IsDefaultDevice(output_device_id)) {
    preferred_device_id = GetPrimaryActiveOutputNode();
  } else {
    if (!base::StringToUint64(output_device_id, &preferred_device_id)) {
      preferred_device_id = 0;  // 0 represents invalid |output_device_id|.
    }
  }

  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.id == preferred_device_id) {
      channel_layout_config = ChannelLayoutConfig::Guess(
          static_cast<int>(device.max_supported_channels));
      // Fall-back to old fashion: always fixed to STEREO layout.
      if (channel_layout_config.channel_layout() ==
          CHANNEL_LAYOUT_UNSUPPORTED) {
        channel_layout_config = ChannelLayoutConfig::Stereo();
      }
      break;
    }
  }

  if (!buffer_size) {  // Not user-provided.
    buffer_size = cras_util_->CrasGetDefaultOutputBufferSize();
  }

  if (buffer_size <= 0) {
    buffer_size = kDefaultOutputBufferSize;
  }

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
}

uint64_t AudioManagerCras::GetPrimaryActiveInputNode() {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kInput)) {
    if (device.active) {
      return device.id;
    }
  }
  return 0;
}

uint64_t AudioManagerCras::GetPrimaryActiveOutputNode() {
  for (const auto& device :
       cras_util_->CrasGetAudioDevices(DeviceType::kOutput)) {
    if (device.active) {
      return device.id;
    }
  }
  return 0;
}

bool AudioManagerCras::IsDefault(const std::string& device_id, bool is_input) {
  return AudioDeviceDescription::IsDefaultDevice(device_id);
}

enum CRAS_CLIENT_TYPE AudioManagerCras::GetClientType() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return CRAS_CLIENT_TYPE_CHROME;
#else
  return CRAS_CLIENT_TYPE_LACROS;
#endif
}

}  // namespace media
