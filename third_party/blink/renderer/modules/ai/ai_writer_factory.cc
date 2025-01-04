// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer_factory.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_create_options.h"
#include "third_party/blink/renderer/modules/ai/ai_mojo_client.h"
#include "third_party/blink/renderer/modules/ai/ai_writer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const char kExceptionMessageUnableToCreateWriter[] =
    "The writer cannot be created.";

mojom::blink::AIWriterTone ToMojoAIWriterTone(V8AIWriterTone tone) {
  switch (tone.AsEnum()) {
    case V8AIWriterTone::Enum::kFormal:
      return mojom::blink::AIWriterTone::kFormal;
    case V8AIWriterTone::Enum::kNeutral:
      return mojom::blink::AIWriterTone::kNeutral;
    case V8AIWriterTone::Enum::kCasual:
      return mojom::blink::AIWriterTone::kCasual;
  }
  NOTREACHED();
}

mojom::blink::AIWriterFormat ToMojoAIWriterFormat(V8AIWriterFormat format) {
  switch (format.AsEnum()) {
    case V8AIWriterFormat::Enum::kPlainText:
      return mojom::blink::AIWriterFormat::kPlainText;
    case V8AIWriterFormat::Enum::kMarkdown:
      return mojom::blink::AIWriterFormat::kMarkdown;
  }
  NOTREACHED();
}

mojom::blink::AIWriterLength ToMojoAIWriterLength(V8AIWriterLength length) {
  switch (length.AsEnum()) {
    case V8AIWriterLength::Enum::kShort:
      return mojom::blink::AIWriterLength::kShort;
    case V8AIWriterLength::Enum::kMedium:
      return mojom::blink::AIWriterLength::kMedium;
    case V8AIWriterLength::Enum::kLong:
      return mojom::blink::AIWriterLength::kLong;
  }
  NOTREACHED();
}

class CreateWriterClient : public GarbageCollected<CreateWriterClient>,
                           public mojom::blink::AIManagerCreateWriterClient,
                           public AIMojoClient<AIWriter> {
 public:
  CreateWriterClient(ScriptState* script_state,
                     AI* ai,
                     ScriptPromiseResolver<AIWriter>* resolver,
                     AIWriterCreateOptions* options)
      : AIMojoClient(script_state, ai, resolver, options->getSignalOr(nullptr)),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        options_(options) {
    mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateWriter(
        std::move(client_remote),
        mojom::blink::AIWriterCreateOptions::New(
            options->getSharedContextOr(g_empty_string),
            ToMojoAIWriterTone(options->tone()),
            ToMojoAIWriterFormat(options->format()),
            ToMojoAIWriterLength(options->length())));
  }
  ~CreateWriterClient() override = default;

  CreateWriterClient(const CreateWriterClient&) = delete;
  CreateWriterClient& operator=(const CreateWriterClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIMojoClient::Trace(visitor);
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
      GetResolver()->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateWriter,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
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

ScriptPromise<V8AICapabilityAvailability> AIWriterFactory::availability(
    ScriptState* script_state,
    AIWriterCreateCoreOptions* options,
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

  ai_->GetAIRemote()->CanCreateWriter(
      mojom::blink::AIWriterCreateOptions::New(
          /*shared_context=*/g_empty_string,
          ToMojoAIWriterTone(options->tone()),
          ToMojoAIWriterFormat(options->format()),
          ToMojoAIWriterLength(options->length())),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8AICapabilityAvailability>* resolver,
             AIWriterFactory* factory,
             mojom::blink::ModelAvailabilityCheckResult result) {
            AICapabilityAvailability availability =
                HandleModelAvailabilityCheckResult(
                    factory->GetExecutionContext(),
                    AIMetrics::AISessionType::kWriter, result);
            resolver->Resolve(AICapabilityAvailabilityToV8(availability));
          },
          WrapPersistent(resolver), WrapWeakPersistent(this)));
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
