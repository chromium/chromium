// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_chromeos.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "ash/components/audio/audio_device.h"
#include "ash/components/audio/cras_audio_handler.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"

namespace media {
namespace {

using ::ash::AudioDevice;
using ::ash::AudioDeviceList;
using ::ash::CrasAudioHandler;

// Default sample rate for input and output streams.
const int kDefaultSampleRate = 48000;

// Default input buffer size.
const int kDefaultInputBufferSize = 1024;

const char kInternalInputVirtualDevice[] = "Built-in mic";
const char kInternalOutputVirtualDevice[] = "Built-in speaker";
const char kHeadphoneLineOutVirtualDevice[] = "Headphone/Line Out";

// Used for the Media.CrosBeamformingDeviceState histogram, currently not used
// since beamforming is disabled.
enum CrosBeamformingDeviceState {
  BEAMFORMING_DEFAULT_ENABLED = 0,
  BEAMFORMING_USER_ENABLED,
  BEAMFORMING_DEFAULT_DISABLED,
  BEAMFORMING_USER_DISABLED,
  BEAMFORMING_STATE_MAX = BEAMFORMING_USER_DISABLED
};

bool HasKeyboardMic(const AudioDeviceList& devices) {
  for (const auto& device : devices) {
    if (device.is_input &&
        device.type == chromeos::AudioDeviceType::kKeyboardMic) {
      return true;
    }
  }
  return false;
}

const AudioDevice* GetDeviceFromId(const AudioDeviceList& devices,
                                   uint64_t id) {
  for (const auto& device : devices) {
    if (device.id == id) {
      return &device;
    }
  }
  return nullptr;
}

// Process |device_list| that two shares the same dev_index by creating a
// virtual device name for them.
void ProcessVirtualDeviceName(AudioDeviceNames* device_names,
                              const AudioDeviceList& device_list) {
  DCHECK_EQ(2U, device_list.size());
  if (device_list[0].type == chromeos::AudioDeviceType::kLineout ||
      device_list[1].type == chromeos::AudioDeviceType::kLineout) {
    device_names->emplace_back(kHeadphoneLineOutVirtualDevice,
                               base::NumberToString(device_list[0].id));
  } else if (device_list[0].type ==
                 chromeos::AudioDeviceType::kInternalSpeaker ||
             device_list[1].type ==
                 chromeos::AudioDeviceType::kInternalSpeaker) {
    device_names->emplace_back(kInternalOutputVirtualDevice,
                               base::NumberToString(device_list[0].id));
  } else {
    DCHECK(device_list[0].IsInternalMic() || device_list[1].IsInternalMic());
    device_names->emplace_back(kInternalInputVirtualDevice,
                               base::NumberToString(device_list[0].id));
  }
}

}  // namespace

bool AudioManagerChromeOS::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerChromeOS::HasAudioInputDevices() {
  AudioDeviceList devices;
  GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_input && devices[i].is_for_simple_usage())
      return true;
  }
  return false;
}

AudioManagerChromeOS::AudioManagerChromeOS(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerCrasBase(std::move(audio_thread), audio_log_factory),
      on_shutdown_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

AudioManagerChromeOS::~AudioManagerChromeOS() = default;

void AudioManagerChromeOS::GetAudioDeviceNamesImpl(
    bool is_input,
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  device_names->push_back(AudioDeviceName::CreateDefault());

  AudioDeviceList devices;
  GetAudioDevices(&devices);

  // |dev_idx_map| is a map of dev_index and their audio devices.
  std::map<int, AudioDeviceList> dev_idx_map;
  for (const auto& device : devices) {
    if (device.is_input != is_input || !device.is_for_simple_usage())
      continue;

    dev_idx_map[dev_index_of(device.id)].push_back(device);
  }

  for (const auto& item : dev_idx_map) {
    if (1 == item.second.size()) {
      const AudioDevice& device = item.second.front();
      device_names->emplace_back(device.display_name,
                                 base::NumberToString(device.id));
    } else {
      // Create virtual device name for audio nodes that share the same device
      // index.
      ProcessVirtualDeviceName(device_names, item.second);
    }
  }
}

void AudioManagerChromeOS::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerChromeOS::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
}

