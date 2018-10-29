// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_hash.h"
#include "media/base/fake_audio_worker.h"

namespace media {

NullAudioSink::NullAudioSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : initialized_(false),
      started_(false),
      playing_(false),
      callback_(NULL),
      task_runner_(task_runner) {}

NullAudioSink::~NullAudioSink() = default;

void NullAudioSink::Initialize(const AudioParameters& params,
                               RenderCallback* callback) {
  DCHECK(!started_);
  fake_worker_.reset(new FakeAudioWorker(task_runner_, params));
  audio_bus_ = AudioBus::Create(params);
  callback_ = callback;
  initialized_ = true;
}

void NullAudioSink::Start() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(initialized_);
  DCHECK(!started_);
  started_ = true;
}

void NullAudioSink::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  started_ = false;
  // Stop may be called at any time, so we have to check before stopping.
  if (fake_worker_)
    fake_worker_->Stop();
}

void NullAudioSink::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(started_);

  if (playing_)
    return;

  fake_worker_->Start(base::Bind(
      &NullAudioSink::CallRender, base::Unretained(this)));

  playing_ = true;
}

void NullAudioSink::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(started_);

  if (!playing_)
    return;

  fake_worker_->Stop();
  playing_ = false;
}

bool NullAudioSink::SetVolume(double volume) {
  // Audio is always muted.
  return volume == 0.0;
}

OutputDeviceInfo NullAudioSink::GetOutputDeviceInfo() {
  return OutputDeviceInfo(OUTPUT_DEVICE_STATUS_OK);
}

bool NullAudioSink::IsOptimizedForHardwareParameters() {
  return false;
}

bool NullAudioSink::CurrentThreadIsRenderingThread() {
  return task_runner_->BelongsToCurrentThread();
}

void NullAudioSink::SwitchOutputDevice(const std::string& device_id,
                                       OutputDeviceStatusCB callback) {
  std::move(callback).Run(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
}

void NullAudioSink::CallRender() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int frames_received = callback_->Render(
      base::TimeDelta(), base::TimeTicks::Now(), 0, audio_bus_.get());
  if (!audio_hash_ || frames_received <= 0)
    return;

  audio_hash_->Update(audio_bus_.get(), frames_received);
}

void NullAudioSink::StartAudioHashForTesting() {
  DCHECK(!initialized_);
  audio_hash_.reset(new AudioHash());
}

std::string NullAudioSink::GetAudioHashForTesting() {
  return audio_hash_ ? audio_hash_->ToString() : std::string();
}

}  // namespace media
