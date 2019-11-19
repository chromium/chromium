// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_renderer_sink.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace media {

FakeAudioRendererSink::FakeAudioRendererSink()
    : FakeAudioRendererSink(
          AudioParameters(AudioParameters::AUDIO_FAKE,
                          CHANNEL_LAYOUT_STEREO,
                          AudioParameters::kTelephoneSampleRate,
                          1)) {}

FakeAudioRendererSink::FakeAudioRendererSink(
    const AudioParameters& hardware_params)
    : state_(kUninitialized),
      callback_(nullptr),
      output_device_info_(std::string(),
                          OUTPUT_DEVICE_STATUS_OK,
                          hardware_params),
      is_optimized_for_hw_params_(true) {}

FakeAudioRendererSink::~FakeAudioRendererSink() {
  DCHECK(!callback_);
}

void FakeAudioRendererSink::Initialize(const AudioParameters& params,
                                       RenderCallback* callback) {
  DCHECK(state_ == kUninitialized || state_ == kStopped);
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
  ChangeState(kInitialized);
}

void FakeAudioRendererSink::Start() {
  DCHECK_EQ(state_, kInitialized);
  ChangeState(kStarted);
}

void FakeAudioRendererSink::Stop() {
  callback_ = NULL;
  ChangeState(kStopped);
}

void FakeAudioRendererSink::Flush() {
  DCHECK_NE(state_, kPlaying);
}

void FakeAudioRendererSink::Pause() {
  DCHECK(state_ == kStarted || state_ == kPlaying) << "state_ " << state_;
  ChangeState(kPaused);
}

void FakeAudioRendererSink::Play() {
  DCHECK(state_ == kStarted || state_ == kPaused) << "state_ " << state_;
  DCHECK_EQ(state_, kPaused);
  ChangeState(kPlaying);
}

bool FakeAudioRendererSink::SetVolume(double volume) {
  return true;
}

OutputDeviceInfo FakeAudioRendererSink::GetOutputDeviceInfo() {
  return output_device_info_;
}

void FakeAudioRendererSink::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(info_cb), output_device_info_));
}

bool FakeAudioRendererSink::IsOptimizedForHardwareParameters() {
  return is_optimized_for_hw_params_;
}

bool FakeAudioRendererSink::CurrentThreadIsRenderingThread() {
  NOTIMPLEMENTED();
  return false;
}

bool FakeAudioRendererSink::Render(AudioBus* dest,
                                   base::TimeDelta delay,
                                   int* frames_written) {
  if (state_ != kPlaying)
    return false;

  *frames_written = callback_->Render(delay, base::TimeTicks::Now(), 0, dest);
  return true;
}

void FakeAudioRendererSink::OnRenderError() {
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kStopped);

  callback_->OnRenderError();
}

void FakeAudioRendererSink::SetIsOptimizedForHardwareParameters(bool value) {
  is_optimized_for_hw_params_ = value;
}

void FakeAudioRendererSink::ChangeState(State new_state) {
  static const char* kStateNames[] = {
    "kUninitialized",
    "kInitialized",
    "kStarted",
    "kPaused",
    "kPlaying",
    "kStopped"
  };

  DVLOG(1) << __func__ << " : " << kStateNames[state_] << " -> "
           << kStateNames[new_state];
  state_ = new_state;
}

}  // namespace media
