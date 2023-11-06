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

#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_convolver_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/reverb.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

ConvolverNode::ConvolverNode(BaseAudioContext& context) : AudioNode(context) {
  SetHandler(ConvolverHandler::Create(*this, context.sampleRate()));
}

ConvolverNode* ConvolverNode::Create(BaseAudioContext& context,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<ConvolverNode>(context);
}

ConvolverNode* ConvolverNode::Create(BaseAudioContext* context,
                                     const ConvolverOptions* options,
                                     ExceptionState& exception_state) {
  ConvolverNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  // It is important to set normalize first because setting the buffer will
  // examing the normalize attribute to see if normalization needs to be done.
  node->setNormalize(!options->disableNormalization());
  if (options->hasBuffer()) {
    node->setBuffer(options->buffer(), exception_state);
  }
  return node;
}

ConvolverHandler& ConvolverNode::GetConvolverHandler() const {
  return static_cast<ConvolverHandler&>(Handler());
}

AudioBuffer* ConvolverNode::buffer() const {
  return buffer_.Get();
}

void ConvolverNode::setBuffer(AudioBuffer* new_buffer,
                              ExceptionState& exception_state) {
  GetConvolverHandler().SetBuffer(new_buffer, exception_state);
  if (!exception_state.HadException()) {
    buffer_ = new_buffer;
  }
}

bool ConvolverNode::normalize() const {
  return GetConvolverHandler().Normalize();
}

void ConvolverNode::setNormalize(bool normalize) {
  GetConvolverHandler().SetNormalize(normalize);
}

void ConvolverNode::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_);
  AudioNode::Trace(visitor);
}

void ConvolverNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ConvolverNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
