/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_node.h"

#include <limits>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_biquad_filter_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_biquad_filter_type.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

constexpr double kDefaultFrequencyValue = 350.0;
constexpr float kMinFrequencyValue = 0.0f;
constexpr double kDefaultQValue = 1.0;
constexpr double kDefaultGainValue = 0.0;
constexpr float kMinGainValue = std::numeric_limits<float>::lowest();
constexpr double kDefaultDetuneValue = 0.0;

}  // namespace

BiquadFilterNode::BiquadFilterNode(BaseAudioContext& context)
    : AudioNode(context),
      frequency_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeBiquadFilterFrequency,
                             kDefaultFrequencyValue,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             kMinFrequencyValue,
                             /*max_value=*/context.sampleRate() / 2)),
      q_(AudioParam::Create(context,
                            Uuid(),
                            AudioParamHandler::kParamTypeBiquadFilterQ,
                            kDefaultQValue,
                            AudioParamHandler::AutomationRate::kAudio,
                            AudioParamHandler::AutomationRateMode::kVariable)),
      gain_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeBiquadFilterGain,
          kDefaultGainValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable,
          kMinGainValue,
          /*max_value=*/40 * log10f(std::numeric_limits<float>::max()))),
      detune_(AudioParam::Create(
          context,
          Uuid(),
          AudioParamHandler::kParamTypeBiquadFilterDetune,
          kDefaultDetuneValue,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable,
          /*min_value=*/-1200 * log2f(std::numeric_limits<float>::max()),
          /*max_value=*/1200 * log2f(std::numeric_limits<float>::max()))) {
  SetHandler(BiquadFilterHandler::Create(*this, context.sampleRate(),
                                         frequency_->Handler(), q_->Handler(),
                                         gain_->Handler(), detune_->Handler()));

  SetType(BiquadProcessor::FilterType::kLowPass);
}

BiquadFilterNode* BiquadFilterNode::Create(BaseAudioContext& context,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<BiquadFilterNode>(context);
}

BiquadFilterNode* BiquadFilterNode::Create(BaseAudioContext* context,
                                           const BiquadFilterOptions* options,
                                           ExceptionState& exception_state) {
  BiquadFilterNode* node = Create(*context, exception_state);

  if (!node) {
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  node->setType(options->type());
  node->q()->setValue(options->q());
  node->detune()->setValue(options->detune());
  node->frequency()->setValue(options->frequency());
  node->gain()->setValue(options->gain());

  return node;
}

void BiquadFilterNode::Trace(Visitor* visitor) const {
  visitor->Trace(frequency_);
  visitor->Trace(q_);
  visitor->Trace(gain_);
  visitor->Trace(detune_);
  AudioNode::Trace(visitor);
}

BiquadProcessor* BiquadFilterNode::GetBiquadProcessor() const {
  return static_cast<BiquadProcessor*>(
      static_cast<BiquadFilterHandler&>(Handler()).Processor());
}

V8BiquadFilterType BiquadFilterNode::type() const {
  switch (
      const_cast<BiquadFilterNode*>(this)->GetBiquadProcessor()->GetType()) {
    case BiquadProcessor::FilterType::kLowPass:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kLowpass);
    case BiquadProcessor::FilterType::kHighPass:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kHighpass);
    case BiquadProcessor::FilterType::kBandPass:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kBandpass);
    case BiquadProcessor::FilterType::kLowShelf:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kLowshelf);
    case BiquadProcessor::FilterType::kHighShelf:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kHighshelf);
    case BiquadProcessor::FilterType::kPeaking:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kPeaking);
    case BiquadProcessor::FilterType::kNotch:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kNotch);
    case BiquadProcessor::FilterType::kAllpass:
      return V8BiquadFilterType(V8BiquadFilterType::Enum::kAllpass);
  }
  NOTREACHED();
}

void BiquadFilterNode::setType(const V8BiquadFilterType& type) {
  switch (type.AsEnum()) {
    case V8BiquadFilterType::Enum::kLowpass:
      SetType(BiquadProcessor::FilterType::kLowPass);
      return;
    case V8BiquadFilterType::Enum::kHighpass:
      SetType(BiquadProcessor::FilterType::kHighPass);
      return;
    case V8BiquadFilterType::Enum::kBandpass:
      SetType(BiquadProcessor::FilterType::kBandPass);
      return;
    case V8BiquadFilterType::Enum::kLowshelf:
      SetType(BiquadProcessor::FilterType::kLowShelf);
      return;
    case V8BiquadFilterType::Enum::kHighshelf:
      SetType(BiquadProcessor::FilterType::kHighShelf);
      return;
    case V8BiquadFilterType::Enum::kPeaking:
      SetType(BiquadProcessor::FilterType::kPeaking);
      return;
    case V8BiquadFilterType::Enum::kNotch:
      SetType(BiquadProcessor::FilterType::kNotch);
      return;
    case V8BiquadFilterType::Enum::kAllpass:
      SetType(BiquadProcessor::FilterType::kAllpass);
      return;
  }
  NOTREACHED();
}

bool BiquadFilterNode::SetType(BiquadProcessor::FilterType type) {
  if (type > BiquadProcessor::FilterType::kAllpass) {
    return false;
  }

  base::UmaHistogramEnumeration("WebAudio.BiquadFilter.Type", type);

  GetBiquadProcessor()->SetType(type);
  return true;
}

void BiquadFilterNode::getFrequencyResponse(
    NotShared<const DOMFloat32Array> frequency_hz,
    NotShared<DOMFloat32Array> mag_response,
    NotShared<DOMFloat32Array> phase_response,
    ExceptionState& exception_state) {
  size_t frequency_hz_length = frequency_hz->length();

  if (mag_response->length() != frequency_hz_length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        ExceptionMessages::IndexOutsideRange(
            "magResponse length", mag_response->length(), frequency_hz_length,
            ExceptionMessages::kInclusiveBound, frequency_hz_length,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  if (phase_response->length() != frequency_hz_length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        ExceptionMessages::IndexOutsideRange(
            "phaseResponse length", phase_response->length(),
            frequency_hz_length, ExceptionMessages::kInclusiveBound,
            frequency_hz_length, ExceptionMessages::kInclusiveBound));
    return;
  }

  int frequency_hz_length_as_int;
  if (!base::CheckedNumeric<int>(frequency_hz_length)
           .AssignIfValid(&frequency_hz_length_as_int)) {
    exception_state.ThrowRangeError(
        "frequencyHz length exceeds the maximum supported length");
    return;
  }

  // If the length is 0, there's nothing to do.
  if (frequency_hz_length_as_int > 0) {
    GetBiquadProcessor()->GetFrequencyResponse(
        frequency_hz_length_as_int, frequency_hz->Data(), mag_response->Data(),
        phase_response->Data());
  }
}

void BiquadFilterNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
  GraphTracer().DidCreateAudioParam(detune_);
  GraphTracer().DidCreateAudioParam(frequency_);
  GraphTracer().DidCreateAudioParam(gain_);
  GraphTracer().DidCreateAudioParam(q_);
}

void BiquadFilterNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioParam(detune_);
  GraphTracer().WillDestroyAudioParam(frequency_);
  GraphTracer().WillDestroyAudioParam(gain_);
  GraphTracer().WillDestroyAudioParam(q_);
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
