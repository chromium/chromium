// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/nix/xdg_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_features.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_unified.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/localized_strings.h"

namespace media {
namespace {

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 50;

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

bool HasKeyboardMic(const chromeos::AudioDeviceList& devices) {
  for (const auto& device : devices) {
    if (device.is_input && device.type == chromeos::AUDIO_TYPE_KEYBOARD_MIC) {
      return true;
    }
  }
  return false;
}

const chromeos::AudioDevice* GetDeviceFromId(
    const chromeos::AudioDeviceList& devices,
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
                              const chromeos::AudioDeviceList& device_list) {
  DCHECK_EQ(2U, device_list.size());
  if (device_list[0].type == chromeos::AUDIO_TYPE_LINEOUT ||
      device_list[1].type == chromeos::AUDIO_TYPE_LINEOUT) {
    device_names->emplace_back(kHeadphoneLineOutVirtualDevice,
                               base::NumberToString(device_list[0].id));
  } else if (device_list[0].type == chromeos::AUDIO_TYPE_INTERNAL_SPEAKER ||
             device_list[1].type == chromeos::AUDIO_TYPE_INTERNAL_SPEAKER) {
    device_names->emplace_back(kInternalOutputVirtualDevice,
                               base::NumberToString(device_list[0].id));
  } else {
    DCHECK(device_list[0].IsInternalMic() || device_list[1].IsInternalMic());
    device_names->emplace_back(kInternalInputVirtualDevice,
                               base::NumberToString(device_list[0].id));
  }
}

}  // namespace

bool AudioManagerCras::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerCras::HasAudioInputDevices() {
  chromeos::AudioDeviceList devices;
  GetAudioDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_input && devices[i].is_for_simple_usage())
      return true;
  }
  return false;
}

AudioManagerCras::AudioManagerCras(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      on_shutdown_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCras::~AudioManagerCras() = default;

void AudioManagerCras::GetAudioDeviceNamesImpl(bool is_input,
                                               AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());

  device_names->push_back(AudioDeviceName::CreateDefault());

  chromeos::AudioDeviceList devices;
  GetAudioDevices(&devices);

  // |dev_idx_map| is a map of dev_index and their audio devices.
  std::map<int, chromeos::AudioDeviceList> dev_idx_map;
  for (const auto& device : devices) {
    if (device.is_input != is_input || !device.is_for_simple_usage())
      continue;

    dev_idx_map[dev_index_of(device.id)].push_back(device);
  }

  for (const auto& item : dev_idx_map) {
    if (1 == item.second.size()) {
      const chromeos::AudioDevice& device = item.second.front();
      device_names->emplace_back(device.display_name,
                                 base::NumberToString(device.id));
    } else {
      // Create virtual device name for audio nodes that share the same device
      // index.
      ProcessVirtualDeviceName(device_names, item.second);
    }
  }
}

void AudioManagerCras::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(true, device_names);
}

void AudioManagerCras::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  GetAudioDeviceNamesImpl(false, device_names);
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  int user_buffer_size = GetUserBufferSize();
  int buffer_size = user_buffer_size ?
      user_buffer_size : kDefaultInputBufferSize;

  // TODO(hshi): Fine-tune audio parameters based on |device_id|. The optimal
  // parameters for the loopback stream may differ from the default.
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
  chromeos::AudioDeviceList devices;
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

std::string AudioManagerCras::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  chromeos::AudioDeviceList devices;
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
      devices.begin(), devices.end(),
      [device_name](const chromeos::AudioDevice& device) {
        return !device.is_input && device.device_name == device_name;
      });
  return output_device_it == devices.end()
             ? ""
             : base::NumberToString(output_device_it->id);
}

std::string AudioManagerCras::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveInputNode());
}

std::string AudioManagerCras::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return base::NumberToString(GetPrimaryActiveOutputNode());
}

std::string AudioManagerCras::GetGroupIDOutput(
    const std::string& output_device_id) {
  chromeos::AudioDeviceList devices;
  GetAudioDevices(&devices);

  return GetHardwareDeviceFromDeviceId(devices, false, output_device_id);
}

std::string AudioManagerCras::GetGroupIDInput(
    const std::string& input_device_id) {
  chromeos::AudioDeviceList devices;
  GetAudioDevices(&devices);

  return GetHardwareDeviceFromDeviceId(devices, true, input_device_id);
}

const char* AudioManagerCras::GetName() {
  return "CRAS";
}

bool AudioManagerCras::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  weak_ptr_factory_.InvalidateWeakPtrs();
  on_shutdown_.Signal();
  return AudioManager::Shutdown();
}

AudioOutputStream* AudioManagerCras::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  // Pinning stream is not supported for MakeLinearOutputStream.
  return MakeOutputStream(params, AudioDeviceDescription::kDefaultDeviceId);
}

AudioOutputStream* AudioManagerCras::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  // TODO(dgreid): Open the correct input device for unified IO.
  return MakeOutputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

