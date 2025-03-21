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
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_create_monitor.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

class CreateSummarizerClient
    : public GarbageCollected<CreateSummarizerClient>,
      public AIContextObserver<AISummarizer>,
      public mojom::blink::AIManagerCreateSummarizerClient {
 public:
  explicit CreateSummarizerClient(
      ScriptState* script_state,
      AI* ai,
      ScriptPromiseResolver<AISummarizer>* resolver,
      AbortSignal* signal,
      AISummarizerCreateOptions* options,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : AIContextObserver(script_state, ai, resolver, signal),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        options_(options) {
    if (options->hasMonitor()) {
      monitor_ = MakeGarbageCollected<AICreateMonitor>(
          ai->GetExecutionContext(), task_runner);
      std::ignore = options->monitor()->Invoke(nullptr, monitor_);
      ai_->GetAIRemote()->AddModelDownloadProgressObserver(
          monitor_->BindRemote());
    }
  }

  ~CreateSummarizerClient() override = default;

  void CreateSummarizer() {
    mojo::PendingRemote<mojom::blink::AIManagerCreateSummarizerClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai_->GetTaskRunner());
    ai_->GetAIRemote()->CreateSummarizer(
        std::move(client_remote), ToMojoSummarizerCreateOptions(options_));
  }

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
    visitor->Trace(options_);
    visitor->Trace(monitor_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AISummarizer>
                    remote_summarizer) override {
    if (!GetResolver()) {
      // The creation was aborted by the user.
      return;
    }
    if (!ai_->GetExecutionContext() || !remote_summarizer) {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    } else {
      AISummarizer* summarizer = MakeGarbageCollected<AISummarizer>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(),
          std::move(remote_summarizer), options_);
      GetResolver()->Resolve(summarizer);
    }
    Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
    if (!GetResolver()) {
      return;
    }

    using mojom::blink::AIManagerCreateClientError;

    switch (error) {
      case AIManagerCreateClientError::kUnableToCreateSession:
      case AIManagerCreateClientError::kUnableToCalculateTokenSize: {
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kInvalidStateError,
            kExceptionMessageUnableToCreateSession);
        break;
      }
      case AIManagerCreateClientError::kInitialInputTooLarge: {
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kQuotaExceededError,
            kExceptionMessageInputTooLarge);
        break;
      }
      case AIManagerCreateClientError::kUnsupportedLanguage: {
        GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kNotSupportedError,
            kExceptionMessageUnsupportedLanguages);
        break;
      }
    }
    Cleanup();
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AI> ai_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateSummarizerClient,
                   CreateSummarizerClient>
      receiver_;

  Member<AISummarizerCreateOptions> options_;
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

  MakeGarbageCollected<CreateSummarizerClient>(
      script_state, ai_.Get(), resolver, signal, options, task_runner_)
      ->CreateSummarizer();
  return promise;
}

}  // namespace blink
