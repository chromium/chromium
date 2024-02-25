// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_bus_pool.h"

#include <memory>
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

namespace media {

AudioBusPoolImpl::AudioBusPoolImpl(const AudioParameters& params,
                                   size_t preallocated,
                                   size_t max_capacity)
    : AudioBusPoolImpl(params,
                       preallocated,
                       max_capacity,
                       base::BindRepeating(
                           static_cast<std::unique_ptr<AudioBus> (*)(
                               const AudioParameters&)>(&AudioBus::Create))) {}

AudioBusPoolImpl::~AudioBusPoolImpl() = default;

std::unique_ptr<AudioBus> AudioBusPoolImpl::GetAudioBus() {
  base::AutoLock auto_lock(lock_);
  if (!audio_buses_.empty()) {
    std::unique_ptr<AudioBus> bus = std::move(audio_buses_.top());
    audio_buses_.pop();
    return bus;
  }
  return create_audio_bus_.Run(params_);
}

void AudioBusPoolImpl::InsertAudioBus(std::unique_ptr<AudioBus> audio_bus) {
  CHECK_EQ(audio_bus->channels(), params_.channels());
  CHECK_EQ(audio_bus->frames(), params_.frames_per_buffer());

  base::AutoLock auto_lock(lock_);
  if (audio_buses_.size() < max_capacity_) {
    audio_buses_.push(std::move(audio_bus));
  }
}

AudioBusPoolImpl::AudioBusPoolImpl(const AudioParameters& params,
                                   size_t preallocated,
                                   size_t max_capacity,
                                   CreateAudioBusCallback create_audio_bus)
    : params_(params),
      max_capacity_(max_capacity),
      create_audio_bus_(std::move(create_audio_bus)) {
  DCHECK_GE(max_capacity, preallocated);
  while (preallocated-- > 0) {
    audio_buses_.push(create_audio_bus_.Run(params_));
  }
}

}  // namespace media