AudioParameters AudioManagerChromeOS::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  int buffer_size =
      user_buffer_size ? user_buffer_size : kDefaultInputBufferSize;

  // TODO(hshi): Fine-tune audio parameters based on |device_id|. The optimal
  // parameters for the loopback stream may differ from the default.
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
  AudioDeviceList devices;
  GetAudioDevices(&devices);
  if (HasKeyboardMic(devices))
    params.set_effects(AudioParameters::KEYBOARD_MIC);

  // Allow experimentation with system echo cancellation with all devices,
  // but enable it by default on devices that actually support it.
  params.set_effects(params.effects() |
                     AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);
  if (base::FeatureList::IsEnabled(features::kCrOSSystemAEC)) {
    if (GetSystemAecSupportedPerBoard()) {
      const int32_t aec_group_id = GetSystemAecGroupIdPerBoard();

      // Check if the system AEC has a group ID which is flagged to be
      // deactivated by the field trial.
      const bool system_aec_deactivated =
          base::GetFieldTrialParamByFeatureAsBool(
              features::kCrOSSystemAECDeactivatedGroups,
              std::to_string(aec_group_id), false);

      if (!system_aec_deactivated) {
        params.set_effects(params.effects() | AudioParameters::ECHO_CANCELLER);
      }
    }
  }

  return params;
}

std::string AudioManagerChromeOS::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  AudioDeviceList devices;
  GetAudioDevices(&devices);

  if (input_device_id == AudioDeviceDescription::kDefaultDeviceId) {
    // Note: the default input should not be associated to any output, as this
    // may lead to accidental uses of a pinned stream.
    return "";
  }

  const std::string device_name =
      GetHardwareDeviceFromDeviceId(devices, true, input_device_id);

  if (device_name.empty())
    return "";

  // Now search for an output device with the same device name.
  auto output_device_it = std::find_if(
      devices.begin(), devices.end(), [device_name](const AudioDevice& device) {
        return !device.is_input && device.device_name == device_name;
      });
  return output_device_it == devices.end()
             ? ""
             : base::NumberToString(output_device_it->id);
}

std::string AudioManagerChromeOS::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveInputNode());
}

std::string AudioManagerChromeOS::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveOutputNode());
}

std::string AudioManagerChromeOS::GetGroupIDOutput(
    const std::string& output_device_id) {
  AudioDeviceList devices;
  GetAudioDevices(&devices);

  return GetHardwareDeviceFromDeviceId(devices, false, output_device_id);
}

std::string AudioManagerChromeOS::GetGroupIDInput(
    const std::string& input_device_id) {
  AudioDeviceList devices;
  GetAudioDevices(&devices);

  return GetHardwareDeviceFromDeviceId(devices, true, input_device_id);
}

bool AudioManagerChromeOS::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_shutdown_.Signal();
  return AudioManager::Shutdown();
}

int AudioManagerChromeOS::GetDefaultOutputBufferSizePerBoard() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  int32_t buffer_size = 512;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    // Unittest may use the same thread for audio thread.
    GetDefaultOutputBufferSizeOnMainThread(&buffer_size, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioManagerChromeOS::GetDefaultOutputBufferSizeOnMainThread,
            weak_this_, base::Unretained(&buffer_size),
            base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return static_cast<int>(buffer_size);
}

bool AudioManagerChromeOS::GetSystemAecSupportedPerBoard() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool system_aec_supported = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    // Unittest may use the same thread for audio thread.
    GetSystemAecSupportedOnMainThread(&system_aec_supported, &event);
  } else {
    // Using base::Unretained is safe here because we wait for callback be
    // executed in main thread before local variables are destructed.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerChromeOS::GetSystemAecSupportedOnMainThread,
                       weak_this_, base::Unretained(&system_aec_supported),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return system_aec_supported;
}

int32_t AudioManagerChromeOS::GetSystemAecGroupIdPerBoard() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  int32_t group_id = CrasAudioHandler::kSystemAecGroupIdNotAvailable;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    // Unittest may use the same thread for audio thread.
    GetSystemAecGroupIdOnMainThread(&group_id, &event);
  } else {
    // Using base::Unretained is safe here because we wait for callback be
    // executed in main thread before local variables are destructed.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerChromeOS::GetSystemAecGroupIdOnMainThread,
                       weak_this_, base::Unretained(&group_id),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return group_id;
}

