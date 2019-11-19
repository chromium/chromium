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

#include "third_party/blink/renderer/modules/webaudio/channel_splitter_node.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/channel_splitter_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

ChannelSplitterHandler::ChannelSplitterHandler(AudioNode& node,
                                               float sample_rate,
                                               unsigned number_of_outputs)
    : AudioHandler(kNodeTypeChannelSplitter, node, sample_rate) {
  // These properties are fixed and cannot be changed by the user.
  channel_count_ = number_of_outputs;
  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kDiscrete);
  AddInput();

  // Create a fixed number of outputs (able to handle the maximum number of
  // channels fed to an input).
  for (unsigned i = 0; i < number_of_outputs; ++i)
    AddOutput(1);

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
  AudioBus* source = Input(0).Bus();
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
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channelCount cannot be changed from the number of outputs.
  if (channel_count != NumberOfOutputs()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelSplitter: channelCount cannot be changed from " +
            String::Number(NumberOfOutputs()));
  }
}

void ChannelSplitterHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'explicit'.
  if (mode != "explicit") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "ChannelSplitter: channelCountMode cannot be changed from 'explicit'");
  }
}

void ChannelSplitterHandler::SetChannelInterpretation(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channelInterpretation must be "discrete"
  if (mode != "discrete") {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ChannelSplitter: channelInterpretation "
                                      "cannot be changed from 'discrete'");
  }
}

// ----------------------------------------------------------------

ChannelSplitterNode::ChannelSplitterNode(BaseAudioContext& context,
                                         unsigned number_of_outputs)
    : AudioNode(context) {
  SetHandler(ChannelSplitterHandler::Create(*this, context.sampleRate(),
                                            number_of_outputs));
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default number of outputs for the splitter node is 6.
  return Create(context, 6, exception_state);
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext& context,
    unsigned number_of_outputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (!number_of_outputs ||
      number_of_outputs > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of outputs", number_of_outputs, 1,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  return MakeGarbageCollected<ChannelSplitterNode>(context, number_of_outputs);
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext* context,
    const ChannelSplitterOptions* options,
    ExceptionState& exception_state) {
  ChannelSplitterNode* node =
      Create(*context, options->numberOfOutputs(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  return node;
}

void ChannelSplitterNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ChannelSplitterNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
