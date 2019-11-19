/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <memory>
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_processor.h"

namespace blink {

AudioBasicProcessorHandler::AudioBasicProcessorHandler(
    NodeType node_type,
    AudioNode& node,
    float sample_rate,
    std::unique_ptr<AudioProcessor> processor)
    : AudioHandler(node_type, node, sample_rate),
      processor_(std::move(processor)) {
  AddInput();
  AddOutput(1);
}

AudioBasicProcessorHandler::~AudioBasicProcessorHandler() {
  // Safe to call the uninitialize() because it's final.
  Uninitialize();
}

void AudioBasicProcessorHandler::Initialize() {
  if (IsInitialized())
    return;

  DCHECK(Processor());
  Processor()->Initialize();

  AudioHandler::Initialize();
}

void AudioBasicProcessorHandler::Uninitialize() {
  if (!IsInitialized())
    return;

  DCHECK(Processor());
  Processor()->Uninitialize();

  AudioHandler::Uninitialize();
}

void AudioBasicProcessorHandler::Process(uint32_t frames_to_process) {
  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized() || !Processor() ||
      Processor()->NumberOfChannels() != NumberOfChannels()) {
    destination_bus->Zero();
  } else {
    AudioBus* source_bus = Input(0).Bus();

    // FIXME: if we take "tail time" into account, then we can avoid calling
    // processor()->process() once the tail dies down.
    if (!Input(0).IsConnected())
      source_bus->Zero();

    Processor()->Process(source_bus, destination_bus, frames_to_process);
  }
}

void AudioBasicProcessorHandler::ProcessOnlyAudioParams(
    uint32_t frames_to_process) {
  if (!IsInitialized() || !Processor())
    return;

  Processor()->ProcessOnlyAudioParams(frames_to_process);
}

// Nice optimization in the very common case allowing for "in-place" processing
void AudioBasicProcessorHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - suggest to the input to render directly into output
  // bus for in-place processing in process() if possible.
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void AudioBasicProcessorHandler::CheckNumberOfChannelsForInput(
    AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &this->Input(0));
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

unsigned AudioBasicProcessorHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

bool AudioBasicProcessorHandler::RequiresTailProcessing() const {
  return processor_->RequiresTailProcessing();
}

double AudioBasicProcessorHandler::TailTime() const {
  return processor_->TailTime();
}

double AudioBasicProcessorHandler::LatencyTime() const {
  return processor_->LatencyTime();
}

bool AudioBasicProcessorHandler::HasNonFiniteOutput() const {
  AudioBus* output_bus = Output(0).Bus();

  for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
    AudioChannel* channel = output_bus->Channel(k);
    if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
      return true;
    }
  }

  return false;
}

}  // namespace blink
