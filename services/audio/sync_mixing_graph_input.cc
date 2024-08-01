// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/sync_mixing_graph_input.h"

#include "base/trace_event/trace_event.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_timestamp_helper.h"

namespace audio {

SyncMixingGraphInput::SyncMixingGraphInput(MixingGraph* graph,
                                           const media::AudioParameters& params)
    : graph_(graph), params_(params) {
  DCHECK(graph);
  CHECK(params_.IsValid());
}

SyncMixingGraphInput::~SyncMixingGraphInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
}

// Either calls Render() directly to produce the requested input audio, or - in
// the case of a frames per buffer mismatch - pulls audio from the |fifo_| which
// in turn calls Render() as needed.
double SyncMixingGraphInput::ProvideInput(
    media::AudioBus* audio_bus,
    uint32_t frames_delayed,
    const media::AudioGlitchInfo& glitch_info) {
  DCHECK_EQ(audio_bus->channels(), params_.channels());
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("audio"),
              "SyncMixingGraphInput::ProvideInput", "delay (frames)",
              frames_delayed, "bus frames", audio_bus->frames());

  glitch_info_accumulator_.Add(glitch_info);

  if (!fifo_ && audio_bus->frames() != params_.frames_per_buffer()) {
    fifo_ = std::make_unique<media::AudioPullFifo>(
        params_.channels(), params_.frames_per_buffer(),
        base::BindRepeating(&SyncMixingGraphInput::Render,
                            base::Unretained(this)));
  }

  // Used by Render() for delay calculation.
  converter_render_frame_delay_ = frames_delayed;
  if (fifo_)
    fifo_->Consume(audio_bus, audio_bus->frames());
  else
    Render(0, audio_bus);
  converter_render_frame_delay_ = 0;

  return volume_;
}

const media::AudioParameters& SyncMixingGraphInput::GetParams() const {
  return params_;
}

void SyncMixingGraphInput::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  volume_ = volume;
}

void SyncMixingGraphInput::Start(
    media::AudioOutputStream::AudioSourceCallback* source_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!fifo_);
  DCHECK(!source_callback_);
  source_callback_ = source_callback;
  graph_->AddInput(this);
}

void SyncMixingGraphInput::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  graph_->RemoveInput(this);
  if (fifo_)
    fifo_.reset();
  source_callback_ = nullptr;
}

void SyncMixingGraphInput::Render(int fifo_frame_delay,
                                  media::AudioBus* audio_bus) {
  DCHECK(source_callback_);
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("audio"),
              "SyncMixingGraphInput::Render", "this", static_cast<void*>(this),
              "delay (frames)", fifo_frame_delay, "bus frames",
              audio_bus->frames());

  base::TimeDelta delay = media::AudioTimestampHelper::FramesToTime(
      converter_render_frame_delay_ + fifo_frame_delay, params_.sample_rate());
  source_callback_->OnMoreData(delay, base::TimeTicks::Now(),
                               glitch_info_accumulator_.GetAndReset(),
                               audio_bus,
                               /*is_mixing=*/true);
}

}  // namespace audio
