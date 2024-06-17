// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_info_accessor_for_tests.h"

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"

namespace media {

AudioDeviceInfoAccessorForTests::AudioDeviceInfoAccessorForTests(
    AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK(audio_manager_);
}

bool AudioDeviceInfoAccessorForTests::HasAudioOutputDevices() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->HasAudioOutputDevices();
}

bool AudioDeviceInfoAccessorForTests::HasAudioInputDevices() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->HasAudioInputDevices();
}

void AudioDeviceInfoAccessorForTests::GetAudioInputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager_->GetAudioInputDeviceDescriptions(device_descriptions);
}

void AudioDeviceInfoAccessorForTests::GetAudioOutputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager_->GetAudioOutputDeviceDescriptions(device_descriptions);
}

AudioParameters AudioDeviceInfoAccessorForTests::GetOutputStreamParameters(
    const std::string& device_id) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetOutputStreamParameters(device_id);
}

AudioParameters AudioDeviceInfoAccessorForTests::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetInputStreamParameters(device_id);
}

std::string AudioDeviceInfoAccessorForTests::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetAssociatedOutputDeviceID(input_device_id);
}

std::string AudioDeviceInfoAccessorForTests::GetDefaultInputDeviceID() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetDefaultInputDeviceID();
}

std::string AudioDeviceInfoAccessorForTests::GetDefaultOutputDeviceID() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetDefaultOutputDeviceID();
}

std::string AudioDeviceInfoAccessorForTests::GetCommunicationsInputDeviceID() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetCommunicationsInputDeviceID();
}

std::string AudioDeviceInfoAccessorForTests::GetCommunicationsOutputDeviceID() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_->GetCommunicationsOutputDeviceID();
}

}  // namespace media
