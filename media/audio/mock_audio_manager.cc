// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/audio/mock_audio_debug_recording_manager.h"
#include "media/base/audio_parameters.h"

namespace media {

MockAudioManager::MockAudioManager(std::unique_ptr<AudioThread> audio_thread)
    : AudioManager(std::move(audio_thread)) {}

MockAudioManager::~MockAudioManager() = default;

void MockAudioManager::ShutdownOnAudioThread() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
}

bool MockAudioManager::HasAudioOutputDevices() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return has_output_devices_;
}

bool MockAudioManager::HasAudioInputDevices() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return has_input_devices_;
}

void MockAudioManager::GetAudioInputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!get_input_device_descriptions_cb_)
    return;
  get_input_device_descriptions_cb_.Run(device_descriptions);
}

void MockAudioManager::GetAudioOutputDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (!get_output_device_descriptions_cb_)
    return;
  get_output_device_descriptions_cb_.Run(device_descriptions);
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return MakeAudioOutputStreamProxy(params, device_id);
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStreamProxy(
    const media::AudioParameters& params,
    const std::string& device_id) {
  return make_output_stream_cb_ ? make_output_stream_cb_.Run(params, device_id)
                                : nullptr;
}

media::AudioInputStream* MockAudioManager::MakeAudioInputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return make_input_stream_cb_ ? make_input_stream_cb_.Run(params, device_id)
                               : nullptr;
}

void MockAudioManager::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

void MockAudioManager::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

AudioParameters MockAudioManager::GetOutputStreamParameters(
      const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return output_params_;
}

AudioParameters MockAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return input_params_;
}

std::string MockAudioManager::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return get_associated_output_device_id_cb_
             ? get_associated_output_device_id_cb_.Run(input_device_id)
             : std::string();
}

std::string MockAudioManager::GetDefaultInputDeviceID() {
  return std::string();
}
std::string MockAudioManager::GetDefaultOutputDeviceID() {
  return std::string();
}
std::string MockAudioManager::GetCommunicationsInputDeviceID() {
  return std::string();
}
std::string MockAudioManager::GetCommunicationsOutputDeviceID() {
  return std::string();
}

std::unique_ptr<AudioLog> MockAudioManager::CreateAudioLog(
    AudioLogFactory::AudioComponent component,
    int component_id) {
  return nullptr;
}

void MockAudioManager::InitializeDebugRecording() {
  if (!GetTaskRunner()->BelongsToCurrentThread()) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&MockAudioManager::InitializeDebugRecording,
                                  base::Unretained(this)));
    return;
  }

  DCHECK(!debug_recording_manager_);
  debug_recording_manager_ = std::make_unique<MockAudioDebugRecordingManager>();
}

AudioDebugRecordingManager* MockAudioManager::GetAudioDebugRecordingManager() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return debug_recording_manager_.get();
}

void MockAudioManager::SetAecDumpRecordingManager(
    base::WeakPtr<AecdumpRecordingManager>) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  // This is no-op by default.
}

const char* MockAudioManager::GetName() {
  return nullptr;
}

void MockAudioManager::SetMakeOutputStreamCB(MakeOutputStreamCallback cb) {
  make_output_stream_cb_ = std::move(cb);
}

void MockAudioManager::SetMakeInputStreamCB(MakeInputStreamCallback cb) {
  make_input_stream_cb_ = std::move(cb);
}

void MockAudioManager::SetInputStreamParameters(const AudioParameters& params) {
  input_params_ = params;
}

void MockAudioManager::SetOutputStreamParameters(
    const AudioParameters& params) {
  output_params_ = params;
}

void MockAudioManager::SetDefaultOutputStreamParameters(
    const AudioParameters& params) {
  default_output_params_ = params;
}

void MockAudioManager::SetHasInputDevices(bool has_input_devices) {
  has_input_devices_ = has_input_devices;
}

void MockAudioManager::SetHasOutputDevices(bool has_output_devices) {
  has_output_devices_ = has_output_devices;
}

void MockAudioManager::SetInputDeviceDescriptionsCallback(
    GetDeviceDescriptionsCallback callback) {
  get_input_device_descriptions_cb_ = std::move(callback);
}

void MockAudioManager::SetOutputDeviceDescriptionsCallback(
    GetDeviceDescriptionsCallback callback) {
  get_output_device_descriptions_cb_ = std::move(callback);
}

void MockAudioManager::SetAssociatedOutputDeviceIDCallback(
    GetAssociatedOutputDeviceIDCallback callback) {
  get_associated_output_device_id_cb_ = std::move(callback);
}

}  // namespace media.
