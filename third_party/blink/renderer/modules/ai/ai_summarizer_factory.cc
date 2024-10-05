// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_summarizer_factory.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_summarizer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

namespace {

mojom::blink::AISummarizerType ToMojoSummarizerType(V8AISummarizerType type) {
  switch (type.AsEnum()) {
    case V8AISummarizerType::Enum::kTlDr:
      return mojom::blink::AISummarizerType::kTLDR;
    case V8AISummarizerType::Enum::kKeyPoints:
      return mojom::blink::AISummarizerType::kKeyPoints;
    case V8AISummarizerType::Enum::kTeaser:
      return mojom::blink::AISummarizerType::kTeaser;
    case V8AISummarizerType::Enum::kHeadline:
      return mojom::blink::AISummarizerType::kHeadline;
  }
}

mojom::blink::AISummarizerFormat ToMojoSummarizerFormat(
    V8AISummarizerFormat format) {
  switch (format.AsEnum()) {
    case V8AISummarizerFormat::Enum::kPlainText:
      return mojom::blink::AISummarizerFormat::kPlainText;
    case V8AISummarizerFormat::Enum::kMarkdown:
      return mojom::blink::AISummarizerFormat::kMarkDown;
  }
}

mojom::blink::AISummarizerLength ToMojoSummarizerLength(
    V8AISummarizerLength length) {
  switch (length.AsEnum()) {
    case V8AISummarizerLength::Enum::kShort:
      return mojom::blink::AISummarizerLength::kShort;
    case V8AISummarizerLength::Enum::kMedium:
      return mojom::blink::AISummarizerLength::kMedium;
    case V8AISummarizerLength::Enum::kLong:
      return mojom::blink::AISummarizerLength::kLong;
  }
}

class CreateSummarizerClient
    : public GarbageCollected<CreateSummarizerClient>,
      public AIMojoClient<AISummarizer>,
      public mojom::blink::AIManagerCreateSummarizerClient {
 public:
  explicit CreateSummarizerClient(AI* ai,
                                  const AISummarizerCreateOptions* options,
                                  ScriptPromiseResolver<AISummarizer>* resolver)
      : AIMojoClient(ai, resolver, options->getSignalOr(nullptr)),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        type_(options->type()),
        format_(options->format()),
        length_(options->length()),
        shared_context_(options->getSharedContextOr(WTF::String())) {}

  ~CreateSummarizerClient() override = default;

  void CreateSummarizer() {
    mojo::PendingRemote<mojom::blink::AIManagerCreateSummarizerClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai_->GetTaskRunner());
    ai_->GetAIRemote()->CreateSummarizer(
        std::move(client_remote),
        mojom::blink::AISummarizerCreateOptions::New(
            shared_context_, ToMojoSummarizerType(type_),
            ToMojoSummarizerFormat(format_), ToMojoSummarizerLength(length_)));
  }

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AISummarizer>
                    remote_summarizer) override {
    if (!GetResolver()) {
      // The creation was aborted by the user.
      return;
    }
    if (!ai_->GetExecutionContext() || !remote_summarizer) {
      GetResolver()->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateSession,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    } else {
      AISummarizer* summarizer = MakeGarbageCollected<AISummarizer>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(),
          std::move(remote_summarizer), shared_context_, type_, format_,
          length_);
      GetResolver()->Resolve(summarizer);
    }
    Cleanup();
  }

 private:
  Member<AI> ai_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateSummarizerClient,
                   CreateSummarizerClient>
      receiver_;

  V8AISummarizerType type_;
  V8AISummarizerFormat format_;
  V8AISummarizerLength length_;
  WTF::String shared_context_;
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
  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<CreateSummarizerClient>(ai_.Get(), options, resolver)
      ->CreateSummarizer();
  return promise;
}

}  // namespace blink
