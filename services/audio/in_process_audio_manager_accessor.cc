// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/in_process_audio_manager_accessor.h"

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"

namespace audio {

InProcessAudioManagerAccessor::InProcessAudioManagerAccessor(
    media::AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK(audio_manager_);
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread())
      << "AudioManagerAccessor must live on audio thread";
}

InProcessAudioManagerAccessor::~InProcessAudioManagerAccessor() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

media::AudioManager* InProcessAudioManagerAccessor::GetAudioManager() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return audio_manager_;
}

void InProcessAudioManagerAccessor::SetAudioLogFactory(
    media::AudioLogFactory* factory) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace audio
