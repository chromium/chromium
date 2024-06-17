// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"
#include "media/base/limits.h"

namespace media {

namespace {

std::optional<AudioParameters> TryToFixChannels(const AudioParameters& params) {
  DCHECK(!params.IsValid());
  AudioParameters params_copy(params);

  // If the number of output channels is greater than the maximum, use the
  // maximum allowed value. Hardware channels are ignored upstream, so it is
  // better to report a valid value if this is the only problem.
  if (params.channels() > limits::kMaxChannels) {
    DCHECK(params.channel_layout() == CHANNEL_LAYOUT_DISCRETE);
    params_copy.SetChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE,
                                       limits::kMaxChannels);
  }

  return params_copy.IsValid() ? params_copy : std::optional<AudioParameters>();
}

}  // namespace

AudioSystemHelper::AudioSystemHelper(AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK(audio_manager_);
}

AudioSystemHelper::~AudioSystemHelper() = default;

void AudioSystemHelper::GetInputStreamParameters(
    const std::string& device_id,
    AudioSystem::OnAudioParamsCallback on_params_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  std::move(on_params_cb).Run(ComputeInputParameters(device_id));
}

void AudioSystemHelper::GetOutputStreamParameters(
    const std::string& device_id,
    AudioSystem::OnAudioParamsCallback on_params_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  std::move(on_params_cb).Run(ComputeOutputParameters(device_id));
}

void AudioSystemHelper::HasInputDevices(
    AudioSystem::OnBoolCallback on_has_devices_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  std::move(on_has_devices_cb).Run(audio_manager_->HasAudioInputDevices());
}

void AudioSystemHelper::HasOutputDevices(
    AudioSystem::OnBoolCallback on_has_devices_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  std::move(on_has_devices_cb).Run(audio_manager_->HasAudioOutputDevices());
}

void AudioSystemHelper::GetDeviceDescriptions(
    bool for_input,
    AudioSystem::OnDeviceDescriptionsCallback on_descriptions_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  AudioDeviceDescriptions descriptions;
  if (for_input)
    audio_manager_->GetAudioInputDeviceDescriptions(&descriptions);
  else
    audio_manager_->GetAudioOutputDeviceDescriptions(&descriptions);
  std::move(on_descriptions_cb).Run(std::move(descriptions));
}

void AudioSystemHelper::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    AudioSystem::OnDeviceIdCallback on_device_id_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  const std::string associated_output_device_id =
      audio_manager_->GetAssociatedOutputDeviceID(input_device_id);
  std::move(on_device_id_cb)
      .Run(associated_output_device_id.empty() ? std::optional<std::string>()
                                               : associated_output_device_id);
}

void AudioSystemHelper::GetInputDeviceInfo(
    const std::string& input_device_id,
    AudioSystem::OnInputDeviceInfoCallback on_input_device_info_cb) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  const std::string associated_output_device_id =
      audio_manager_->GetAssociatedOutputDeviceID(input_device_id);
  std::move(on_input_device_info_cb)
      .Run(ComputeInputParameters(input_device_id),
           associated_output_device_id.empty() ? std::optional<std::string>()
                                               : associated_output_device_id);
}

std::optional<AudioParameters> AudioSystemHelper::ComputeInputParameters(
    const std::string& device_id) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  // TODO(olka): remove this when AudioManager::GetInputStreamParameters()
  // returns invalid parameters if the device is not found.
  if (AudioDeviceDescription::IsLoopbackDevice(device_id)) {
    // For system audio capture, we need an output device (namely speaker)
    // instead of an input device (namely microphone) to work.
    // AudioManager::GetInputStreamParameters will check |device_id| and
    // query the correct device for audio parameters by itself.
    if (!audio_manager_->HasAudioOutputDevices())
      return std::optional<AudioParameters>();
  } else {
    if (!audio_manager_->HasAudioInputDevices())
      return std::optional<AudioParameters>();
  }

  AudioParameters params = audio_manager_->GetInputStreamParameters(device_id);
  return params.IsValid() ? params : TryToFixChannels(params);
}

std::optional<AudioParameters> AudioSystemHelper::ComputeOutputParameters(
    const std::string& device_id) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  // TODO(olka): remove this when
  // AudioManager::GetOutputStreamParameters() returns invalid
  // parameters if the device is not found.
  if (!audio_manager_->HasAudioOutputDevices())
    return std::optional<AudioParameters>();

  std::string effective_device_id =
      AudioDeviceDescription::IsDefaultDevice(device_id)
          ? audio_manager_->GetDefaultOutputDeviceID()
          : device_id;

  AudioParameters params =
      audio_manager_->GetOutputStreamParameters(effective_device_id);

  if (params.IsValid())
    return params;

  return TryToFixChannels(params);
}

}  // namespace media
