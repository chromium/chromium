// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_RECONFIGURABLE_AUDIO_BUS_POOL_H_
#define MEDIA_AUDIO_RECONFIGURABLE_AUDIO_BUS_POOL_H_

#include <memory>

#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"
#include "media/base/reentrancy_checker.h"

namespace media {

class AudioBusPool;

class MEDIA_EXPORT ReconfigurableAudioBusPoolImpl : public AudioBusPool {
 public:
  explicit ReconfigurableAudioBusPoolImpl(
      base::TimeDelta preallocated_audio_bus_pool_duration);
  ReconfigurableAudioBusPoolImpl(const ReconfigurableAudioBusPoolImpl&) =
      delete;
  ReconfigurableAudioBusPoolImpl& operator=(
      const ReconfigurableAudioBusPoolImpl&) = delete;
  ~ReconfigurableAudioBusPoolImpl() override;

  // AudioBusPool
  // Must call Reconfigure() first and can't be called concurrently with
  // Reconfigure().
  std::unique_ptr<AudioBus> GetAudioBus() override;
  void InsertAudioBus(std::unique_ptr<AudioBus> audio_bus) override;

  // Must be called once before GetAudioBus() or InsertAudioBus() and can't be
  // called concurrently with GetAudioBus() or InsertAudioBus().
  void Reconfigure(const AudioParameters& audio_parameters);

 private:
  friend class ReconfigurableAudioBusPoolTest;

  std::unique_ptr<AudioBusPool> audio_bus_pool_;
  AudioParameters audio_parameters_;
  const base::TimeDelta preallocated_audio_bus_pool_duration_;

  REENTRANCY_CHECKER(reentrancy_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_RECONFIGURABLE_AUDIO_BUS_POOL_H_
