// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/reconfigurable_audio_bus_pool.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_bus_pool.h"
#include "media/base/audio_parameters.h"
#include "media/base/reentrancy_checker.h"

namespace media {

ReconfigurableAudioBusPoolImpl::ReconfigurableAudioBusPoolImpl(
    base::TimeDelta preallocated_audio_bus_pool_duration)
    : preallocated_audio_bus_pool_duration_(
          preallocated_audio_bus_pool_duration) {}

ReconfigurableAudioBusPoolImpl::~ReconfigurableAudioBusPoolImpl() = default;

std::unique_ptr<AudioBus> ReconfigurableAudioBusPoolImpl::GetAudioBus() {
  CHECK(audio_bus_pool_);
  NON_REENTRANT_SCOPE(reentrancy_checker_);

  return audio_bus_pool_->GetAudioBus();
}

void ReconfigurableAudioBusPoolImpl::InsertAudioBus(
    std::unique_ptr<AudioBus> audio_bus) {
  CHECK(audio_bus_pool_);
  if (audio_bus->channels() != audio_parameters_.channels() ||
      audio_bus->frames() != audio_parameters_.frames_per_buffer()) {
    // Drop an in-flight audio bus if the pool has been reconfigured.
    return;
  }

  audio_bus_pool_->InsertAudioBus(std::move(audio_bus));
}

void ReconfigurableAudioBusPoolImpl::Reconfigure(
    const AudioParameters& audio_parameters) {
  NON_REENTRANT_SCOPE(reentrancy_checker_);
  if (!audio_bus_pool_ || !audio_parameters.Equals(audio_parameters_)) {
    audio_parameters_ = audio_parameters;
    int number_of_audio_buses = preallocated_audio_bus_pool_duration_ /
                                audio_parameters.GetBufferDuration();

    audio_bus_pool_ = std::make_unique<AudioBusPoolImpl>(
        audio_parameters, number_of_audio_buses, number_of_audio_buses);
  }
}

}  // namespace media
