// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/debug_recording.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/audio/audio_manager.h"

namespace audio {

DebugRecording::DebugRecording(
    mojo::PendingReceiver<mojom::DebugRecording> receiver,
    media::AudioManager* audio_manager,
    TracedServiceRef service_ref)
    : audio_manager_(audio_manager),
      receiver_(this, std::move(receiver)),
      service_ref_(std::move(service_ref)) {
  DCHECK(audio_manager_ != nullptr);
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  // On connection error debug recording is disabled, but the object is not
  // destroyed. It will be cleaned-up by service either on next bind request
  // or when service is shut down.
  receiver_.set_disconnect_handler(
      base::BindOnce(&DebugRecording::Disable, base::Unretained(this)));
}

DebugRecording::~DebugRecording() {
  Disable();
}

void DebugRecording::Enable(
    mojo::PendingRemote<mojom::DebugRecordingFileProvider>
        recording_file_provider) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!IsEnabled());
  file_provider_.Bind(std::move(recording_file_provider));
  media::AudioDebugRecordingManager* debug_recording_manager =
      audio_manager_->GetAudioDebugRecordingManager();
  if (debug_recording_manager == nullptr)
    return;
  debug_recording_manager->EnableDebugRecording(base::BindRepeating(
      &DebugRecording::CreateWavFile, weak_factory_.GetWeakPtr()));
}

void DebugRecording::Disable() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  // Client connection is lost, resetting the reference.
  service_ref_ = TracedServiceRef();
  if (!IsEnabled())
    return;
  file_provider_.reset();
  receiver_.reset();

  media::AudioDebugRecordingManager* debug_recording_manager =
      audio_manager_->GetAudioDebugRecordingManager();
  if (debug_recording_manager == nullptr)
    return;
  debug_recording_manager->DisableDebugRecording();
}

void DebugRecording::CreateWavFile(
    media::AudioDebugRecordingStreamType stream_type,
    uint32_t id,
    mojom::DebugRecordingFileProvider::CreateWavFileCallback reply_callback) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  file_provider_->CreateWavFile(stream_type, id, std::move(reply_callback));
}

bool DebugRecording::IsEnabled() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return file_provider_.is_bound();
}

}  // namespace audio
