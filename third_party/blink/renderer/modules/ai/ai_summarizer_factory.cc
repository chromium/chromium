// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_factory.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

class CreateSummarizerClient
    : public GarbageCollected<CreateSummarizerClient>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::AIManagerCreateSummarizerClient {
 public:
  explicit CreateSummarizerClient(AI* ai,
                                  ScriptPromiseResolver<AISummarizer>* resolver)
      : ExecutionContextLifecycleObserver(ai->GetExecutionContext()),
        ai_(ai),
        resolver_(resolver),
        receiver_(this, GetExecutionContext()) {}

  ~CreateSummarizerClient() override = default;

  void CreateSummarizer() {
    mojo::PendingRemote<mojom::blink::AIManagerCreateSummarizerClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai_->GetTaskRunner());
    ai_->GetAIRemote()->CreateSummarizer(std::move(client_remote));
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(resolver_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojo::PendingRemote<blink::mojom::blink::AISummarizer>
                    remote_summarizer) override {
    if (!GetExecutionContext() || !remote_summarizer) {
      resolver_->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateSession,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    } else {
      AISummarizer* summarizer = MakeGarbageCollected<AISummarizer>(
          GetExecutionContext(), ai_->GetTaskRunner(),
          std::move(remote_summarizer));
      resolver_->Resolve(summarizer);
    }
    Cleanup();
  }

  void ContextDestroyed() override { Cleanup(); }

 private:
  void Cleanup() {
    ai_.Clear();
    if (resolver_) {
      resolver_->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateSession,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    }
    resolver_.Clear();
    receiver_.reset();
  }

  Member<AI> ai_;
  Member<ScriptPromiseResolver<AISummarizer>> resolver_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateSummarizerClient,
                   CreateSummarizerClient>
      receiver_;
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

ScriptPromise<AISummarizerCapabilities> AISummarizerFactory::capabilities(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AISummarizerCapabilities>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AISummarizerCapabilities>>(
          script_state);
  auto promise = resolver->Promise();
  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  ai_->GetAIRemote()->CanCreateSummarizer(WTF::BindOnce(
      [](ScriptPromiseResolver<AISummarizerCapabilities>* resolver,
         AISummarizerFactory* factory,
         mojom::blink::ModelAvailabilityCheckResult result) {
        AICapabilityAvailability availability =
            HandleModelAvailabilityCheckResult(
                factory->GetExecutionContext(),
                AIMetrics::AISessionType::kSummarizer, result);
        resolver->Resolve(MakeGarbageCollected<AISummarizerCapabilities>(
            AICapabilityAvailabilityToV8(availability)));
      },
      WrapPersistent(resolver), WrapWeakPersistent(this)));
  return promise;
}

ScriptPromise<AISummarizer> AISummarizerFactory::create(
    ScriptState* script_state,
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
  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<CreateSummarizerClient>(ai_.Get(), resolver)
      ->CreateSummarizer();
  return promise;
}

}  // namespace blink
