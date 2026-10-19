// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CONTEXT_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CONTEXT_OBSERVER_H_

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/on_device_model/public/mojom/download_observer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_create_monitor_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/modules/ai/create_monitor.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"

namespace blink {

// AIContextObserver is a base class for the renderer to send a mojo IPC to the
// AI component. It adds observers for the execution context lifecycle and the
// abort signal. The resources will be freed when the execution context gets
// destroyed or the user explicitly aborts.
template <typename V8SessionObjectType>
class AIContextObserver : public ContextLifecycleObserver {
 public:
  AIContextObserver(ScriptState* script_state,
                    ExecutionContextClient* context_client,
                    ScriptPromiseResolver<V8SessionObjectType>* resolver,
                    AbortSignal* abort_signal)
      : script_state_(script_state),
        context_client_(context_client),
        resolver_(resolver),
        abort_signal_(abort_signal) {
    CHECK(resolver);
    SetContextLifecycleNotifier(context_client_->GetExecutionContext());
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(
          BindOnce(&AIContextObserver::OnAborted, WrapWeakPersistent(this)));
    }
  }

  // `GarbageCollectedMixin` implementation
  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(context_client_);
    visitor->Trace(resolver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
    visitor->Trace(monitor_);
  }

  ~AIContextObserver() override = default;

 protected:
  ScriptState* GetScriptState() { return script_state_; }
  ScriptPromiseResolver<V8SessionObjectType>* GetResolver() {
    return resolver_;
  }
  AbortSignal* GetAbortSignal() { return abort_signal_; }

  // Initializes the download progress monitor from `options` (if present) and
  // invokes its callback. Returns false if the callback threw or aborted.
  template <typename CreateOptions>
  bool InitializeMonitor(const CreateOptions* options,
                         scoped_refptr<base::SequencedTaskRunner> task_runner) {
    CHECK(options);
    if (!options->hasMonitor()) {
      return resolver_ != nullptr;
    }
    monitor_ = MakeGarbageCollected<CreateMonitor>(
        context_client_->GetExecutionContext(), abort_signal_,
        std::move(task_runner));
    // Use `InvokeAndCatch` and reject `resolver_` explicitly instead of
    // `Invoke`, which detaches `resolver_` and would fail if `Promise()` is
    // called after constructing the observer, or trip `Dispose()`'s detach
    // check on `Cleanup()`.
    if (auto result = options->monitor()->InvokeAndCatch(nullptr, monitor_);
        !result.has_value()) {
      if (resolver_) {
        resolver_->Reject(result.error());
      }
      Cleanup();
      return false;
    }
    return resolver_ != nullptr;
  }

  mojo::PendingRemote<on_device_model::mojom::blink::DownloadObserver>
  BindMonitorRemote() {
    return monitor_ ? monitor_->BindRemote() : mojo::NullRemote();
  }

  // Ensures 0 and 100% download progress events are dispatched via `monitor_`
  // (which ignores redundant updates). Returns whether `resolver_` remains
  // valid, as progress listeners may abort creation or destroy the context.
  bool CompleteDownloadProgress() {
    if (monitor_ && resolver_) {
      monitor_->OnDownloadProgressUpdate(0, kNormalizedDownloadProgressMax);
    }
    if (monitor_ && resolver_) {
      monitor_->OnDownloadProgressUpdate(kNormalizedDownloadProgressMax,
                                         kNormalizedDownloadProgressMax);
    }
    return resolver_ != nullptr;
  }

  void Cleanup() {
    ResetReceiver();
    resolver_ = nullptr;
    monitor_ = nullptr;
    keep_alive_.Clear();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

  virtual void ResetReceiver() = 0;

  ScriptState* GetScriptState() const { return script_state_.Get(); }

 private:
  // `ContextLifecycleObserver` implementation
  void ContextDestroyed() override { Cleanup(); }

  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(abort_signal_->reason(script_state_));
    Cleanup();
  }

  Member<ScriptState> script_state_;
  Member<ExecutionContextClient> context_client_;
  Member<ScriptPromiseResolver<V8SessionObjectType>> resolver_;
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  Member<CreateMonitor> monitor_;
  SelfKeepAlive<AIContextObserver> keep_alive_{{}, this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_CONTEXT_OBSERVER_H_
