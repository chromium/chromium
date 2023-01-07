// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_IN_PROCESS_AUDIO_MANAGER_ACCESSOR_H_
#define SERVICES_AUDIO_IN_PROCESS_AUDIO_MANAGER_ACCESSOR_H_

#include "base/memory/raw_ptr.h"
#include "services/audio/service.h"

namespace media {
class AudioManager;
class AudioLogFactory;
}

namespace audio {

// Holds a pointer to an existing AudioManager. Must be created on AudioManager
// main thread. To be used with in-process Audio service which does not own
// AudioManager.
class InProcessAudioManagerAccessor final
    : public Service::AudioManagerAccessor {
 public:
  explicit InProcessAudioManagerAccessor(media::AudioManager* audio_manager);

  InProcessAudioManagerAccessor(const InProcessAudioManagerAccessor&) = delete;
  InProcessAudioManagerAccessor& operator=(
      const InProcessAudioManagerAccessor&) = delete;

  ~InProcessAudioManagerAccessor() final;

  void Shutdown() final {}  // AudioManager must be shut down by its owner.
  media::AudioManager* GetAudioManager() final;

  // Should not be called on this implementation.
  void SetAudioLogFactory(media::AudioLogFactory* factory) final;

 private:
  const raw_ptr<media::AudioManager> audio_manager_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_IN_PROCESS_AUDIO_MANAGER_ACCESSOR_H_
