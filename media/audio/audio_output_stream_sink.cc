// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_stream_sink.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

AudioOutputStreamSink::AudioOutputStreamSink()
    : initialized_(false),
      started_(false),
      render_callback_(NULL),
      active_render_callback_(NULL),
      audio_task_runner_(AudioManager::Get()->GetTaskRunner()),
      stream_(NULL) {}

AudioOutputStreamSink::~AudioOutputStreamSink() = default;

void AudioOutputStreamSink::Initialize(const AudioParameters& params,
                                       RenderCallback* callback) {
  DCHECK(callback);
  DCHECK(!started_);
  params_ = params;
  render_callback_ = callback;
  initialized_ = true;
}

void AudioOutputStreamSink::Start() {
  DCHECK(initialized_);
  DCHECK(!started_);
  {
    base::AutoLock al(callback_lock_);
    active_render_callback_ = render_callback_;
  }
  started_ = true;
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputStreamSink::DoStart, this, params_));
}

void AudioOutputStreamSink::Stop() {
  ClearCallback();
  started_ = false;
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputStreamSink::DoStop, this));
}

void AudioOutputStreamSink::Pause() {
  ClearCallback();
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputStreamSink::DoPause, this));
}

void AudioOutputStreamSink::Flush() {
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputStreamSink::DoFlush, this));
}

void AudioOutputStreamSink::Play() {
  {
    base::AutoLock al(callback_lock_);
    active_render_callback_ = render_callback_;
  }
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputStreamSink::DoPlay, this));
}

bool AudioOutputStreamSink::SetVolume(double volume) {
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputStreamSink::DoSetVolume, this, volume));
  return true;
}

OutputDeviceInfo AudioOutputStreamSink::GetOutputDeviceInfo() {
  return OutputDeviceInfo(OUTPUT_DEVICE_STATUS_OK);
}

void AudioOutputStreamSink::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(info_cb), GetOutputDeviceInfo()));
}

bool AudioOutputStreamSink::IsOptimizedForHardwareParameters() {
  return true;
}

bool AudioOutputStreamSink::CurrentThreadIsRenderingThread() {
  NOTIMPLEMENTED();
  return false;
}

int AudioOutputStreamSink::OnMoreData(base::TimeDelta delay,
                                      base::TimeTicks delay_timestamp,
                                      int prior_frames_skipped,
                                      AudioBus* dest) {
  // Note: Runs on the audio thread created by the OS.
  base::AutoLock al(callback_lock_);
  if (!active_render_callback_)
    return 0;

  return active_render_callback_->Render(delay, delay_timestamp,
                                         prior_frames_skipped, dest);
}

void AudioOutputStreamSink::OnError() {
  // Note: Runs on the audio thread created by the OS.
  base::AutoLock al(callback_lock_);
  if (active_render_callback_)
    active_render_callback_->OnRenderError();
}

void AudioOutputStreamSink::DoStart(const AudioParameters& params) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  // Create an AudioOutputStreamProxy which will handle any and all resampling
  // necessary to generate a low latency output stream.
  active_params_ = params;
  stream_ = AudioManager::Get()->MakeAudioOutputStreamProxy(active_params_,
                                                            std::string());
  if (!stream_ || !stream_->Open()) {
    {
      base::AutoLock al(callback_lock_);
      if (active_render_callback_)
        active_render_callback_->OnRenderError();
    }
    if (stream_)
      stream_->Close();
    stream_ = NULL;
  }
}

void AudioOutputStreamSink::DoStop() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  if (!stream_)
    return;

  DoPause();
  stream_->Close();
  stream_ = NULL;
}

void AudioOutputStreamSink::DoPause() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  stream_->Stop();
}

void AudioOutputStreamSink::DoFlush() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  if (stream_) {
    stream_->Flush();
  }
}

void AudioOutputStreamSink::DoPlay() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  stream_->Start(this);
}

void AudioOutputStreamSink::DoSetVolume(double volume) {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  stream_->SetVolume(volume);
}

void AudioOutputStreamSink::ClearCallback() {
  base::AutoLock al(callback_lock_);
  active_render_callback_ = NULL;
}

}  // namespace media
