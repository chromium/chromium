// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer_factory.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/ai_writer.h"
#include "third_party/blink/renderer/modules/ai/ai_writing_assistance_create_client.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class CreateWriterClient final : public AIWritingAssistanceCreateClient<
                                     mojom::blink::AIWriter,
                                     mojom::blink::AIManagerCreateWriterClient,
                                     AIWriterCreateOptions,
                                     AIWriter> {
 public:
  CreateWriterClient(ScriptState* script_state,
                     AI* ai,
                     ScriptPromiseResolver<AIWriter>* resolver,
                     AIWriterCreateOptions* options)
      : AIWritingAssistanceCreateClient(script_state, ai, resolver, options) {}

  void Trace(Visitor* visitor) const override {
    AIWritingAssistanceCreateClient::Trace(visitor);
  }

  // AIWritingAssistanceCreateClient:
  void RemoteCreate(
      mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
          client_remote) override {
    ai_->GetAIRemote()->CreateWriter(std::move(client_remote),
                                     ToMojoWriterCreateOptions(options_));
  }
};

}  // namespace

AIWriterFactory::AIWriterFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()), ai_(ai) {}

void AIWriterFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<V8AIAvailability> AIWriterFactory::availability(
    ScriptState* script_state,
    AIWriterCreateCoreOptions* options,
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

  ai_->GetAIRemote()->CanCreateWriter(
      ToMojoWriterCreateOptions(options),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8AIAvailability>* resolver,
             AIWriterFactory* factory,
             mojom::blink::ModelAvailabilityCheckResult result) {
            AIAvailability availability = HandleModelAvailabilityCheckResult(
                factory->GetExecutionContext(),
                AIMetrics::AISessionType::kWriter, result);
            resolver->Resolve(AIAvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapPersistent(this)));
  return promise;
}

ScriptPromise<AIWriter> AIWriterFactory::create(
    ScriptState* script_state,
    AIWriterCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIWriter>();
  }
  CHECK(options);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIWriter>>(script_state);
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

  MakeGarbageCollected<CreateWriterClient>(script_state, ai_, resolver, options)
      ->Create();
  return promise;
}

}  // namespace blink
