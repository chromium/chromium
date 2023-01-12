// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/debug_recording.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/audio/audio_manager.h"

namespace audio {

DebugRecording::DebugRecording(
    mojo::PendingReceiver<mojom::DebugRecording> receiver,
    media::AudioManager* audio_manager,
    media::AecdumpRecordingManager* aecdump_recording_manager)
    : audio_manager_(audio_manager),
      aecdump_recording_manager_(aecdump_recording_manager),
      receiver_(this, std::move(receiver)) {
  DCHECK(audio_manager_ != nullptr);
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

  // The remote end may disable debug recording by closing the connection. The
  // DebugRecording object itself is not destroyed: It will be cleaned-up by
  // service either on next bind request or when service is shut down.
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
  if (debug_recording_manager) {
    debug_recording_manager->EnableDebugRecording(base::BindRepeating(
        &DebugRecording::CreateWavFile, weak_factory_.GetWeakPtr()));
  }

  if (aecdump_recording_manager_) {
    aecdump_recording_manager_->EnableDebugRecording(base::BindRepeating(
        &DebugRecording::CreateAecdumpFile, weak_factory_.GetWeakPtr()));
  }
}

void DebugRecording::Disable() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  if (!IsEnabled())
    return;
  file_provider_.reset();
  receiver_.reset();

  media::AudioDebugRecordingManager* debug_recording_manager =
      audio_manager_->GetAudioDebugRecordingManager();
  if (debug_recording_manager) {
    debug_recording_manager->DisableDebugRecording();
  }

  if (aecdump_recording_manager_) {
    aecdump_recording_manager_->DisableDebugRecording();
  }
}

void DebugRecording::CreateWavFile(
    media::AudioDebugRecordingStreamType stream_type,
    uint32_t id,
    mojom::DebugRecordingFileProvider::CreateWavFileCallback reply_callback) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  file_provider_->CreateWavFile(stream_type, id, std::move(reply_callback));
}

void DebugRecording::CreateAecdumpFile(
    uint32_t id,
    mojom::DebugRecordingFileProvider::CreateAecdumpFileCallback
        reply_callback) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  file_provider_->CreateAecdumpFile(id, std::move(reply_callback));
}

bool DebugRecording::IsEnabled() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return file_provider_.is_bound();
}

}  // namespace audio
