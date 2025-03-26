// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter_factory.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class CreateRewriterClient final
    : public AIWritingAssistanceCreateClient<
          mojom::blink::AIRewriter,
          mojom::blink::AIManagerCreateRewriterClient,
          AIRewriterCreateOptions,
          AIRewriter> {
 public:
  CreateRewriterClient(ScriptState* script_state,
                       AI* ai,
                       ScriptPromiseResolver<AIRewriter>* resolver,
                       AIRewriterCreateOptions* options)
      : AIWritingAssistanceCreateClient(script_state, ai, resolver, options) {}

  void Trace(Visitor* visitor) const override {
    AIWritingAssistanceCreateClient::Trace(visitor);
  }

  // AIWritingAssistanceCreateClient:
  void RemoteCreate(
      mojo::PendingRemote<mojom::blink::AIManagerCreateRewriterClient>
          client_remote) override {
    ai_->GetAIRemote()->CreateRewriter(std::move(client_remote),
                                       ToMojoRewriterCreateOptions(options_));
  }
};

}  // namespace

AIRewriterFactory::AIRewriterFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()), ai_(ai) {}

void AIRewriterFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<V8AIAvailability> AIRewriterFactory::availability(
    ScriptState* script_state,
    AIRewriterCreateCoreOptions* options,
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

  ai_->GetAIRemote()->CanCreateRewriter(
      ToMojoRewriterCreateOptions(options),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8AIAvailability>* resolver,
             AIRewriterFactory* factory,
             mojom::blink::ModelAvailabilityCheckResult result) {
            AIAvailability availability = HandleModelAvailabilityCheckResult(
                factory->GetExecutionContext(),
                AIMetrics::AISessionType::kRewriter, result);
            resolver->Resolve(AIAvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(this)));
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
                                             options)
      ->Create();
  return promise;
}

}  // namespace blink
