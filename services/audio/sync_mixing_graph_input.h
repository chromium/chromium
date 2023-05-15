// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SYNC_MIXING_GRAPH_INPUT_H_
#define SERVICES_AUDIO_SYNC_MIXING_GRAPH_INPUT_H_

#include <atomic>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "services/audio/mixing_graph.h"

namespace media {
class AudioPullFifo;
}

namespace audio {

// Input to the mixing graph.
class SyncMixingGraphInput final : public MixingGraph::Input {
 public:
  const double kDefaultVolume = 1.0;

  SyncMixingGraphInput(MixingGraph* graph,
                       const media::AudioParameters& params);
  ~SyncMixingGraphInput() final;

  // media::AudioConverter::InputCallback.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const media::AudioGlitchInfo& glitch_info) final;

  const media::AudioParameters& GetParams() const final;

  // Sets a new volume to be applied to the input during mixing.
  void SetVolume(double volume) final;

  // Adds the input to the mixing graph. The mixing graph will repeatedly pull
  // data from |callback|.
  void Start(media::AudioOutputStream::AudioSourceCallback* callback) final;

  // Removes the input from the mixing graph. The mixing graph will stop pulling
  // data from the input.
  void Stop() final;

 private:
  void Render(int fifo_frame_delay, media::AudioBus* audio_bus);

  // Pointer to the mixing graph to which the input belongs.
  const raw_ptr<MixingGraph, FlakyDanglingUntriaged> graph_;

  // Channel layout, sample rate and number of frames of the input.
  const media::AudioParameters params_;

  // Volume of the input.
  std::atomic<double> volume_{kDefaultVolume};

  // Callback providing audio to the mixing graph when requested.
  raw_ptr<media::AudioOutputStream::AudioSourceCallback> source_callback_ =
      nullptr;

  // Handles buffering when there is a mismatch in number of frames between the
  // input and the output of the mixing graph. Created on-demand.
  std::unique_ptr<media::AudioPullFifo> fifo_;

  // Accumulates glitch info in ProvideInput() and passes it on to
  // |source_callback_| in Render().
  media::AudioGlitchInfo::Accumulator glitch_info_accumulator_;

  // Used for calculating the playback delay.
  int converter_render_frame_delay_ = 0;
  SEQUENCE_CHECKER(owning_sequence_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SYNC_MIXING_GRAPH_INPUT_H_
