// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_device_mixer_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_latency.h"
#include "services/audio/device_listener_output_stream.h"
#include "services/audio/output_device_mixer.h"

namespace media {

// Helper class to get access to the protected AudioManager API.
class AudioManagerPowerUser {
 public:
  explicit AudioManagerPowerUser(AudioManager* audio_manager)
      : audio_manager_(audio_manager) {}
  std::string GetDefaultOutputDeviceID() {
    return audio_manager_->GetDefaultOutputDeviceID();
  }
  AudioParameters GetOutputStreamParameters(const std::string& device_id) {
    return audio_manager_->GetOutputStreamParameters(device_id);
  }

 private:
  AudioManager* const audio_manager_;
};

}  // namespace media

namespace audio {

namespace {

// Helper function to make sure the default device ID is consistent.
std::string UniformizeDefaultDeviceId(const std::string& device_id) {
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id))
    return "";

  return device_id;
}

}  // namespace

OutputDeviceMixerManager::OutputDeviceMixerManager(
    media::AudioManager* audio_manager,
    OutputDeviceMixer::CreateCallback create_mixer_callback)
    : audio_manager_(audio_manager),
      create_mixer_callback_(std::move(create_mixer_callback)) {}

OutputDeviceMixerManager::~OutputDeviceMixerManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

media::AudioOutputStream* OutputDeviceMixerManager::MakeOutputStream(
    const std::string& device_id,
    const media::AudioParameters& params,
    base::OnceClosure close_stream_on_device_change) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (params.format() != media::AudioParameters::AUDIO_PCM_LOW_LATENCY) {
    DLOG(WARNING) << "Making unmixable output stream";

    return CreateDeviceListenerStream(std::move(close_stream_on_device_change),
                                      device_id, params);
  }

  auto physical_device_id = ConvertToPhysicalDeviceId(device_id);

  OutputDeviceMixer* mixer = FindMixer(physical_device_id);

  if (!mixer)
    mixer = AddMixer(physical_device_id);

  // Add mixer can still fail.
  if (!mixer)
    return nullptr;

  return mixer->MakeMixableStream(params,
                                  std::move(close_stream_on_device_change));
}

void OutputDeviceMixerManager::OnDeviceChange() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio"),
               "OutputDeviceMixerManager::OnDeviceChange");
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  current_default_device_id_ = absl::nullopt;

  OutputDeviceMixers old_mixers;
  output_device_mixers_.swap(old_mixers);

  // Do not call StopListening(), as |old_mixers| are being destroyed anyways.
  for (auto&& mixer : old_mixers)
    mixer->ProcessDeviceChange();
}

void OutputDeviceMixerManager::StartListening(
    ReferenceOutput::Listener* listener,
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::string device_id = UniformizeDefaultDeviceId(output_device_id);

  ListenerSet& listeners = device_id_to_listeners_[device_id];

  bool insert_succeeded = listeners.insert(listener).second;
  DCHECK(insert_succeeded);  // |listener| shouldn't already be in the set.

  OutputDeviceMixer* mixer = FindMixer(ConvertToPhysicalDeviceId(device_id));

  if (!mixer)
    return;

  mixer->StartListening(listener);
}

void OutputDeviceMixerManager::StopListening(
    ReferenceOutput::Listener* listener,
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::string device_id = UniformizeDefaultDeviceId(output_device_id);

  ListenerSet& listeners = device_id_to_listeners_.at(device_id);

  auto listener_it = listeners.find(listener);
  DCHECK(listener_it != listeners.end());

  listeners.erase(listener_it);

  // If |listener| is the last listener, remove the set.
  if (listeners.empty())
    device_id_to_listeners_.erase(device_id);

  OutputDeviceMixer* mixer = FindMixer(ConvertToPhysicalDeviceId(device_id));

  // The mixer was never created, because there was no playback to that device
  // (possibly after a device device change). Listening never started, so there
  // is nothing to stop.
  if (!mixer)
    return;

  mixer->StopListening(listener);
}

