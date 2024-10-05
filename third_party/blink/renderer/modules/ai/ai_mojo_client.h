// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_CLIENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// AIMojoClient is a base class for the renderer to send a mojo IPC to the AI
// component. It adds observers for the execution context lifecycle and the
// abort signal. The resources will be freed when the execution context gets
// destroyed or the user explicitly aborts.
template <typename V8SessionObjectType>
class AIMojoClient : public ContextLifecycleObserver {
 public:
  AIMojoClient(ExecutionContextClient* context_client,
               ScriptPromiseResolver<V8SessionObjectType>* resolver,
               AbortSignal* abort_signal)
      : context_client_(context_client),
        resolver_(resolver),
        abort_signal_(abort_signal) {
    CHECK(resolver);
    SetContextLifecycleNotifier(context_client_->GetExecutionContext());
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(
          WTF::BindOnce(&AIMojoClient::OnAborted, WrapWeakPersistent(this)));
    }
  }

  // `GarbageCollectedMixin` implementation
  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(context_client_);
    visitor->Trace(resolver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  ~AIMojoClient() override = default;

 protected:
  ScriptPromiseResolver<V8SessionObjectType>* GetResolver() {
    return resolver_;
  }

  virtual void Cleanup() {
    context_client_.Clear();
    resolver_ = nullptr;
    keep_alive_.Clear();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

 private:
  // `ContextLifecycleObserver` implementation
  void ContextDestroyed() override { Cleanup(); }

  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(DOMException::Create(
        kExceptionMessageRequestAborted,
        DOMException::GetErrorName(DOMExceptionCode::kAbortError)));
    Cleanup();
  }

  Member<ExecutionContextClient> context_client_;
  Member<ScriptPromiseResolver<V8SessionObjectType>> resolver_;
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  SelfKeepAlive<AIMojoClient> keep_alive_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_CLIENT_H_
