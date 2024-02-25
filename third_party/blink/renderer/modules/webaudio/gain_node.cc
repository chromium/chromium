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

#include "third_party/blink/renderer/modules/webaudio/gain_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gain_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/gain_handler.h"

namespace blink {

namespace {

constexpr double kDefaultGainValue = 1.0;

}  // namespace

GainNode::GainNode(BaseAudioContext& context)
    : AudioNode(context),
      gain_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeGainGain,
          kDefaultGainValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(
      GainHandler::Create(*this, context.sampleRate(), gain_->Handler()));
}

GainNode* GainNode::Create(BaseAudioContext& context,
                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<GainNode>(context);
}

GainNode* GainNode::Create(BaseAudioContext* context,
                           const GainOptions* options,
                           ExceptionState& exception_state) {
  GainNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->gain()->setValue(options->gain());

  return node;
}

AudioParam* GainNode::gain() const {
  return gain_.Get();
}

void GainNode::Trace(Visitor* visitor) const {
  visitor->Trace(gain_);
  AudioNode::Trace(visitor);
}

void GainNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(gain_);
}

void GainNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(gain_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
