// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/iir_filter_node.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_iir_filter_options.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

IIRFilterHandler::IIRFilterHandler(AudioNode& node,
                                   float sample_rate,
                                   const Vector<double>& feedforward_coef,
                                   const Vector<double>& feedback_coef,
                                   bool is_filter_stable)
    : AudioBasicProcessorHandler(
          kNodeTypeIIRFilter,
          node,
          sample_rate,
          std::make_unique<IIRProcessor>(sample_rate,
                                         1,
                                         feedforward_coef,
                                         feedback_coef,
                                         is_filter_stable)) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);
}

scoped_refptr<IIRFilterHandler> IIRFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    const Vector<double>& feedforward_coef,
    const Vector<double>& feedback_coef,
    bool is_filter_stable) {
  return base::AdoptRef(new IIRFilterHandler(
      node, sample_rate, feedforward_coef, feedback_coef, is_filter_stable));
}

// Determine if filter is stable based on the feedback coefficients.
// We compute the reflection coefficients for the filter.  If, at any
// point, the magnitude of the reflection coefficient is greater than
// or equal to 1, the filter is declared unstable.
//
// Let A(z) be the feedback polynomial given by
//   A[n](z) = 1 + a[1]/z + a[2]/z^2 + ... + a[n]/z^n
//
// The first reflection coefficient k[n] = a[n].  Then, recursively compute
//
//   A[n-1](z) = (A[n](z) - k[n]*A[n](1/z)/z^n)/(1-k[n]^2);
//
// stopping at A[1](z).  If at any point |k[n]| >= 1, the filter is
// unstable.
static bool IsFilterStable(const Vector<double>& feedback_coef) {
  // Make a copy of the feedback coefficients
  Vector<double> coef(feedback_coef);
  int order = coef.size() - 1;

  // If necessary, normalize filter coefficients so that constant term is 1.
  if (coef[0] != 1) {
    for (int m = 1; m <= order; ++m)
      coef[m] /= coef[0];
    coef[0] = 1;
  }

  // Begin recursion, using a work array to hold intermediate results.
  Vector<double> work(order + 1);
  for (int n = order; n >= 1; --n) {
    double k = coef[n];

    if (std::fabs(k) >= 1)
      return false;

    // Note that A[n](1/z)/z^n is basically the coefficients of A[n]
    // in reverse order.
    double factor = 1 - k * k;
    for (int m = 0; m <= n; ++m)
      work[m] = (coef[m] - k * coef[n - m]) / factor;
    coef.swap(work);
  }

  return true;
}

void IIRFilterHandler::Process(uint32_t frames_to_process) {
  AudioBasicProcessorHandler::Process(frames_to_process);

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&IIRFilterHandler::NotifyBadState, AsWeakPtr()));
    }
  }
}

void IIRFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext())
    return;

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          NodeTypeName() + ": state is bad, probably due to unstable filter."));
}

IIRFilterNode::IIRFilterNode(BaseAudioContext& context,
                             const Vector<double>& feedforward_coef,
                             const Vector<double>& feedback_coef,
                             bool is_filter_stable)
    : AudioNode(context) {
  SetHandler(IIRFilterHandler::Create(*this, context.sampleRate(),
                                      feedforward_coef, feedback_coef,
                                      is_filter_stable));

  // Histogram of the IIRFilter order.  createIIRFilter ensures that the length
  // of |feedbackCoef| is in the range [1, IIRFilter::kMaxOrder + 1].  The order
  // is one less than the length of this vector.
  base::UmaHistogramSparse("WebAudio.IIRFilterNode.Order",
                           feedback_coef.size() - 1);
}

IIRFilterNode* IIRFilterNode::Create(BaseAudioContext& context,
                                     const Vector<double>& feedforward_coef,
                                     const Vector<double>& feedback_coef,
                                     ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // TODO(crbug.com/1055983): Remove this when the execution context validity
  // check is not required in the AudioNode factory methods.
  if (!context.CheckExecutionContextAndThrowIfNecessary(exception_state))
    return nullptr;

  if (feedback_coef.size() == 0 ||
      (feedback_coef.size() > IIRFilter::kMaxOrder + 1)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of feedback coefficients", feedback_coef.size(), 1,
            ExceptionMessages::kInclusiveBound, IIRFilter::kMaxOrder + 1,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (feedforward_coef.size() == 0 ||
      (feedforward_coef.size() > IIRFilter::kMaxOrder + 1)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<size_t>(
            "number of feedforward coefficients", feedforward_coef.size(), 1,
            ExceptionMessages::kInclusiveBound, IIRFilter::kMaxOrder + 1,
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (feedback_coef[0] == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "First feedback coefficient cannot be zero.");
    return nullptr;
  }

  bool has_non_zero_coef = false;

  for (wtf_size_t k = 0; k < feedforward_coef.size(); ++k) {
    if (feedforward_coef[k] != 0) {
      has_non_zero_coef = true;
      break;
    }
  }

  if (!has_non_zero_coef) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "At least one feedforward coefficient must be non-zero.");
    return nullptr;
  }

  bool is_filter_stable = IsFilterStable(feedback_coef);
  if (!is_filter_stable) {
    StringBuilder message;
    message.Append("Unstable IIRFilter with feedback coefficients: [");
    message.AppendNumber(feedback_coef[0]);
    for (wtf_size_t k = 1; k < feedback_coef.size(); ++k) {
      message.Append(", ");
      message.AppendNumber(feedback_coef[k]);
    }
    message.Append(']');

    context.GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning, message.ToString()));
  }

  return MakeGarbageCollected<IIRFilterNode>(context, feedforward_coef,
                                             feedback_coef, is_filter_stable);
}

IIRFilterNode* IIRFilterNode::Create(BaseAudioContext* context,
                                     const IIRFilterOptions* options,
                                     ExceptionState& exception_state) {
  IIRFilterNode* node = Create(*context, options->feedforward(),
                               options->feedback(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  return node;
}

void IIRFilterNode::Trace(Visitor* visitor) const {
  AudioNode::Trace(visitor);
}

IIRProcessor* IIRFilterNode::GetIIRFilterProcessor() const {
  return static_cast<IIRProcessor*>(
      static_cast<IIRFilterHandler&>(Handler()).Processor());
}

void IIRFilterNode::getFrequencyResponse(
    NotShared<const DOMFloat32Array> frequency_hz,
    NotShared<DOMFloat32Array> mag_response,
    NotShared<DOMFloat32Array> phase_response,
    ExceptionState& exception_state) {
  size_t frequency_hz_length = frequency_hz.View()->length();

  // All the arrays must have the same length.  Just verify that all
  // the arrays have the same length as the |frequency_hz| array.
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

  // Nothing to do if the length is 0.
  if (frequency_hz_length_as_int > 0) {
    GetIIRFilterProcessor()->GetFrequencyResponse(
        frequency_hz_length_as_int, frequency_hz.View()->Data(),
        mag_response.View()->Data(), phase_response.View()->Data());
  }
}

void IIRFilterNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void IIRFilterNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
