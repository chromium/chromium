// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter_factory.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const char kExceptionMessageUnableToCreateRewriter[] =
    "The rewriter cannot be created.";

mojom::blink::AIRewriterTone ToMojoAIRewriterTone(V8AIRewriterTone tone) {
  switch (tone.AsEnum()) {
    case V8AIRewriterTone::Enum::kAsIs:
      return mojom::blink::AIRewriterTone::kAsIs;
    case V8AIRewriterTone::Enum::kMoreFormal:
      return mojom::blink::AIRewriterTone::kMoreFormal;
    case V8AIRewriterTone::Enum::kMoreCasual:
      return mojom::blink::AIRewriterTone::kMoreCasual;
  }
  NOTREACHED();
}

mojom::blink::AIRewriterFormat ToMojoAIRewriterFormat(
    V8AIRewriterFormat format) {
  switch (format.AsEnum()) {
    case V8AIRewriterFormat::Enum::kAsIs:
      return mojom::blink::AIRewriterFormat::kAsIs;
    case V8AIRewriterFormat::Enum::kPlainText:
      return mojom::blink::AIRewriterFormat::kPlainText;
    case V8AIRewriterFormat::Enum::kMarkdown:
      return mojom::blink::AIRewriterFormat::kMarkdown;
  }
  NOTREACHED();
}

mojom::blink::AIRewriterLength ToMojoAIRewriterLength(
    V8AIRewriterLength length) {
  switch (length.AsEnum()) {
    case V8AIRewriterLength::Enum::kAsIs:
      return mojom::blink::AIRewriterLength::kAsIs;
    case V8AIRewriterLength::Enum::kShorter:
      return mojom::blink::AIRewriterLength::kShorter;
    case V8AIRewriterLength::Enum::kLonger:
      return mojom::blink::AIRewriterLength::kLonger;
  }
  NOTREACHED();
}

class CreateRewriterClient : public GarbageCollected<CreateRewriterClient>,
                             public mojom::blink::AIManagerCreateRewriterClient,
                             public AIMojoClient<AIRewriter> {
 public:
  CreateRewriterClient(ScriptState* script_state,
                       AI* ai,
                       ScriptPromiseResolver<AIRewriter>* resolver,
                       AIRewriterCreateOptions* options)
      : AIMojoClient(script_state, ai, resolver, options->getSignalOr(nullptr)),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        options_(options) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateRewriterClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateRewriter(
        std::move(client_remote),
        mojom::blink::AIRewriterCreateOptions::New(
            options->getSharedContextOr(g_empty_string),
            ToMojoAIRewriterTone(options->tone()),
            ToMojoAIRewriterFormat(options->format()),
            ToMojoAIRewriterLength(options->length())));
  }
  ~CreateRewriterClient() override = default;

  CreateRewriterClient(const CreateRewriterClient&) = delete;
  CreateRewriterClient& operator=(const CreateRewriterClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
    visitor->Trace(options_);
  }

  void OnResult(
      mojo::PendingRemote<mojom::blink::AIRewriter> rewriter) override {
    if (!GetResolver()) {
      return;
    }
    if (rewriter) {
      GetResolver()->Resolve(MakeGarbageCollected<AIRewriter>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(), std::move(rewriter),
          options_));
    } else {
      GetResolver()->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateRewriter,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    }
    Cleanup();
  }

  void ResetReceiver() override { receiver_.reset(); }

 private:
  Member<AI> ai_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateRewriterClient,
                   CreateRewriterClient>
      receiver_;
  Member<AIRewriterCreateOptions> options_;
};

}  // namespace

AIRewriterFactory::AIRewriterFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()), ai_(ai) {}

void AIRewriterFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<V8AICapabilityAvailability> AIRewriterFactory::availability(
    ScriptState* script_state,
    AIRewriterCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AICapabilityAvailability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AICapabilityAvailability>>(
          script_state);
  auto promise = resolver->Promise();
  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  // TODO: Pass option to underlying check.
  ai_->GetAIRemote()->CanCreateRewriter(
      mojom::blink::AIRewriterCreateOptions::New(
          /*shared_context=*/g_empty_string,
          ToMojoAIRewriterTone(options->tone()),
          ToMojoAIRewriterFormat(options->format()),
          ToMojoAIRewriterLength(options->length())),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8AICapabilityAvailability>* resolver,
             AIRewriterFactory* factory,
             mojom::blink::ModelAvailabilityCheckResult result) {
            AICapabilityAvailability availability =
                HandleModelAvailabilityCheckResult(
                    factory->GetExecutionContext(),
                    AIMetrics::AISessionType::kRewriter, result);
            resolver->Resolve(AICapabilityAvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapWeakPersistent(this)));
  return promise;
}

ScriptPromise<AIRewriter> AIRewriterFactory::create(
    ScriptState* script_state,
    AIRewriterCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIRewriter>();
  }
  CHECK(options);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIRewriter>>(script_state);
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

  MakeGarbageCollected<CreateRewriterClient>(script_state, ai_, resolver,
                                             options);
  return promise;
}

}  // namespace blink
