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

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_biquad_filter_options.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

BiquadFilterHandler::BiquadFilterHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& frequency,
                                         AudioParamHandler& q,
                                         AudioParamHandler& gain,
                                         AudioParamHandler& detune)
    : AudioBasicProcessorHandler(kNodeTypeBiquadFilter,
                                 node,
                                 sample_rate,
                                 std::make_unique<BiquadProcessor>(sample_rate,
                                                                   1,
                                                                   frequency,
                                                                   q,
                                                                   gain,
                                                                   detune)) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);

  // Initialize the handler so that AudioParams can be processed.
  Initialize();
}

scoped_refptr<BiquadFilterHandler> BiquadFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& frequency,
    AudioParamHandler& q,
    AudioParamHandler& gain,
    AudioParamHandler& detune) {
  return base::AdoptRef(
      new BiquadFilterHandler(node, sample_rate, frequency, q, gain, detune));
}

void BiquadFilterHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "BiquadFilterHandler::Process");

  AudioBasicProcessorHandler::Process(frames_to_process);

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&BiquadFilterHandler::NotifyBadState,
                              AsWeakPtr()));
    }
  }
}

void BiquadFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext())
    return;

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          NodeTypeName() +
              ": state is bad, probably due to unstable filter caused "
              "by fast parameter automation."));
}

BiquadFilterNode::BiquadFilterNode(BaseAudioContext& context)
    : AudioNode(context),
      frequency_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeBiquadFilterFrequency,
                             350.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             0,
                             context.sampleRate() / 2)),
      q_(AudioParam::Create(context,
                            Uuid(),
                            AudioParamHandler::kParamTypeBiquadFilterQ,
                            1.0,
                            AudioParamHandler::AutomationRate::kAudio,
                            AudioParamHandler::AutomationRateMode::kVariable)),
      gain_(AudioParam::Create(context,
                               Uuid(),
                               AudioParamHandler::kParamTypeBiquadFilterGain,
                               0.0,
                               AudioParamHandler::AutomationRate::kAudio,
                               AudioParamHandler::AutomationRateMode::kVariable,
                               std::numeric_limits<float>::lowest(),
                               40 * log10f(std::numeric_limits<float>::max()))),
      detune_(
          AudioParam::Create(context,
                             Uuid(),
                             AudioParamHandler::kParamTypeBiquadFilterDetune,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable,
                             -1200 * log2f(std::numeric_limits<float>::max()),
                             1200 * log2f(std::numeric_limits<float>::max()))) {
  SetHandler(BiquadFilterHandler::Create(*this, context.sampleRate(),
                                         frequency_->Handler(), q_->Handler(),
                                         gain_->Handler(), detune_->Handler()));

  setType("lowpass");
}

BiquadFilterNode* BiquadFilterNode::Create(BaseAudioContext& context,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state))
    return nullptr;

  return MakeGarbageCollected<BiquadFilterNode>(context);
}

BiquadFilterNode* BiquadFilterNode::Create(BaseAudioContext* context,
                                           const BiquadFilterOptions* options,
                                           ExceptionState& exception_state) {
  BiquadFilterNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

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

String BiquadFilterNode::type() const {
  switch (
      const_cast<BiquadFilterNode*>(this)->GetBiquadProcessor()->GetType()) {
    case BiquadProcessor::FilterType::kLowPass:
      return "lowpass";
    case BiquadProcessor::FilterType::kHighPass:
      return "highpass";
    case BiquadProcessor::FilterType::kBandPass:
      return "bandpass";
    case BiquadProcessor::FilterType::kLowShelf:
      return "lowshelf";
    case BiquadProcessor::FilterType::kHighShelf:
      return "highshelf";
    case BiquadProcessor::FilterType::kPeaking:
      return "peaking";
    case BiquadProcessor::FilterType::kNotch:
      return "notch";
    case BiquadProcessor::FilterType::kAllpass:
      return "allpass";
  }
  NOTREACHED();
  return "lowpass";
}

void BiquadFilterNode::setType(const String& type) {
  if (type == "lowpass") {
    SetType(BiquadProcessor::FilterType::kLowPass);
  } else if (type == "highpass") {
    SetType(BiquadProcessor::FilterType::kHighPass);
  } else if (type == "bandpass") {
    SetType(BiquadProcessor::FilterType::kBandPass);
  } else if (type == "lowshelf") {
    SetType(BiquadProcessor::FilterType::kLowShelf);
  } else if (type == "highshelf") {
    SetType(BiquadProcessor::FilterType::kHighShelf);
  } else if (type == "peaking") {
    SetType(BiquadProcessor::FilterType::kPeaking);
  } else if (type == "notch") {
    SetType(BiquadProcessor::FilterType::kNotch);
  } else if (type == "allpass") {
    SetType(BiquadProcessor::FilterType::kAllpass);
  }
}

bool BiquadFilterNode::SetType(BiquadProcessor::FilterType type) {
  if (type > BiquadProcessor::FilterType::kAllpass)
    return false;

  base::UmaHistogramEnumeration("WebAudio.BiquadFilter.Type", type);

  GetBiquadProcessor()->SetType(type);
  return true;
}

void BiquadFilterNode::getFrequencyResponse(
    NotShared<const DOMFloat32Array> frequency_hz,
    NotShared<DOMFloat32Array> mag_response,
    NotShared<DOMFloat32Array> phase_response,
    ExceptionState& exception_state) {
  size_t frequency_hz_length = frequency_hz.View()->length();

  if (mag_response.View()->length() != frequency_hz_length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        ExceptionMessages::IndexOutsideRange(
            "magResponse length", mag_response.View()->length(),
            frequency_hz_length, ExceptionMessages::kInclusiveBound,
            frequency_hz_length, ExceptionMessages::kInclusiveBound));
    return;
  }

  if (phase_response.View()->length() != frequency_hz_length) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        ExceptionMessages::IndexOutsideRange(
            "phaseResponse length", phase_response.View()->length(),
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
        frequency_hz_length_as_int, frequency_hz.View()->Data(),
        mag_response.View()->Data(), phase_response.View()->Data());
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
