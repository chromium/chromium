// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher.h"

#include "base/task/single_thread_task_runner.h"
#include "media/audio/audio_manager.h"

namespace media {

AudioOutputDispatcher::AudioOutputDispatcher(AudioManager* audio_manager)
    : audio_manager_(audio_manager) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
}

AudioOutputDispatcher::~AudioOutputDispatcher() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

}  // namespace media