const std::string& OutputDeviceMixerManager::ConvertToPhysicalDeviceId(
    const std::string& device_id) {
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id))
    return GetCurrentDefaultDeviceId();

  return device_id;
}

const std::string& OutputDeviceMixerManager::GetCurrentDefaultDeviceId() {
  if (!current_default_device_id_.has_value()) {
    current_default_device_id_ =
        media::AudioManagerPowerUser(audio_manager_).GetDefaultOutputDeviceID();
  }

  return current_default_device_id_.value();
}

OutputDeviceMixer* OutputDeviceMixerManager::FindMixer(
    const std::string& physical_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!media::AudioDeviceDescription::IsDefaultDevice(physical_device_id));

  for (const auto& mixer : output_device_mixers_) {
    if (mixer->device_id() == physical_device_id)
      return mixer.get();
  }

  return nullptr;
}

OutputDeviceMixer* OutputDeviceMixerManager::AddMixer(
    const std::string& physical_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!media::AudioDeviceDescription::IsDefaultDevice(physical_device_id));

  DCHECK(!FindMixer(physical_device_id));

  media::AudioParameters output_params =
      media::AudioManagerPowerUser(audio_manager_)
          .GetOutputStreamParameters(physical_device_id);

  if (!output_params.IsValid()) {
    LOG(ERROR) << "Adding OutputDeviceMixer failed: invalid output parameters";
    return nullptr;
  }

  output_params.set_frames_per_buffer(media::AudioLatency::GetRtcBufferSize(
      output_params.sample_rate(), /*hardware_buffer_size=*/0));

  // base::Unretained(this) is safe here, because |output_device_mixers_|
  // are owned by |this|.
  std::unique_ptr<OutputDeviceMixer> output_device_mixer =
      create_mixer_callback_.Run(
          physical_device_id, output_params,
          base::BindRepeating(
              &OutputDeviceMixerManager::CreateDeviceListenerStream,
              base::Unretained(this),
              base::BindRepeating(&OutputDeviceMixerManager::OnDeviceChange,
                                  base::Unretained(this))),
          audio_manager_->GetTaskRunner());

  // The |physical_device_id| might no longer be valid, e.g. if a device was
  // unplugged.
  if (!output_device_mixer) {
    LOG(ERROR) << "Adding OutputDeviceMixer failed: creation error";
    return nullptr;
  }

  auto* mixer = output_device_mixer.get();
  output_device_mixers_.push_back(std::move(output_device_mixer));

  AttachListenersById(physical_device_id, mixer);

  // If we just created a mixer for the current default device ID, also attach
  // the "default device" listeners.
  if (physical_device_id == GetCurrentDefaultDeviceId())
    AttachListenersById(UniformizeDefaultDeviceId(""), mixer);

  return mixer;
}

void OutputDeviceMixerManager::AttachListenersById(const std::string& device_id,
                                                   OutputDeviceMixer* mixer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(mixer);

  auto listeners_it = device_id_to_listeners_.find(device_id);

  // Nothing to attach.
  if (listeners_it == device_id_to_listeners_.end())
    return;

  for (auto&& listener : listeners_it->second)
    mixer->StartListening(listener);
}

media::AudioOutputStream* OutputDeviceMixerManager::CreateDeviceListenerStream(
    base::OnceClosure on_device_change_callback,
    const std::string& device_id,
    const media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  media::AudioOutputStream* stream =
      audio_manager_->MakeAudioOutputStreamProxy(params, device_id);
  if (!stream) {
    LOG(ERROR) << "Stream proxy limit reached";
    return nullptr;
  }

  // If we are creating this stream through a OutputDeviceMixer,
  // |on_device_change_callback| will call OnDeviceChange(), which notifies all
  // mixers of a device change, and closes the streams.
  //
  // If we are directly creating this stream, |on_device_change_callback| will
  // synchronously close the returned stream.
  return new DeviceListenerOutputStream(audio_manager_, stream,
                                        std::move(on_device_change_callback));
}

}  // namespace audio