AudioParameters AudioManagerChromeOS::GetPreferredOutputStreamParameters(
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

  if (preferred_device_id) {
    AudioDeviceList devices;
    GetAudioDevices(&devices);
    const AudioDevice* device = GetDeviceFromId(devices, preferred_device_id);
    if (device && device->is_input == false) {
      channel_layout =
          GuessChannelLayout(static_cast<int>(device->max_supported_channels));
      // Fall-back to old fashion: always fixed to STEREO layout.
      if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
        channel_layout = CHANNEL_LAYOUT_STEREO;
      }
    }
  }

  if (!buffer_size)  // Not user-provided.
    buffer_size = GetDefaultOutputBufferSizePerBoard();

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
      buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
}

bool AudioManagerChromeOS::IsDefault(const std::string& device_id,
                                     bool is_input) {
  AudioDeviceNames device_names;
  GetAudioDeviceNamesImpl(is_input, &device_names);
  DCHECK(!device_names.empty());
  const AudioDeviceName& device_name = device_names.front();
  return device_name.unique_id == device_id;
}

std::string AudioManagerChromeOS::GetHardwareDeviceFromDeviceId(
    const AudioDeviceList& devices,
    bool is_input,
    const std::string& device_id) {
  uint64_t u64_device_id = 0;
  if (AudioDeviceDescription::IsDefaultDevice(device_id)) {
    u64_device_id =
        is_input ? GetPrimaryActiveInputNode() : GetPrimaryActiveOutputNode();
  } else {
    if (!base::StringToUint64(device_id, &u64_device_id))
      return "";
  }

  const AudioDevice* device = GetDeviceFromId(devices, u64_device_id);

  return device ? device->device_name : "";
}

void AudioManagerChromeOS::GetAudioDevices(AudioDeviceList* devices) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    GetAudioDevicesOnMainThread(devices, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerChromeOS::GetAudioDevicesOnMainThread,
                       weak_this_, base::Unretained(devices),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
}

void AudioManagerChromeOS::GetAudioDevicesOnMainThread(
    AudioDeviceList* devices,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // CrasAudioHandler is shut down before AudioManagerChromeOS.
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->GetAudioDevices(devices);
  event->Signal();
}

uint64_t AudioManagerChromeOS::GetPrimaryActiveInputNode() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  uint64_t device_id = 0;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    GetPrimaryActiveInputNodeOnMainThread(&device_id, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioManagerChromeOS::GetPrimaryActiveInputNodeOnMainThread,
            weak_this_, &device_id, &event));
  }
  WaitEventOrShutdown(&event);
  return device_id;
}

uint64_t AudioManagerChromeOS::GetPrimaryActiveOutputNode() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  uint64_t device_id = 0;
  if (main_task_runner_->BelongsToCurrentThread()) {
    // Unittest may use the same thread for audio thread.
    GetPrimaryActiveOutputNodeOnMainThread(&device_id, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AudioManagerChromeOS::GetPrimaryActiveOutputNodeOnMainThread,
            weak_this_, base::Unretained(&device_id),
            base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return device_id;
}

void AudioManagerChromeOS::GetPrimaryActiveInputNodeOnMainThread(
    uint64_t* active_input_node_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (CrasAudioHandler::Get()) {
    *active_input_node_id =
        CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  }
  event->Signal();
}

void AudioManagerChromeOS::GetPrimaryActiveOutputNodeOnMainThread(
    uint64_t* active_output_node_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (CrasAudioHandler::Get()) {
    *active_output_node_id =
        CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  }
  event->Signal();
}

void AudioManagerChromeOS::GetDefaultOutputBufferSizeOnMainThread(
    int32_t* buffer_size,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (CrasAudioHandler::Get())
    CrasAudioHandler::Get()->GetDefaultOutputBufferSize(buffer_size);
  event->Signal();
}

void AudioManagerChromeOS::GetSystemAecSupportedOnMainThread(
    bool* system_aec_supported,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (CrasAudioHandler::Get()) {
    *system_aec_supported = CrasAudioHandler::Get()->system_aec_supported();
  }
  event->Signal();
}

void AudioManagerChromeOS::GetSystemAecGroupIdOnMainThread(
    int32_t* group_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (CrasAudioHandler::Get())
    *group_id = CrasAudioHandler::Get()->system_aec_group_id();
  event->Signal();
}

void AudioManagerChromeOS::WaitEventOrShutdown(base::WaitableEvent* event) {
  base::WaitableEvent* waitables[] = {event, &on_shutdown_};
  base::WaitableEvent::WaitMany(waitables, base::size(waitables));
}

enum CRAS_CLIENT_TYPE AudioManagerChromeOS::GetClientType() {
  return CRAS_CLIENT_TYPE_CHROME;
}

}  // namespace media
