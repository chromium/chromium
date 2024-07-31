// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer.h"

#include <cmath>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"

namespace blink {

constexpr base::TimeDelta kPauseDelay = base::Seconds(10);

AudioRendererMixer::AudioRendererMixer(
    const media::AudioParameters& output_params,
    scoped_refptr<media::AudioRendererSink> sink)
    : output_params_(output_params),
      audio_sink_(std::move(sink)),
      aggregate_converter_(output_params, output_params, true),
      pause_delay_(kPauseDelay),
      last_play_time_(base::TimeTicks::Now()),
      // Initialize `playing_` to true since Start() results in an auto-play.
      playing_(true) {
  DCHECK(audio_sink_);

  // If enabled we will disable the real audio output stream for muted/silent
  // playbacks after some time elapses.
  RenderCallback* callback = this;
  audio_sink_->Initialize(output_params, callback);
  audio_sink_->Start();
}

AudioRendererMixer::~AudioRendererMixer() {
  // AudioRendererSink must be stopped before mixer is destructed.
  audio_sink_->Stop();

  // Ensure that all mixer inputs have removed themselves prior to destruction.
  DCHECK(aggregate_converter_.empty());
  DCHECK(converters_.empty());
  DCHECK(error_callbacks_.empty());
}

void AudioRendererMixer::AddMixerInput(
    const media::AudioParameters& input_params,
    media::AudioConverter::InputCallback* input) {
  base::AutoLock auto_lock(lock_);
  if (!playing_) {
    playing_ = true;
    last_play_time_ = base::TimeTicks::Now();
    audio_sink_->Play();
  }

  int input_sample_rate = input_params.sample_rate();
  if (can_passthrough(input_sample_rate)) {
    aggregate_converter_.AddInput(input);
  } else {
    auto converter = converters_.find(input_sample_rate);
    if (converter == converters_.end()) {
      std::pair<AudioConvertersMap::iterator, bool> result = converters_.insert(
          std::make_pair(input_sample_rate,
                         std::make_unique<media::LoopbackAudioConverter>(
                             // We expect all InputCallbacks to be
                             // capable of handling arbitrary buffer
                             // size requests, disabling FIFO.
                             input_params, output_params_, true)));
      converter = result.first;

      // Add newly-created resampler as an input to the aggregate mixer.
      aggregate_converter_.AddInput(converter->second.get());
    }
    converter->second->AddInput(input);
  }
}

void AudioRendererMixer::RemoveMixerInput(
    const media::AudioParameters& input_params,
    media::AudioConverter::InputCallback* input) {
  base::AutoLock auto_lock(lock_);

  int input_sample_rate = input_params.sample_rate();
  if (can_passthrough(input_sample_rate)) {
    aggregate_converter_.RemoveInput(input);
  } else {
    auto converter = converters_.find(input_sample_rate);
    CHECK(converter != converters_.end(), base::NotFatalUntil::M130);
    converter->second->RemoveInput(input);
    if (converter->second->empty()) {
      // Remove converter when it's empty.
      aggregate_converter_.RemoveInput(converter->second.get());
      converters_.erase(converter);
    }
  }
}

void AudioRendererMixer::AddErrorCallback(AudioRendererMixerInput* input) {
  base::AutoLock auto_lock(lock_);
  error_callbacks_.insert(input);
}

void AudioRendererMixer::RemoveErrorCallback(AudioRendererMixerInput* input) {
  base::AutoLock auto_lock(lock_);
  error_callbacks_.erase(input);
}

bool AudioRendererMixer::CurrentThreadIsRenderingThread() {
  return audio_sink_->CurrentThreadIsRenderingThread();
}

void AudioRendererMixer::SetPauseDelayForTesting(base::TimeDelta delay) {
  base::AutoLock auto_lock(lock_);
  pause_delay_ = delay;
}

bool AudioRendererMixer::HasSinkError() {
  base::AutoLock auto_lock(lock_);
  return sink_error_;
}

int AudioRendererMixer::Render(base::TimeDelta delay,
                               base::TimeTicks delay_timestamp,
                               const media::AudioGlitchInfo& glitch_info,
                               media::AudioBus* audio_bus) {
  TRACE_EVENT("audio", "AudioRendererMixer::Render", "playout_delay (ms)",
              delay.InMillisecondsF(), "delay_timestamp (ms)",
              (delay_timestamp - base::TimeTicks()).InMillisecondsF());
  base::AutoLock auto_lock(lock_);

  // If there are no mixer inputs and we haven't seen one for a while, pause the
  // sink to avoid wasting resources when media elements are present but remain
  // in the pause state.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (!aggregate_converter_.empty()) {
    last_play_time_ = now;
  } else if (now - last_play_time_ >= pause_delay_ && playing_) {
    audio_sink_->Pause();
    playing_ = false;
  }

  // Since AudioConverter uses uint32_t for delay calculations, we must drop
  // negative delay values (which are incorrect anyways).
  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  uint32_t frames_delayed =
      base::saturated_cast<uint32_t>(media::AudioTimestampHelper::TimeToFrames(
          delay, output_params_.sample_rate()));
  aggregate_converter_.ConvertWithInfo(frames_delayed, glitch_info, audio_bus);
  return audio_bus->frames();
}

void AudioRendererMixer::OnRenderError() {
  // Call each mixer input and signal an error.
  base::AutoLock auto_lock(lock_);
  sink_error_ = true;
  for (AudioRendererMixerInput* input : error_callbacks_) {
    input->OnRenderError();
  }
}

}  // namespace blink
