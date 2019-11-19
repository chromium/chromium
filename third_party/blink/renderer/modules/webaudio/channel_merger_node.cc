/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/channel_merger_node.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/channel_merger_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

ChannelMergerHandler::ChannelMergerHandler(AudioNode& node,
                                           float sample_rate,
                                           unsigned number_of_inputs)
    : AudioHandler(kNodeTypeChannelMerger, node, sample_rate) {
  // These properties are fixed for the node and cannot be changed by user.
  channel_count_ = 1;
  SetInternalChannelCountMode(kExplicit);

  // Create the requested number of inputs.
  for (unsigned i = 0; i < number_of_inputs; ++i)
    AddInput();

  // Create the output with the requested number of channels.
  AddOutput(number_of_inputs);

  Initialize();

  // Until something is connected, we're not actively processing, so disable
  // outputs so that we produce a single channel of silence.  The graph lock is
  // needed to be able to disable outputs.
  BaseAudioContext::GraphAutoLocker context_locker(Context());

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
  AudioNodeOutput& output = this->Output(0);
  DCHECK_EQ(frames_to_process, output.Bus()->length());

  unsigned number_of_output_channels = output.NumberOfChannels();
  DCHECK_EQ(NumberOfInputs(), number_of_output_channels);

  // Merge multiple inputs into one output.
  for (unsigned i = 0; i < number_of_output_channels; ++i) {
    AudioNodeInput& input = this->Input(i);
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
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channelCount must be 1.
  if (channel_count != 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelMerger: channelCount cannot be changed from 1");
  }
}

void ChannelMergerHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'explicit'.
  if (mode != "explicit") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelMerger: channelCountMode cannot be changed from 'explicit'");
  }
}

// ----------------------------------------------------------------

ChannelMergerNode::ChannelMergerNode(BaseAudioContext& context,
                                     unsigned number_of_inputs)
    : AudioNode(context) {
  SetHandler(ChannelMergerHandler::Create(*this, context.sampleRate(),
                                          number_of_inputs));
}

ChannelMergerNode* ChannelMergerNode::Create(BaseAudioContext& context,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // The default number of inputs for the merger node is 6.
  return Create(context, 6, exception_state);
}

ChannelMergerNode* ChannelMergerNode::Create(BaseAudioContext& context,
                                             unsigned number_of_inputs,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!number_of_inputs ||
      number_of_inputs > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of inputs", number_of_inputs, 1,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  return MakeGarbageCollected<ChannelMergerNode>(context, number_of_inputs);
}

ChannelMergerNode* ChannelMergerNode::Create(
    BaseAudioContext* context,
    const ChannelMergerOptions* options,
    ExceptionState& exception_state) {
  ChannelMergerNode* node =
      Create(*context, options->numberOfInputs(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  return node;
}

void ChannelMergerNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ChannelMergerNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
