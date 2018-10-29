// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_output_stream.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

VirtualAudioOutputStream::VirtualAudioOutputStream(
    const AudioParameters& params, VirtualAudioInputStream* target,
    const AfterCloseCallback& after_close_cb)
    : params_(params), target_input_stream_(target),
      after_close_cb_(after_close_cb), callback_(NULL), volume_(1.0f) {
  DCHECK(params_.IsValid());
  DCHECK(target);

  // VAOS can be constructed on any thread, but will DCHECK that all
  // AudioOutputStream methods are called from the same thread.
  thread_checker_.DetachFromThread();
}

VirtualAudioOutputStream::~VirtualAudioOutputStream() {
  DCHECK(!callback_);
}

bool VirtualAudioOutputStream::Open() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void VirtualAudioOutputStream::Start(AudioSourceCallback* callback)  {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback_);
  callback_ = callback;
  target_input_stream_->AddInputProvider(this, params_);
}

void VirtualAudioOutputStream::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (callback_) {
    target_input_stream_->RemoveInputProvider(this, params_);
    callback_ = NULL;
  }
}

void VirtualAudioOutputStream::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Stop();

  // If a non-null AfterCloseCallback was provided to the constructor, invoke it
  // here.  The callback is moved to a stack-local first since |this| could be
  // destroyed during Run().
  if (after_close_cb_) {
    const AfterCloseCallback cb = after_close_cb_;
    after_close_cb_.Reset();
    cb.Run(this);
  }
}

void VirtualAudioOutputStream::SetVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  volume_ = volume;
}

void VirtualAudioOutputStream::GetVolume(double* volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *volume = volume_;
}

double VirtualAudioOutputStream::ProvideInput(AudioBus* audio_bus,
                                              uint32_t frames_delayed) {
  // Note: This method may be invoked on any one thread, depending on the
  // platform.
  DCHECK(callback_);

  const base::TimeDelta delay =
      AudioTimestampHelper::FramesToTime(frames_delayed, params_.sample_rate());
  const int frames =
      callback_->OnMoreData(delay, base::TimeTicks::Now(), 0, audio_bus);
  if (frames < audio_bus->frames())
    audio_bus->ZeroFramesPartial(frames, audio_bus->frames() - frames);

  return frames > 0 ? volume_ : 0;
}

}  // namespace media
