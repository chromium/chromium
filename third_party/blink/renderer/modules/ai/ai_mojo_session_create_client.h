// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_SESSION_CREATE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_SESSION_CREATE_CLIENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// AIMojoSessionCreateClient is a base class for AI object factories
// to create a mojo session. It adds observers for the execution context
// lifecycle and the abort signal. The resources will be freed when the
// execution context gets destroyed or the user explicitly aborts.
template <typename V8SessionObjectType>
class AIMojoSessionCreateClient : public ContextLifecycleObserver {
 public:
  AIMojoSessionCreateClient(
      AI* ai,
      ScriptPromiseResolver<V8SessionObjectType>* resolver,
      AbortSignal* abort_signal)
      : ai_(ai), resolver_(resolver), abort_signal_(abort_signal) {
    CHECK(resolver);
    SetContextLifecycleNotifier(ai->GetExecutionContext());
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(WTF::BindOnce(
          &AIMojoSessionCreateClient::OnAborted, WrapWeakPersistent(this)));
    }
  }

  // `GarbageCollectedMixin` implementation
  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(resolver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  ~AIMojoSessionCreateClient() override = default;

 protected:
  ScriptPromiseResolver<V8SessionObjectType>* GetResolver() {
    return resolver_;
  }

  virtual void Cleanup() {
    ai_.Clear();
    resolver_ = nullptr;
    keep_alive_.Clear();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

  AI* GetAI() { return ai_; }

 private:
  // `ContextDestroyed` implementation
  void ContextDestroyed() override { Cleanup(); }

  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(DOMException::Create(
        "Aborted", DOMException::GetErrorName(DOMExceptionCode::kAbortError)));
    Cleanup();
  }

  Member<AI> ai_;
  Member<ScriptPromiseResolver<V8SessionObjectType>> resolver_;
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  SelfKeepAlive<AIMojoSessionCreateClient> keep_alive_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_MOJO_SESSION_CREATE_CLIENT_H_
