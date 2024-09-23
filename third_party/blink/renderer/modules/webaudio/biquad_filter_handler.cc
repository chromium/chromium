// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_processor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr uint32_t kNumberOfChannels = 1;

}  // namespace

BiquadFilterHandler::BiquadFilterHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& frequency,
                                         AudioParamHandler& q,
                                         AudioParamHandler& gain,
                                         AudioParamHandler& detune)
    : AudioBasicProcessorHandler(
          kNodeTypeBiquadFilter,
          node,
          sample_rate,
          std::make_unique<BiquadProcessor>(
              sample_rate,
              kNumberOfChannels,
              node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
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
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void BiquadFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          NodeTypeName() +
              ": state is bad, probably due to unstable filter caused "
              "by fast parameter automation."));
}

}  // namespace blink
