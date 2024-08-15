// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_writer_factory.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_writer_create_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_writer.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const char kExceptionMessageUnableToCreateWriter[] =
    "The writer cannot be created.";

class CreateWriterClient : public GarbageCollected<CreateWriterClient>,
                           public mojom::blink::AIManagerCreateWriterClient,
                           public ContextLifecycleObserver {
 public:
  CreateWriterClient(AI* ai,
                     ScriptPromiseResolver<AIWriter>* resolver,
                     AbortSignal* signal,
                     String shared_context_string)
      : ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        resolver_(resolver),
        shared_context_string_(shared_context_string),
        abort_signal_(signal) {
    CHECK(resolver_);
    SetContextLifecycleNotifier(ai->GetExecutionContext());
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(WTF::BindOnce(
          &CreateWriterClient::OnAborted, WrapWeakPersistent(this)));
    }

    mojo::PendingRemote<mojom::blink::AIManagerCreateWriterClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateWriter(shared_context_string_,
                                     std::move(client_remote));
  }
  ~CreateWriterClient() override = default;

  CreateWriterClient(const CreateWriterClient&) = delete;
  CreateWriterClient& operator=(const CreateWriterClient&) = delete;

  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
    visitor->Trace(resolver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  void OnResult(mojo::PendingRemote<mojom::blink::AIWriter> writer) override {
    if (!resolver_) {
      return;
    }
    if (writer) {
      resolver_->Resolve(MakeGarbageCollected<AIWriter>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(), std::move(writer),
          shared_context_string_));
    } else {
      resolver_->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateWriter,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    }
  }

  // ContextLifecycleObserver methods
  void ContextDestroyed() override { Cleanup(); }

 private:
  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(DOMException::Create(
        "Aborted", DOMException::GetErrorName(DOMExceptionCode::kAbortError)));
    Cleanup();
  }

  void Cleanup() {
    resolver_ = nullptr;
    keep_alive_.Clear();
    receiver_.reset();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

  Member<AI> ai_;
  HeapMojoReceiver<mojom::blink::AIManagerCreateWriterClient,
                   CreateWriterClient>
      receiver_;
  // `resolver_` will be reset on Cleanup().
  Member<ScriptPromiseResolver<AIWriter>> resolver_;
  const String shared_context_string_;
  SelfKeepAlive<CreateWriterClient> keep_alive_{this};
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

}  // namespace

AIWriterFactory::AIWriterFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()), ai_(ai) {}

void AIWriterFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<AIWriter> AIWriterFactory::create(
    ScriptState* script_state,
    const AIWriterCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIWriter>();
  }
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "Aborted");
    return ScriptPromise<AIWriter>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIWriter>>(script_state);
  auto promise = resolver->Promise();

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<CreateWriterClient>(
      ai_, resolver, signal, options->getSharedContextOr(String()));
  return promise;
}

}  // namespace blink
