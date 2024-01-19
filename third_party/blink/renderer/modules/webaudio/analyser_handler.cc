// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/analyser_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr unsigned kDefaultNumberOfInputChannels = 2;
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

AnalyserHandler::AnalyserHandler(AudioNode& node, float sample_rate)
    : AudioHandler(kNodeTypeAnalyser, node, sample_rate),
      analyser_(
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames()) {
  AddInput();
  channel_count_ = kDefaultNumberOfInputChannels;
  AddOutput(kDefaultNumberOfOutputChannels);

  Initialize();
}

scoped_refptr<AnalyserHandler> AnalyserHandler::Create(AudioNode& node,
                                                       float sample_rate) {
  return base::AdoptRef(new AnalyserHandler(node, sample_rate));
}

AnalyserHandler::~AnalyserHandler() {
  Uninitialize();
}

void AnalyserHandler::Process(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  // It's possible that output is not connected. Assign nullptr to indicate
  // such case.
  AudioBus* output_bus = Output(0).RenderingFanOutCount() > 0
      ? Output(0).Bus() : nullptr;

  if (!IsInitialized() && output_bus) {
    output_bus->Zero();
    return;
  }

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();

  // Give the analyser the audio which is passing through this
  // AudioNode.  This must always be done so that the state of the
  // Analyser reflects the current input.
  analyser_.WriteInput(input_bus.get(), frames_to_process);

  // Subsequent steps require `output_bus` to be valid.
  if (!output_bus) {
    return;
  }

  if (!Input(0).IsConnected()) {
    // No inputs, so clear the output, and propagate the silence hint.
    output_bus->Zero();
    return;
  }

  // For in-place processing, our override of pullInputs() will just pass the
  // audio data through unchanged if the channel count matches from input to
  // output (resulting in inputBus == outputBus). Otherwise, do an up-mix to
  // stereo.
  if (input_bus != output_bus) {
    output_bus->CopyFrom(*input_bus);
  }
}

void AnalyserHandler::SetFftSize(unsigned size,
                                 ExceptionState& exception_state) {
  if (!analyser_.SetFftSize(size)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        (size < RealtimeAnalyser::kMinFFTSize ||
         size > RealtimeAnalyser::kMaxFFTSize)
            ? ExceptionMessages::IndexOutsideRange(
                  "FFT size", size, RealtimeAnalyser::kMinFFTSize,
                  ExceptionMessages::kInclusiveBound,
                  RealtimeAnalyser::kMaxFFTSize,
                  ExceptionMessages::kInclusiveBound)
            : ("The value provided (" + String::Number(size) +
               ") is not a power of two."));
  }
}

void AnalyserHandler::SetMinDecibels(double k,
                                     ExceptionState& exception_state) {
  if (k < MaxDecibels()) {
    analyser_.SetMinDecibels(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMaximumBound("minDecibels", k,
                                                    MaxDecibels()));
  }
}

void AnalyserHandler::SetMaxDecibels(double k,
                                     ExceptionState& exception_state) {
  if (k > MinDecibels()) {
    analyser_.SetMaxDecibels(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexExceedsMinimumBound("maxDecibels", k,
                                                    MinDecibels()));
  }
}

void AnalyserHandler::SetMinMaxDecibels(double min_decibels,
                                        double max_decibels,
                                        ExceptionState& exception_state) {
  if (min_decibels >= max_decibels) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "maxDecibels (" + String::Number(max_decibels) +
            ") must be greater than or equal to minDecibels " + "( " +
            String::Number(min_decibels) + ").");
    return;
  }
  analyser_.SetMinDecibels(min_decibels);
  analyser_.SetMaxDecibels(max_decibels);
}

void AnalyserHandler::SetSmoothingTimeConstant(
    double k,
    ExceptionState& exception_state) {
  if (k >= 0 && k <= 1) {
    analyser_.SetSmoothingTimeConstant(k);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "smoothing value", k, 0.0, ExceptionMessages::kInclusiveBound, 1.0,
            ExceptionMessages::kInclusiveBound));
  }
}

void AnalyserHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  if (Output(0).IsConnected()) {
    // When an AnalyserHandler is connected to a downstream node, it will get
    // pulled by the downstream node, thus remove it from the context's
    // automatic pull list.
    if (need_automatic_pull_) {
      Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
      need_automatic_pull_ = false;
    }
  } else {
    unsigned number_of_input_connections =
        Input(0).NumberOfRenderingConnections();
    // When an AnalyserHandler is not connected to any downstream node while
    // still connected from upstream node(s), add it to the context's automatic
    // pull list.
    //
    // But don't remove the AnalyserHandler if there are no inputs connected to
    // the node.  The node needs to be pulled so that the internal state is
    // updated with the correct input signal (of zeroes).
    if (number_of_input_connections && !need_automatic_pull_) {
      Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
      need_automatic_pull_ = true;
    }
  }
}

bool AnalyserHandler::RequiresTailProcessing() const {
  // Tail time is always non-zero so tail processing is required.
  return true;
}

double AnalyserHandler::TailTime() const {
  return RealtimeAnalyser::kMaxFFTSize /
         static_cast<double>(Context()->sampleRate());
}

void AnalyserHandler::PullInputs(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  AudioBus* output_bus = Output(0).RenderingFanOutCount() > 0
      ? Output(0).Bus() : nullptr;

  Input(0).Pull(output_bus, frames_to_process);
}

void AnalyserHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &Input(0));

  unsigned number_of_channels = input->NumberOfChannels();

  if (number_of_channels != Output(0).NumberOfChannels()) {
    // This will propagate the channel count to any nodes connected further
    // downstream in the graph.
    Output(0).SetNumberOfChannels(number_of_channels);
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);

  UpdatePullStatusIfNeeded();
}

}  // namespace blink
