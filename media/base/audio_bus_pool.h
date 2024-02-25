// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUS_POOL_H_
#define MEDIA_BASE_AUDIO_BUS_POOL_H_

#include <memory>
#include <stack>

#include "base/synchronization/lock.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Thread-safe interface for reusing AudioBuses.
class MEDIA_EXPORT AudioBusPool {
 public:
  virtual ~AudioBusPool() = default;

  // If there is an AudioBus in the pool, return it. Otherwise, allocate and
  // return a new AudioBus with the correct number of frames and channels.
  virtual std::unique_ptr<AudioBus> GetAudioBus() = 0;

  // Inserts an AudioBus to the pool, allowing it to be reused with
  // GetAudioBus. |audio_bus| needs to have the same number of frames and
  // channels as was specified when creating the AudioBusPool, or the call will
  // crash.
  virtual void InsertAudioBus(std::unique_ptr<AudioBus> audio_bus) = 0;
};

class MEDIA_EXPORT AudioBusPoolImpl final : public AudioBusPool {
 public:
  AudioBusPoolImpl(const AudioParameters& params,
                   size_t preallocated,
                   size_t max_capacity);

  AudioBusPoolImpl(const AudioBusPool&) = delete;
  AudioBusPoolImpl& operator=(const AudioBusPool&) = delete;
  ~AudioBusPoolImpl() override;

  std::unique_ptr<AudioBus> GetAudioBus() override;

  void InsertAudioBus(std::unique_ptr<AudioBus> audio_bus) override;

 private:
  friend class AudioBusPoolTest;
  friend class ReconfigurableAudioBusPoolTest;

  using CreateAudioBusCallback =
      base::RepeatingCallback<std::unique_ptr<AudioBus>(
          const AudioParameters&)>;

  AudioBusPoolImpl(const AudioParameters& params,
                   size_t preallocated,
                   size_t max_capacity,
                   CreateAudioBusCallback create_audio_bus);

  const AudioParameters params_;

  const size_t max_capacity_;

  CreateAudioBusCallback create_audio_bus_;

  std::stack<std::unique_ptr<AudioBus>> audio_buses_ GUARDED_BY(lock_);

  base::Lock lock_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUS_POOL_H_
