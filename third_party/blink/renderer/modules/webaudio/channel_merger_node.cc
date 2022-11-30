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

#include "third_party/blink/renderer/bindings/modules/v8/v8_channel_merger_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/channel_merger_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// The default number of inputs for the merger node is 6.
constexpr unsigned kDefaultNumberOfInputs = 6;

}  // namespace

ChannelMergerNode::ChannelMergerNode(BaseAudioContext& context,
                                     unsigned number_of_inputs)
    : AudioNode(context) {
  SetHandler(ChannelMergerHandler::Create(*this, context.sampleRate(),
                                          number_of_inputs));
}

ChannelMergerNode* ChannelMergerNode::Create(BaseAudioContext& context,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return Create(context, kDefaultNumberOfInputs, exception_state);
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

  if (!node) {
    return nullptr;
  }

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
