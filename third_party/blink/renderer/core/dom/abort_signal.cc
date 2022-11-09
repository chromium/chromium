// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

class OnceCallbackAlgorithm final : public AbortSignal::Algorithm {
 public:
  explicit OnceCallbackAlgorithm(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  ~OnceCallbackAlgorithm() override = default;

  void Run() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
};

class FollowAlgorithm final : public AbortSignal::Algorithm {
 public:
  FollowAlgorithm(ScriptState* script_state,
                  AbortSignal* parent,
                  AbortSignal* following)
      : script_state_(script_state), parent_(parent), following_(following) {}
  ~FollowAlgorithm() override = default;

  void Run() override {
    following_->SignalAbort(script_state_, parent_->reason(script_state_));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(parent_);
    visitor->Trace(following_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<ScriptState> script_state_;
  Member<AbortSignal> parent_;
  Member<AbortSignal> following_;
};

// Variant of `AbortAlgorithmCollection` that implements removal. This holds
// weak references to algorithm handles, leaving the lifetime up to algorithm
// creators. Used only when features::kAbortSignalHandleBasedRemoval is true.
class RemovableAbortAlgorithmCollection final
    : public AbortSignal::AbortAlgorithmCollection {
 public:
  RemovableAbortAlgorithmCollection() = default;
  ~RemovableAbortAlgorithmCollection() = default;

  RemovableAbortAlgorithmCollection(const RemovableAbortAlgorithmCollection&) =
      delete;
  RemovableAbortAlgorithmCollection& operator=(
      const RemovableAbortAlgorithmCollection&) = delete;

  void AddAlgorithm(AbortSignal::AlgorithmHandle* handle) override {
    DCHECK(!abort_algorithms_.Contains(handle));
    // This always appends since `handle` is not already in the collection.
    abort_algorithms_.insert(handle);
  }

  void RemoveAlgorithm(AbortSignal::AlgorithmHandle* handle) override {
    abort_algorithms_.erase(handle);
  }

  void Clear() override { abort_algorithms_.clear(); }

  void Run() override {
    for (AbortSignal::AlgorithmHandle* handle : abort_algorithms_) {
      handle->GetAlgorithm()->Run();
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(abort_algorithms_);
    AbortAlgorithmCollection::Trace(visitor);
  }

 private:
  HeapLinkedHashSet<WeakMember<AbortSignal::AlgorithmHandle>> abort_algorithms_;
};

// Variant of `AbortAlgorithmCollection` that does not implement removal. This
// holds strong references to algorithms, leaving algorithms around for as long
// as the signal is alive. Enabled when features::kAbortSignalHandleBasedRemoval
// is false.
class UnremovableAbortAlgorithmCollection final
    : public AbortSignal::AbortAlgorithmCollection {
 public:
  UnremovableAbortAlgorithmCollection() = default;
  ~UnremovableAbortAlgorithmCollection() = default;

  UnremovableAbortAlgorithmCollection(
      const UnremovableAbortAlgorithmCollection&) = delete;
  UnremovableAbortAlgorithmCollection& operator=(
      const UnremovableAbortAlgorithmCollection&) = delete;

  void AddAlgorithm(AbortSignal::AlgorithmHandle* handle) override {
    abort_algorithms_.push_back(handle->GetAlgorithm());
  }

  void RemoveAlgorithm(AbortSignal::AlgorithmHandle* handle) override {}

  void Clear() override { abort_algorithms_.clear(); }

  void Run() override {
    for (AbortSignal::Algorithm* algorithm : abort_algorithms_) {
      algorithm->Run();
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(abort_algorithms_);
    AbortAlgorithmCollection::Trace(visitor);
  }

 private:
  HeapVector<Member<AbortSignal::Algorithm>> abort_algorithms_;
};

}  // namespace

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : execution_context_(execution_context) {
  if (base::FeatureList::IsEnabled(features::kAbortSignalHandleBasedRemoval)) {
    abort_algorithms_ =
        MakeGarbageCollected<RemovableAbortAlgorithmCollection>();
  } else {
    abort_algorithms_ =
        MakeGarbageCollected<UnremovableAbortAlgorithmCollection>();
  }
}

AbortSignal::~AbortSignal() = default;

// static
AbortSignal* AbortSignal::abort(ScriptState* script_state) {
  v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "signal is aborted without reason");
  CHECK(!dom_exception.IsEmpty());
  ScriptValue reason(script_state->GetIsolate(), dom_exception);
  return abort(script_state, reason);
}

// static
AbortSignal* AbortSignal::abort(ScriptState* script_state, ScriptValue reason) {
  DCHECK(!reason.IsEmpty());
  AbortSignal* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  signal->abort_reason_ = reason;
  return signal;
}

// static
AbortSignal* AbortSignal::timeout(ScriptState* script_state,
                                  uint64_t milliseconds) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  AbortSignal* signal = MakeGarbageCollected<AbortSignal>(context);
  // The spec requires us to use the timer task source, but there are a few
  // timer task sources due to our throttling implementation. We match
  // setTimeout for immediate timeouts, but use the high-nesting task type for
  // all positive timeouts so they are eligible for throttling (i.e. no
  // nesting-level exception).
  TaskType task_type = milliseconds == 0
                           ? TaskType::kJavascriptTimerImmediate
                           : TaskType::kJavascriptTimerDelayedHighNesting;
  // `signal` needs to be held with a strong reference to keep it alive in case
  // there are or will be event handlers attached.
  context->GetTaskRunner(task_type)->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&AbortSignal::AbortTimeoutFired, WrapPersistent(signal),
                    WrapPersistent(script_state)),
      base::Milliseconds(milliseconds));
  return signal;
}

