// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/channel_splitter_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfOutputChannels = 1;

}  // namespace

ChannelSplitterHandler::ChannelSplitterHandler(AudioNode& node,
                                               float sample_rate,
                                               unsigned number_of_outputs)
    : AudioHandler(kNodeTypeChannelSplitter, node, sample_rate) {
  // These properties are fixed and cannot be changed by the user.
  channel_count_ = number_of_outputs;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);
  SetInternalChannelInterpretation(AudioBus::kDiscrete);
  AddInput();

  // Create a fixed number of outputs (able to handle the maximum number of
  // channels fed to an input).
  for (unsigned i = 0; i < number_of_outputs; ++i) {
    AddOutput(kNumberOfOutputChannels);
  }

  Initialize();
}

scoped_refptr<ChannelSplitterHandler> ChannelSplitterHandler::Create(
    AudioNode& node,
    float sample_rate,
    unsigned number_of_outputs) {
  return base::AdoptRef(
      new ChannelSplitterHandler(node, sample_rate, number_of_outputs));
}

void ChannelSplitterHandler::Process(uint32_t frames_to_process) {
  scoped_refptr<AudioBus> source = Input(0).Bus();
  DCHECK(source);
  DCHECK_EQ(frames_to_process, source->length());

  unsigned number_of_source_channels = source->NumberOfChannels();

  for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
    AudioBus* destination = Output(i).Bus();
    DCHECK(destination);

    if (i < number_of_source_channels) {
      // Split the channel out if it exists in the source.
      // It would be nice to avoid the copy and simply pass along pointers, but
      // this becomes extremely difficult with fanout and fanin.
      destination->Channel(0)->CopyFrom(source->Channel(i));
    } else if (Output(i).RenderingFanOutCount() > 0) {
      // Only bother zeroing out the destination if it's connected to anything
      destination->Zero();
    }
  }
}

void ChannelSplitterHandler::SetChannelCount(unsigned channel_count,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channelCount cannot be changed from the number of outputs.
  if (channel_count != NumberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelSplitter: channelCount cannot be changed from " +
            String::Number(NumberOfOutputs()));
  }
}

void ChannelSplitterHandler::SetChannelCountMode(
    V8ChannelCountMode::Enum mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'explicit'.
  if (mode != V8ChannelCountMode::Enum::kExplicit) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelSplitter: channelCountMode cannot be changed from 'explicit'");
  }
}

void ChannelSplitterHandler::SetChannelInterpretation(
    V8ChannelInterpretation::Enum mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channelInterpretation must be "discrete"
  if (mode != V8ChannelInterpretation::Enum::kDiscrete) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ChannelSplitter: channelInterpretation "
                                      "cannot be changed from 'discrete'");
  }
}

}  // namespace blink