int AudioManagerCras::GetDefaultOutputBufferSizePerBoard() {
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
            &AudioManagerCras::GetDefaultOutputBufferSizeOnMainThread,
            weak_this_, base::Unretained(&buffer_size),
            base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return static_cast<int>(buffer_size);
}

bool AudioManagerCras::GetSystemAecSupportedPerBoard() {
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
        base::BindOnce(&AudioManagerCras::GetSystemAecSupportedOnMainThread,
                       weak_this_, base::Unretained(&system_aec_supported),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return system_aec_supported;
}

int32_t AudioManagerCras::GetSystemAecGroupIdPerBoard() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  int32_t group_id = chromeos::CrasAudioHandler::kSystemAecGroupIdNotAvailable;
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
        base::BindOnce(&AudioManagerCras::GetSystemAecGroupIdOnMainThread,
                       weak_this_, base::Unretained(&group_id),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return group_id;
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = GetDefaultOutputBufferSizePerBoard();
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    channel_layout = input_params.channel_layout();
    buffer_size =
        std::min(static_cast<int>(limits::kMaxAudioBufferSize),
                 std::max(static_cast<int>(limits::kMinAudioBufferSize),
                          input_params.frames_per_buffer()));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, sample_rate,
      buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));

  return params;
}

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  return new CrasUnifiedStream(params, this, device_id);
}

AudioInputStream* AudioManagerCras::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this, device_id);
}

bool AudioManagerCras::IsDefault(const std::string& device_id, bool is_input) {
  AudioDeviceNames device_names;
  GetAudioDeviceNamesImpl(is_input, &device_names);
  DCHECK(!device_names.empty());
  const AudioDeviceName& device_name = device_names.front();
  return device_name.unique_id == device_id;
}

std::string AudioManagerCras::GetHardwareDeviceFromDeviceId(
    const chromeos::AudioDeviceList& devices,
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

  const chromeos::AudioDevice* device = GetDeviceFromId(devices, u64_device_id);

  return device ? device->device_name : "";
}

void AudioManagerCras::GetAudioDevices(chromeos::AudioDeviceList* devices) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    GetAudioDevicesOnMainThread(devices, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerCras::GetAudioDevicesOnMainThread,
                       weak_this_, base::Unretained(devices),
                       base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
}

void AudioManagerCras::GetAudioDevicesOnMainThread(
    chromeos::AudioDeviceList* devices,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  // CrasAudioHandler is shut down before AudioManagerCras.
  if (chromeos::CrasAudioHandler::Get())
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(devices);
  event->Signal();
}

uint64_t AudioManagerCras::GetPrimaryActiveInputNode() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  uint64_t device_id = 0;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  if (main_task_runner_->BelongsToCurrentThread()) {
    GetPrimaryActiveInputNodeOnMainThread(&device_id, &event);
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioManagerCras::GetPrimaryActiveInputNodeOnMainThread,
                       weak_this_, &device_id, &event));
  }
  WaitEventOrShutdown(&event);
  return device_id;
}

uint64_t AudioManagerCras::GetPrimaryActiveOutputNode() {
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
            &AudioManagerCras::GetPrimaryActiveOutputNodeOnMainThread,
            weak_this_, base::Unretained(&device_id),
            base::Unretained(&event)));
  }
  WaitEventOrShutdown(&event);
  return device_id;
}

void AudioManagerCras::GetPrimaryActiveInputNodeOnMainThread(
    uint64_t* active_input_node_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (chromeos::CrasAudioHandler::Get()) {
    *active_input_node_id =
        chromeos::CrasAudioHandler::Get()->GetPrimaryActiveInputNode();
  }
  event->Signal();
}

void AudioManagerCras::GetPrimaryActiveOutputNodeOnMainThread(
    uint64_t* active_output_node_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (chromeos::CrasAudioHandler::Get()) {
    *active_output_node_id =
        chromeos::CrasAudioHandler::Get()->GetPrimaryActiveOutputNode();
  }
  event->Signal();
}

void AudioManagerCras::GetDefaultOutputBufferSizeOnMainThread(
    int32_t* buffer_size,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (chromeos::CrasAudioHandler::Get())
    chromeos::CrasAudioHandler::Get()->GetDefaultOutputBufferSize(buffer_size);
  event->Signal();
}

void AudioManagerCras::GetSystemAecSupportedOnMainThread(
    bool* system_aec_supported,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (chromeos::CrasAudioHandler::Get()) {
    *system_aec_supported =
        chromeos::CrasAudioHandler::Get()->system_aec_supported();
  }
  event->Signal();
}

void AudioManagerCras::GetSystemAecGroupIdOnMainThread(
    int32_t* group_id,
    base::WaitableEvent* event) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (chromeos::CrasAudioHandler::Get())
    *group_id = chromeos::CrasAudioHandler::Get()->system_aec_group_id();
  event->Signal();
}

void AudioManagerCras::WaitEventOrShutdown(base::WaitableEvent* event) {
  base::WaitableEvent* waitables[] = {event, &on_shutdown_};
  base::WaitableEvent::WaitMany(waitables, base::size(waitables));
}

}  // namespace media
