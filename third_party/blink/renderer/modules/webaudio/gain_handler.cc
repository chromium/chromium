// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/gain_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfOutputChannels = 1;

}  // namespace

GainHandler::GainHandler(AudioNode& node,
                         float sample_rate,
                         AudioParamHandler& gain)
    : AudioHandler(kNodeTypeGain, node, sample_rate),
      gain_(&gain),
      sample_accurate_gain_values_(
          GetDeferredTaskHandler().RenderQuantumFrames()) {
  AddInput();
  AddOutput(kNumberOfOutputChannels);

  Initialize();
}

scoped_refptr<GainHandler> GainHandler::Create(AudioNode& node,
                                               float sample_rate,
                                               AudioParamHandler& gain) {
  return base::AdoptRef(new GainHandler(node, sample_rate, gain));
}

void GainHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "GainHandler::Process");

  AudioBus* output_bus = Output(0).Bus();
  DCHECK(output_bus);

  if (!IsInitialized() || !Input(0).IsConnected()) {
    output_bus->Zero();
  } else {
    scoped_refptr<AudioBus> input_bus = Input(0).Bus();

    bool is_sample_accurate = gain_->HasSampleAccurateValues();

    if (is_sample_accurate && gain_->IsAudioRate()) {
      // Apply sample-accurate gain scaling for precise envelopes, grain
      // windows, etc.
      DCHECK_LE(frames_to_process, sample_accurate_gain_values_.size());
      float* gain_values = sample_accurate_gain_values_.Data();
      gain_->CalculateSampleAccurateValues(gain_values, frames_to_process);
      output_bus->CopyWithSampleAccurateGainValuesFrom(*input_bus, gain_values,
                                                       frames_to_process);

      return;
    }

    // The gain is not sample-accurate or not a-rate.  In this case, we have a
    // fixed gain for the render and just need to incorporate any inputs to the
    // gain, if any.
    float gain = is_sample_accurate ? gain_->FinalValue() : gain_->Value();

    if (gain == 0) {
      output_bus->Zero();
    } else {
      output_bus->CopyWithGainFrom(*input_bus, gain);
    }
  }
}

void GainHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  constexpr unsigned render_quantum_frames_expected = 128;
  CHECK_EQ(GetDeferredTaskHandler().RenderQuantumFrames(),
           render_quantum_frames_expected);
  DCHECK_LE(frames_to_process, render_quantum_frames_expected);

  float values[render_quantum_frames_expected];

  gain_->CalculateSampleAccurateValues(values, frames_to_process);
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void GainHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK(input);
  DCHECK_EQ(input, &Input(0));

  unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further
    // downstream in the graph.
    Output(0).SetNumberOfChannels(number_of_channels);
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

}  // namespace blink
