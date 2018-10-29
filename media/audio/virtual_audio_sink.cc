// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/virtual_audio_sink.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/audio/virtual_audio_input_stream.h"

namespace media {

// Buffer size limit is chosen large enough that in the normal case, we do not
// have data loss.
constexpr int kBufferSizeSecond = 1;

// The accuracy contains two components: 1. the cross-process communication,
// there is fluctuation between the actual and the ideal instant when we receive
// the audio data; 2. The system clock resolution: worst case is ~15ms on
// Windows machines without a working high-resolution clock.
constexpr int kClockAccuracyMillisecond = 20;

// See AudioShifter comment for detail about this parameter. We just take the
// suggestion from there.
constexpr int kAdjustTimeSecond = 1;

VirtualAudioSink::VirtualAudioSink(const AudioParameters& param,
                                   VirtualAudioInputStream* target,
                                   const AfterCloseCallback& callback)
    : params_(param),
      target_(target),
      shifter_(base::TimeDelta::FromSeconds(kBufferSizeSecond),
               base::TimeDelta::FromMilliseconds(kClockAccuracyMillisecond),
               base::TimeDelta::FromSeconds(kAdjustTimeSecond),
               param.sample_rate(),
               param.channels()),
      after_close_callback_(callback) {
  target_->AddInputProvider(this, params_);
}

VirtualAudioSink::~VirtualAudioSink() = default;

void VirtualAudioSink::Close() {
  target_->RemoveInputProvider(this, params_);
  if (after_close_callback_)
    std::move(after_close_callback_).Run(this);
}

void VirtualAudioSink::OnData(std::unique_ptr<AudioBus> source,
                              base::TimeTicks reference_time) {
  base::AutoLock lock(shifter_lock_);
  shifter_.Push(std::move(source), reference_time);
}

double VirtualAudioSink::ProvideInput(AudioBus* audio_bus,
                                      uint32_t frames_delayed) {
  base::AutoLock lock(shifter_lock_);
  shifter_.Pull(audio_bus,
                base::TimeTicks::Now() +
                    base::TimeDelta::FromMicroseconds(
                        frames_delayed * params_.GetMicrosecondsPerFrame()));
  return 1;
}
}
