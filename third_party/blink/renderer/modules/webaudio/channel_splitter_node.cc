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

#include "third_party/blink/renderer/bindings/modules/v8/v8_channel_splitter_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/channel_splitter_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// Default number of outputs for the splitter node is 6.
constexpr unsigned kDefaultNumberOfOutputs = 6;

}  // namespace

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

  return Create(context, kDefaultNumberOfOutputs, exception_state);
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

  if (!node) {
    return nullptr;
  }

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
