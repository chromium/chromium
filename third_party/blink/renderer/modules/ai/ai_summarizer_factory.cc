// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_factory.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"

namespace blink {

namespace {

class CreateSummarizerClient final
    : public AIWritingAssistanceCreateClient<
          mojom::blink::AISummarizer,
          mojom::blink::AIManagerCreateSummarizerClient,
          AISummarizerCreateOptions,
          AISummarizer> {
 public:
  CreateSummarizerClient(ScriptState* script_state,
                         AI* ai,
                         ScriptPromiseResolver<AISummarizer>* resolver,
                         AISummarizerCreateOptions* options)
      : AIWritingAssistanceCreateClient(script_state, ai, resolver, options) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(
          ai->GetExecutionContext(), ai->GetTaskRunner());
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
      ai->GetAIRemote()->AddModelDownloadProgressObserver(
          monitor_->BindRemote());
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(monitor_);
    AIWritingAssistanceCreateClient::Trace(visitor);
  }

  // AIWritingAssistanceCreateClient:
  void RemoteCreate(
      mojo::PendingRemote<mojom::blink::AIManagerCreateSummarizerClient>
          client_remote) override {
    ai_->GetAIRemote()->CreateSummarizer(
        std::move(client_remote), ToMojoSummarizerCreateOptions(options_));
  }

 private:
  Member<AICreateMonitor> monitor_;
};

}  // namespace

AISummarizerFactory::AISummarizerFactory(
    AI* ai,
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context), ai_(ai), task_runner_(task_runner) {}

void AISummarizerFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<V8AIAvailability> AISummarizerFactory::availability(
    ScriptState* script_state,
    AISummarizerCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AIAvailability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIAvailability>>(
          script_state);
  auto promise = resolver->Promise();
  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  ai_->GetAIRemote()->CanCreateSummarizer(
      ToMojoSummarizerCreateOptions(options),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8AIAvailability>* resolver,
             AISummarizerFactory* factory,
             mojom::blink::ModelAvailabilityCheckResult result) {
            AIAvailability availability = HandleModelAvailabilityCheckResult(
                factory->GetExecutionContext(),
                AIMetrics::AISessionType::kSummarizer, result);
            resolver->Resolve(AIAvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(this)));
  return promise;
}

ScriptPromise<AISummarizer> AISummarizerFactory::create(
    ScriptState* script_state,
    AISummarizerCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AISummarizer>();
  }
  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kSummarizer),
      AIMetrics::AIAPI::kSummarizerCreate);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AISummarizer>>(script_state);
  auto promise = resolver->Promise();

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<CreateSummarizerClient>(script_state, ai_, resolver,
                                               options)
      ->Create();
  return promise;
}

}  // namespace blink
