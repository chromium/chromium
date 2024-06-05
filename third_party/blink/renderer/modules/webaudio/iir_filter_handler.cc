// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/iir_filter_handler.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/iir_processor.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr uint32_t kNumberOfChannels = 1;

}  // namespace

IIRFilterHandler::IIRFilterHandler(AudioNode& node,
                                   float sample_rate,
                                   const Vector<double>& feedforward_coef,
                                   const Vector<double>& feedback_coef,
                                   bool is_filter_stable)
    : AudioBasicProcessorHandler(
          kNodeTypeIIRFilter,
          node,
          sample_rate,
          std::make_unique<IIRProcessor>(
              sample_rate,
              kNumberOfChannels,
              node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
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

void IIRFilterHandler::Process(uint32_t frames_to_process) {
  AudioBasicProcessorHandler::Process(frames_to_process);

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(*task_runner_, FROM_HERE,
                          CrossThreadBindOnce(&IIRFilterHandler::NotifyBadState,
                                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void IIRFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          NodeTypeName() + ": state is bad, probably due to unstable filter."));
}

}  // namespace blink
