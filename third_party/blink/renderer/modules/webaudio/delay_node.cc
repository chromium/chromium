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

#include "third_party/blink/renderer/modules/webaudio/delay_node.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_delay_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/delay_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr double kDefaultDelayTimeValue = 0.0;
constexpr float kMinDelayTimeValue = 0.0f;

constexpr double kDefaultMaximumDelayTime = 1.0;  // 1 second
constexpr double kMaximumAllowedDelayTime = 180.0;

}  // namespace

DelayNode::DelayNode(BaseAudioContext& context, double max_delay_time)
    : AudioNode(context),
      delay_time_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeDelayDelayTime,
                             kDefaultDelayTimeValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             kMinDelayTimeValue,
                             max_delay_time)) {
  SetHandler(DelayHandler::Create(*this, context.sampleRate(),
                                  delay_time_->Handler(), max_delay_time));
}

DelayNode* DelayNode::Create(BaseAudioContext& context,
                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return Create(context, kDefaultMaximumDelayTime, exception_state);
}

DelayNode* DelayNode::Create(BaseAudioContext& context,
                             double max_delay_time,
                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (max_delay_time <= 0 || max_delay_time >= kMaximumAllowedDelayTime) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "max delay time", max_delay_time, 0.0,
            ExceptionMessages::kExclusiveBound, kMaximumAllowedDelayTime,
            ExceptionMessages::kExclusiveBound));
    return nullptr;
  }

  return MakeGarbageCollected<DelayNode>(context, max_delay_time);
}

DelayNode* DelayNode::Create(BaseAudioContext* context,
                             const DelayOptions* options,
                             ExceptionState& exception_state) {
  // maxDelayTime has a default value specified.
  DelayNode* node = Create(*context, options->maxDelayTime(), exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->delayTime()->setValue(options->delayTime());

  return node;
}

AudioParam* DelayNode::delayTime() {
  return delay_time_.Get();
}

void DelayNode::Trace(Visitor* visitor) const {
  visitor->Trace(delay_time_);
  AudioNode::Trace(visitor);
}

void DelayNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(delay_time_);
}

void DelayNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(delay_time_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
