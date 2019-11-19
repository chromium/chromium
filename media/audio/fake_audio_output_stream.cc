// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_output_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_manager_base.h"

namespace media {

// static
AudioOutputStream* FakeAudioOutputStream::MakeFakeStream(
    AudioManagerBase* manager, const AudioParameters& params) {
  return new FakeAudioOutputStream(manager, params);
}

FakeAudioOutputStream::FakeAudioOutputStream(AudioManagerBase* manager,
                                             const AudioParameters& params)
    : audio_manager_(manager),
      fixed_data_delay_(FakeAudioWorker::ComputeFakeOutputDelay(params)),
      callback_(NULL),
      fake_worker_(manager->GetWorkerTaskRunner(), params),
      audio_bus_(AudioBus::Create(params)) {}

FakeAudioOutputStream::~FakeAudioOutputStream() {
  DCHECK(!callback_);
}

bool FakeAudioOutputStream::Open() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_bus_->Zero();
  return true;
}

void FakeAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  callback_ = callback;
  fake_worker_.Start(base::BindRepeating(&FakeAudioOutputStream::CallOnMoreData,
                                         base::Unretained(this)));
}

void FakeAudioOutputStream::Stop() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  fake_worker_.Stop();
  callback_ = NULL;
}

void FakeAudioOutputStream::Close() {
  DCHECK(!callback_);
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager_->ReleaseOutputStream(this);
}

void FakeAudioOutputStream::Flush() {}

void FakeAudioOutputStream::SetVolume(double volume) {}

void FakeAudioOutputStream::GetVolume(double* volume) {
  *volume = 0;
}

void FakeAudioOutputStream::CallOnMoreData(base::TimeTicks ideal_time,
                                           base::TimeTicks now) {
  DCHECK(audio_manager_->GetWorkerTaskRunner()->BelongsToCurrentThread());
  // Real streams provide small tweaks to their delay values, alongside the
  // current system time; and so the same is done here.
  const auto delay =
      fixed_data_delay_ + std::max(base::TimeDelta(), ideal_time - now);
  callback_->OnMoreData(delay, now, 0, audio_bus_.get());
}

void FakeAudioOutputStream::SetMute(bool muted) {}

}  // namespace media
