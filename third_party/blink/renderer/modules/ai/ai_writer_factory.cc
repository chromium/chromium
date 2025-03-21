// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer_factory.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/ai_writer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

class CreateWriterClient : public GarbageCollected<CreateWriterClient>,
                           public mojom::blink::AIManagerCreateWriterClient,
                           public AIContextObserver<AIWriter> {
 public:
  CreateWriterClient(ScriptState* script_state,
                     AI* ai,
                     ScriptPromiseResolver<AIWriter>* resolver,
                     AIWriterCreateOptions* options)
      : AIContextObserver(script_state,
                          ai,
                          resolver,
                          options->getSignalOr(nullptr)),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        options_(options) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateWriter(std::move(client_remote),
                                     ToMojoWriterCreateOptions(options));
  }
  ~CreateWriterClient() override = default;

  CreateWriterClient(const CreateWriterClient&) = delete;
  CreateWriterClient& operator=(const CreateWriterClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(options_);
    visitor->Trace(receiver_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AIWriter> writer) override {
    if (!GetResolver()) {
      return;
    }
    if (writer) {
      GetResolver()->Resolve(MakeGarbageCollected<AIWriter>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(), std::move(writer),
          options_));
    } else {
      GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
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
  HeapMojoReceiver<mojom::blink::AIManagerCreateWriterClient,
                   CreateWriterClient>
      receiver_;
  Member<AIWriterCreateOptions> options_;
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

  MakeGarbageCollected<CreateWriterClient>(script_state, ai_, resolver,
                                           options);
  return promise;
}

}  // namespace blink
