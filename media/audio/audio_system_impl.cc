// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/bind_to_current_loop.h"

// Using base::Unretained for |audio_manager_| is safe since AudioManager is
// deleted after audio thread is stopped.

// No need to bind the callback to the current loop if we are on the audio
// thread. However, the client still expects to receive the reply
// asynchronously, so we always post the helper function, which will
// syncronously call the (bound to current loop or not) callback. Thus the
// client always receives the callback on the thread it accesses AudioSystem on.

namespace media {

namespace {

void GetInputStreamParametersOnAudioThread(
    AudioManager* audio_manager,
    const std::string& device_id,
    AudioSystem::OnAudioParamsCallback on_params_cb) {
  AudioSystemHelper(audio_manager)
      .GetInputStreamParameters(device_id, std::move(on_params_cb));
}

void GetOutputStreamParametersOnAudioThread(
    AudioManager* audio_manager,
    const std::string& device_id,
    AudioSystem::OnAudioParamsCallback on_params_cb) {
  AudioSystemHelper(audio_manager)
      .GetOutputStreamParameters(device_id, std::move(on_params_cb));
}

void HasInputDevicesOnAudioThread(
    AudioManager* audio_manager,
    AudioSystem::OnBoolCallback on_has_devices_cb) {
  AudioSystemHelper(audio_manager)
      .HasInputDevices(std::move(on_has_devices_cb));
}

void HasOutputDevicesOnAudioThread(
    AudioManager* audio_manager,
    AudioSystem::OnBoolCallback on_has_devices_cb) {
  AudioSystemHelper(audio_manager)
      .HasOutputDevices(std::move(on_has_devices_cb));
}

void GetDeviceDescriptionsOnAudioThread(
    AudioManager* audio_manager,
    bool for_input,
    AudioSystem::OnDeviceDescriptionsCallback on_descriptions_cb) {
  AudioSystemHelper(audio_manager)
      .GetDeviceDescriptions(for_input, std::move(on_descriptions_cb));
}

void GetAssociatedOutputDeviceIDOnAudioThread(
    AudioManager* audio_manager,
    const std::string& input_device_id,
    AudioSystem::OnDeviceIdCallback on_device_id_cb) {
  AudioSystemHelper(audio_manager)
      .GetAssociatedOutputDeviceID(input_device_id, std::move(on_device_id_cb));
}

void GetInputDeviceInfoOnAudioThread(
    AudioManager* audio_manager,
    const std::string& input_device_id,
    AudioSystem::OnInputDeviceInfoCallback on_input_device_info_cb) {
  AudioSystemHelper(audio_manager)
      .GetInputDeviceInfo(input_device_id, std::move(on_input_device_info_cb));
}

}  // namespace

template <typename... Args>
inline base::OnceCallback<void(Args...)>
AudioSystemImpl::MaybeBindToCurrentLoop(
    base::OnceCallback<void(Args...)> callback) {
  return audio_manager_->GetTaskRunner()->BelongsToCurrentThread()
             ? std::move(callback)
             : media::BindToCurrentLoop(std::move(callback));
}

// static
std::unique_ptr<AudioSystem> AudioSystemImpl::CreateInstance() {
  DCHECK(AudioManager::Get()) << "AudioManager instance is not created";
  return std::make_unique<AudioSystemImpl>(AudioManager::Get());
}

AudioSystemImpl::AudioSystemImpl(AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DETACH_FROM_THREAD(thread_checker_);
}

void AudioSystemImpl::GetInputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetInputStreamParametersOnAudioThread,
                     base::Unretained(audio_manager_), device_id,
                     MaybeBindToCurrentLoop(std::move(on_params_cb))));
}

void AudioSystemImpl::GetOutputStreamParameters(
    const std::string& device_id,
    OnAudioParamsCallback on_params_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetOutputStreamParametersOnAudioThread,
                     base::Unretained(audio_manager_), device_id,
                     MaybeBindToCurrentLoop(std::move(on_params_cb))));
}

void AudioSystemImpl::HasInputDevices(OnBoolCallback on_has_devices_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HasInputDevicesOnAudioThread,
                     base::Unretained(audio_manager_),
                     MaybeBindToCurrentLoop(std::move(on_has_devices_cb))));
}

void AudioSystemImpl::HasOutputDevices(OnBoolCallback on_has_devices_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HasOutputDevicesOnAudioThread,
                     base::Unretained(audio_manager_),
                     MaybeBindToCurrentLoop(std::move(on_has_devices_cb))));
}

void AudioSystemImpl::GetDeviceDescriptions(
    bool for_input,
    OnDeviceDescriptionsCallback on_descriptions_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetDeviceDescriptionsOnAudioThread,
                                base::Unretained(audio_manager_), for_input,
                                MaybeBindToCurrentLoop(
                                    WrapCallbackWithDeviceNameLocalization(
                                        std::move(on_descriptions_cb)))));
}

void AudioSystemImpl::GetAssociatedOutputDeviceID(
    const std::string& input_device_id,
    OnDeviceIdCallback on_device_id_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetAssociatedOutputDeviceIDOnAudioThread,
                     base::Unretained(audio_manager_), input_device_id,
                     MaybeBindToCurrentLoop(std::move(on_device_id_cb))));
}

void AudioSystemImpl::GetInputDeviceInfo(
    const std::string& input_device_id,
    OnInputDeviceInfoCallback on_input_device_info_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_manager_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetInputDeviceInfoOnAudioThread, base::Unretained(audio_manager_),
          input_device_id,
          MaybeBindToCurrentLoop(std::move(on_input_device_info_cb))));
}

}  // namespace media
