// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_rewriter_factory.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_rewriter_create_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_rewriter.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
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
  NOTREACHED_IN_MIGRATION();
}

mojom::blink::AIRewriterLength ToMojoAIRewriterLength(V8AIRewriterLength tone) {
  switch (tone.AsEnum()) {
    case V8AIRewriterLength::Enum::kAsIs:
      return mojom::blink::AIRewriterLength::kAsIs;
    case V8AIRewriterLength::Enum::kShorter:
      return mojom::blink::AIRewriterLength::kShorter;
    case V8AIRewriterLength::Enum::kLonger:
      return mojom::blink::AIRewriterLength::kLonger;
  }
  NOTREACHED_IN_MIGRATION();
}

// TODO(crbug.com/358214322): Consider unifying the logic of this class with
// CreateWriterClient in ai_writer_factory.cc.
class CreateRewriterClient : public GarbageCollected<CreateRewriterClient>,
                             public mojom::blink::AIManagerCreateRewriterClient,
                             public ContextLifecycleObserver {
 public:
  CreateRewriterClient(AI* ai,
                       ScriptPromiseResolver<AIRewriter>* resolver,
                       AbortSignal* signal,
                       V8AIRewriterTone tone,
                       V8AIRewriterLength length,
                       String shared_context_string)
      : ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        resolver_(resolver),
        shared_context_string_(shared_context_string),
        abort_signal_(signal),
        tone_(tone),
        length_(length) {
    CHECK(resolver_);
    SetContextLifecycleNotifier(ai->GetExecutionContext());
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(WTF::BindOnce(
          &CreateRewriterClient::OnAborted, WrapWeakPersistent(this)));
    }

    mojo::PendingRemote<mojom::blink::AIManagerCreateRewriterClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai->GetTaskRunner());
    ai_->GetAIRemote()->CreateRewriter(
        shared_context_string_, ToMojoAIRewriterTone(tone),
        ToMojoAIRewriterLength(length), std::move(client_remote));
  }
  ~CreateRewriterClient() override = default;

  CreateRewriterClient(const CreateRewriterClient&) = delete;
  CreateRewriterClient& operator=(const CreateRewriterClient&) = delete;

  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
    visitor->Trace(resolver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  void OnResult(
      mojo::PendingRemote<mojom::blink::AIRewriter> rewriter) override {
    if (!resolver_) {
      return;
    }
    if (rewriter) {
      resolver_->Resolve(MakeGarbageCollected<AIRewriter>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(), std::move(rewriter),
          shared_context_string_, tone_, length_));
    } else {
      resolver_->Reject(DOMException::Create(
          kExceptionMessageUnableToCreateRewriter,
          DOMException::GetErrorName(DOMExceptionCode::kInvalidStateError)));
    }
    Cleanup();
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
  HeapMojoReceiver<mojom::blink::AIManagerCreateRewriterClient,
                   CreateRewriterClient>
      receiver_;
  // `resolver_` will be reset on Cleanup().
  Member<ScriptPromiseResolver<AIRewriter>> resolver_;
  const String shared_context_string_;
  SelfKeepAlive<CreateRewriterClient> keep_alive_{this};
  Member<AbortSignal> abort_signal_;
  const V8AIRewriterTone tone_;
  const V8AIRewriterLength length_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

}  // namespace

AIRewriterFactory::AIRewriterFactory(AI* ai)
    : ExecutionContextClient(ai->GetExecutionContext()), ai_(ai) {}

void AIRewriterFactory::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_);
}

ScriptPromise<AIRewriter> AIRewriterFactory::create(
    ScriptState* script_state,
    const AIRewriterCreateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AIRewriter>();
  }
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "Aborted");
    return ScriptPromise<AIRewriter>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AIRewriter>>(script_state);
  auto promise = resolver->Promise();

  if (!ai_->GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  MakeGarbageCollected<CreateRewriterClient>(
      ai_, resolver, signal, options->tone(), options->length(),
      options->getSharedContextOr(String()));
  return promise;
}

}  // namespace blink
