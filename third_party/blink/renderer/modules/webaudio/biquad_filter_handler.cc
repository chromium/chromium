// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_processor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr uint32_t kNumberOfChannels = 1;
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

BiquadFilterHandler::BiquadFilterHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& frequency,
                                         AudioParamHandler& q,
                                         AudioParamHandler& gain,
                                         AudioParamHandler& detune)
    : AudioHandler(NodeType::kNodeTypeBiquadFilter, node, sample_rate),
      processor_(std::make_unique<BiquadProcessor>(
          sample_rate,
          kNumberOfChannels,
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
          frequency,
          q,
          gain,
          detune)) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);

  // Initialize the handler so that AudioParams can be processed.
  Initialize();
}

scoped_refptr<BiquadFilterHandler> BiquadFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& frequency,
    AudioParamHandler& q,
    AudioParamHandler& gain,
    AudioParamHandler& detune) {
  return base::AdoptRef(
      new BiquadFilterHandler(node, sample_rate, frequency, q, gain, detune));
}

void BiquadFilterHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Initialize();

  AudioHandler::Initialize();
}

void BiquadFilterHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Uninitialize();

  AudioHandler::Uninitialize();
}

void BiquadFilterHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "BiquadFilterHandler::Process");

  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized() || !Processor() ||
      Processor()->NumberOfChannels() != NumberOfChannels()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    // FIXME: if we take "tail time" into account, then we can avoid calling
    // processor()->process() once the tail dies down.
    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    Processor()->Process(source_bus.get(), destination_bus, frames_to_process);
  }

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&BiquadFilterHandler::NotifyBadState,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void BiquadFilterHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized() || !Processor()) {
    return;
  }

  Processor()->ProcessOnlyAudioParams(frames_to_process);
}

// Nice optimization in the very common case allowing for "in-place" processing
void BiquadFilterHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - suggest to the input to render directly into output
  // bus for in-place processing in process() if possible.
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void BiquadFilterHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &Input(0));
  DCHECK(Processor());

  unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further down
    // the chain...
    Output(0).SetNumberOfChannels(number_of_channels);

    // Re-initialize the processor with the new channel count.
    Processor()->SetNumberOfChannels(number_of_channels);
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

unsigned BiquadFilterHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

bool BiquadFilterHandler::RequiresTailProcessing() const {
  return processor_->RequiresTailProcessing();
}

double BiquadFilterHandler::TailTime() const {
  return processor_->TailTime();
}

double BiquadFilterHandler::LatencyTime() const {
  return processor_->LatencyTime();
}

bool BiquadFilterHandler::HasNonFiniteOutput() const {
  AudioBus* output_bus = Output(0).Bus();

  for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
    AudioChannel* channel = output_bus->Channel(k);
    if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
      return true;
    }
  }

  return false;
}

void BiquadFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          NodeTypeName() +
              ": state is bad, probably due to unstable filter caused "
              "by fast parameter automation."));
}

}  // namespace blink