void AbortSignal::AbortTimeoutFired(ScriptState* script_state) {
  if (GetExecutionContext()->IsContextDestroyed() ||
      !script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  auto* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> reason = V8ThrowDOMException::CreateOrEmpty(
      isolate, DOMExceptionCode::kTimeoutError, "signal timed out");
  SignalAbort(script_state, ScriptValue(isolate, reason));
}

ScriptValue AbortSignal::reason(ScriptState* script_state) const {
  DCHECK(script_state->GetIsolate()->InContext());
  if (abort_reason_.IsEmpty()) {
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }
  return abort_reason_;
}

void AbortSignal::throwIfAborted(ScriptState* script_state,
                                 ExceptionState& exception_state) const {
  if (!aborted())
    return;
  exception_state.RethrowV8Exception(reason(script_state).V8Value());
}

const AtomicString& AbortSignal::InterfaceName() const {
  return event_target_names::kAbortSignal;
}

ExecutionContext* AbortSignal::GetExecutionContext() const {
  return execution_context_.Get();
}

AbortSignal::AlgorithmHandle* AbortSignal::AddAlgorithm(Algorithm* algorithm) {
  if (aborted())
    return nullptr;

  auto* handle = MakeGarbageCollected<AlgorithmHandle>(algorithm);
  abort_algorithms_->AddAlgorithm(handle);
  return handle;
}

void AbortSignal::RemoveAlgorithm(AlgorithmHandle* handle) {
  if (aborted())
    return;
  abort_algorithms_->RemoveAlgorithm(handle);
}

AbortSignal::AlgorithmHandle* AbortSignal::AddAlgorithm(
    base::OnceClosure algorithm) {
  if (aborted())
    return nullptr;
  auto* callback_algorithm =
      MakeGarbageCollected<OnceCallbackAlgorithm>(std::move(algorithm));
  auto* handle = MakeGarbageCollected<AlgorithmHandle>(callback_algorithm);
  abort_algorithms_->AddAlgorithm(handle);
  return handle;
}

void AbortSignal::SignalAbort(ScriptState* script_state) {
  v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "signal is aborted without reason");
  CHECK(!dom_exception.IsEmpty());
  ScriptValue reason(script_state->GetIsolate(), dom_exception);
  SignalAbort(script_state, reason);
}

void AbortSignal::SignalAbort(ScriptState* script_state, ScriptValue reason) {
  DCHECK(!reason.IsEmpty());
  if (aborted())
    return;
  if (reason.IsUndefined()) {
    abort_reason_ = ScriptValue(
        script_state->GetIsolate(),
        V8ThrowDOMException::CreateOrEmpty(
            script_state->GetIsolate(), DOMExceptionCode::kAbortError,
            "signal is aborted with undefined reason"));
  } else {
    abort_reason_ = reason;
  }
  abort_algorithms_->Run();
  abort_algorithms_->Clear();
  dependent_signal_algorithms_.clear();
  DispatchEvent(*Event::Create(event_type_names::kAbort));
}

void AbortSignal::Follow(ScriptState* script_state, AbortSignal* parent) {
  if (aborted())
    return;
  if (parent->aborted()) {
    SignalAbort(script_state, parent->reason(script_state));
    return;
  }

  auto* handle = parent->AddAlgorithm(
      MakeGarbageCollected<FollowAlgorithm>(script_state, parent, this));
  parent->dependent_signal_algorithms_.push_back(handle);
}

void AbortSignal::Trace(Visitor* visitor) const {
  visitor->Trace(abort_reason_);
  visitor->Trace(execution_context_);
  visitor->Trace(abort_algorithms_);
  visitor->Trace(dependent_signal_algorithms_);
  EventTargetWithInlineData::Trace(visitor);
}

AbortSignal::AlgorithmHandle::AlgorithmHandle(AbortSignal::Algorithm* algorithm)
    : algorithm_(algorithm) {}

AbortSignal::AlgorithmHandle::~AlgorithmHandle() = default;

void AbortSignal::AlgorithmHandle::Trace(Visitor* visitor) const {
  visitor->Trace(algorithm_);
}

}  // namespace blink
