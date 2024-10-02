// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/channel_merger_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfInputChannels = 1;

}  // namespace

ChannelMergerHandler::ChannelMergerHandler(AudioNode& node,
                                           float sample_rate,
                                           unsigned number_of_inputs)
    : AudioHandler(kNodeTypeChannelMerger, node, sample_rate) {
  // These properties are fixed for the node and cannot be changed by user.
  channel_count_ = kNumberOfInputChannels;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);

  // Create the requested number of inputs.
  for (unsigned i = 0; i < number_of_inputs; ++i) {
    AddInput();
  }

  // Create the output with the requested number of channels.
  AddOutput(number_of_inputs);

  Initialize();

  // Until something is connected, we're not actively processing, so disable
  // outputs so that we produce a single channel of silence.  The graph lock is
  // needed to be able to disable outputs.
  DeferredTaskHandler::GraphAutoLocker context_locker(Context());

  DisableOutputs();
}

scoped_refptr<ChannelMergerHandler> ChannelMergerHandler::Create(
    AudioNode& node,
    float sample_rate,
    unsigned number_of_inputs) {
  return base::AdoptRef(
      new ChannelMergerHandler(node, sample_rate, number_of_inputs));
}

void ChannelMergerHandler::Process(uint32_t frames_to_process) {
  AudioNodeOutput& output = Output(0);
  DCHECK_EQ(frames_to_process, output.Bus()->length());

  unsigned number_of_output_channels = output.NumberOfChannels();
  DCHECK_EQ(NumberOfInputs(), number_of_output_channels);

  // Merge multiple inputs into one output.
  for (unsigned i = 0; i < number_of_output_channels; ++i) {
    AudioNodeInput& input = Input(i);
    DCHECK_EQ(input.NumberOfChannels(), 1u);
    AudioChannel* output_channel = output.Bus()->Channel(i);
    if (input.IsConnected()) {
      // The mixing rules will be applied so multiple channels are down-
      // mixed to mono (when the mixing rule is defined). Note that only
      // the first channel will be taken for the undefined input channel
      // layout.
      //
      // See:
      // http://webaudio.github.io/web-audio-api/#channel-up-mixing-and-down-mixing
      AudioChannel* input_channel = input.Bus()->Channel(0);
      output_channel->CopyFrom(input_channel);

    } else {
      // If input is unconnected, fill zeros in the channel.
      output_channel->Zero();
    }
  }
}

void ChannelMergerHandler::SetChannelCount(unsigned channel_count,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channelCount must be 1.
  if (channel_count != 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelMerger: channelCount cannot be changed from 1");
  }
}

void ChannelMergerHandler::SetChannelCountMode(
    V8ChannelCountMode::Enum mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'explicit'.
  if (mode != V8ChannelCountMode::Enum::kExplicit) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelMerger: channelCountMode cannot be changed from 'explicit'");
  }
}

}  // namespace blink
