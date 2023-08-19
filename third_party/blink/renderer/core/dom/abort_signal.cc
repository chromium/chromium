// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_manager.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_type.h"
#include "third_party/blink/renderer/core/dom/abort_signal_registry.h"
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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
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

}  // namespace

class FollowAlgorithm final : public AbortSignal::Algorithm {
 public:
  FollowAlgorithm(ScriptState* script_state,
                  AbortSignal* parent,
                  AbortSignal* following)
      : script_state_(script_state), parent_(parent), following_(following) {}
  ~FollowAlgorithm() override = default;

  void Run() override {
    following_->SignalAbort(script_state_, parent_->reason(script_state_),
                            AbortSignal::SignalAbortPassKey());
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

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : AbortSignal(execution_context, SignalType::kInternal) {}

AbortSignal::AbortSignal(ExecutionContext* execution_context,
                         SignalType signal_type) {
  DCHECK_NE(signal_type, SignalType::kComposite);
  InitializeCommon(execution_context, signal_type);

  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    composition_manager_ = MakeGarbageCollected<SourceSignalCompositionManager>(
        *this, AbortSignalCompositionType::kAbort);
  }
}

AbortSignal::AbortSignal(ScriptState* script_state,
                         HeapVector<Member<AbortSignal>>& source_signals) {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  InitializeCommon(ExecutionContext::From(script_state),
                   SignalType::kComposite);

  // If any of the signals are aborted, skip the linking and just abort this
  // signal.
  for (auto& source : source_signals) {
    if (source->aborted()) {
      abort_reason_ = source->reason(script_state);
      source_signals.clear();
      break;
    }
  }
  composition_manager_ =
      MakeGarbageCollected<DependentSignalCompositionManager>(
          *this, AbortSignalCompositionType::kAbort, source_signals);
  // Ensure the registry isn't created during GC, e.g. during an abort
  // controller's prefinalizer.
  AbortSignalRegistry::From(*ExecutionContext::From(script_state));
}

void AbortSignal::InitializeCommon(ExecutionContext* execution_context,
                                   SignalType signal_type) {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled() ||
         signal_type != SignalType::kComposite);
  execution_context_ = execution_context;
  signal_type_ = signal_type;
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
  AbortSignal* signal = MakeGarbageCollected<AbortSignal>(
      ExecutionContext::From(script_state), SignalType::kAborted);
  signal->abort_reason_ = reason;
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    signal->composition_manager_->Settle();
  }
  return signal;
}

// static
AbortSignal* AbortSignal::any(ScriptState* script_state,
                              HeapVector<Member<AbortSignal>> signals) {
  return MakeGarbageCollected<AbortSignal>(script_state, signals);
}

// static
AbortSignal* AbortSignal::timeout(ScriptState* script_state,
                                  uint64_t milliseconds) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  AbortSignal* signal =
      MakeGarbageCollected<AbortSignal>(context, SignalType::kTimeout);
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
  SignalAbort(script_state, ScriptValue(isolate, reason), SignalAbortPassKey());
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
  if (aborted() || (RuntimeEnabledFeatures::AbortSignalCompositionEnabled() &&
                    composition_manager_->IsSettled())) {
    return nullptr;
  }
  auto* handle = MakeGarbageCollected<AlgorithmHandle>(algorithm, this);
  CHECK(!abort_algorithms_.Contains(handle));
  // This always appends since `handle` is not already in the collection.
  abort_algorithms_.insert(handle);
  return handle;
}

AbortSignal::AlgorithmHandle* AbortSignal::AddAlgorithm(
    base::OnceClosure algorithm) {
  if (aborted() || (RuntimeEnabledFeatures::AbortSignalCompositionEnabled() &&
                    composition_manager_->IsSettled())) {
    return nullptr;
  }
  auto* callback_algorithm =
      MakeGarbageCollected<OnceCallbackAlgorithm>(std::move(algorithm));
  auto* handle =
      MakeGarbageCollected<AlgorithmHandle>(callback_algorithm, this);
  CHECK(!abort_algorithms_.Contains(handle));
  // This always appends since `handle` is not already in the collection.
  abort_algorithms_.insert(handle);
  return handle;
}

void AbortSignal::RemoveAlgorithm(AlgorithmHandle* handle) {
  if (aborted() || (RuntimeEnabledFeatures::AbortSignalCompositionEnabled() &&
                    composition_manager_->IsSettled())) {
    return;
  }
  abort_algorithms_.erase(handle);
}

void AbortSignal::SignalAbort(ScriptState* script_state,
                              ScriptValue reason,
                              SignalAbortPassKey) {
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

  for (AbortSignal::AlgorithmHandle* handle : abort_algorithms_) {
    handle->GetAlgorithm()->Run();
  }

  if (!RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    // This is cleared when the signal is settled when the feature is enabled.
    abort_algorithms_.clear();
  }
  dependent_signal_algorithms_.clear();
  DispatchEvent(*Event::Create(event_type_names::kAbort));

  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    DCHECK(composition_manager_);
    // Dependent signals are linked directly to source signals, so the abort
    // only gets propagated for source signals.
    if (auto* source_signal_manager = DynamicTo<SourceSignalCompositionManager>(
            composition_manager_.Get())) {
      // This is safe against reentrancy because new dependents are not added to
      // already aborted signals.
      for (auto& signal : source_signal_manager->GetDependentSignals()) {
        signal->SignalAbort(script_state, abort_reason_, SignalAbortPassKey());
      }
    }
    composition_manager_->Settle();
  }
}

void AbortSignal::Follow(ScriptState* script_state, AbortSignal* parent) {
  if (aborted())
    return;
  if (parent->aborted()) {
    SignalAbort(script_state, parent->reason(script_state),
                SignalAbortPassKey());
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
  visitor->Trace(composition_manager_);
  EventTarget::Trace(visitor);
}

AbortSignalCompositionManager* AbortSignal::GetCompositionManager(
    AbortSignalCompositionType type) {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  if (type == AbortSignalCompositionType::kAbort) {
    return composition_manager_;
  }
  return nullptr;
}

void AbortSignal::DetachFromController() {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  if (aborted()) {
    return;
  }
  composition_manager_->Settle();
}

void AbortSignal::OnSignalSettled(AbortSignalCompositionType type) {
  CHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  if (type == AbortSignalCompositionType::kAbort) {
    abort_algorithms_.clear();
  }
  if (signal_type_ == SignalType::kComposite) {
    InvokeRegistryCallback([&](AbortSignalRegistry& registry) {
      registry.UnregisterSignal(*this, type);
    });
  }
}

bool AbortSignal::CanAbort() const {
  if (aborted()) {
    return false;
  }
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    return !composition_manager_->IsSettled();
  }
  return true;
}

void AbortSignal::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  OnEventListenerAddedOrRemoved(event_type, AddRemoveType::kAdded);
}

void AbortSignal::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  OnEventListenerAddedOrRemoved(event_type, AddRemoveType::kRemoved);
}

void AbortSignal::InvokeRegistryCallback(
    base::FunctionRef<void(AbortSignalRegistry&)> callback) {
  CHECK_EQ(signal_type_, SignalType::kComposite);
  callback(*AbortSignalRegistry::From(*GetExecutionContext()));
}

void AbortSignal::OnEventListenerAddedOrRemoved(const AtomicString& event_type,
                                                AddRemoveType add_or_remove) {
  if (!RuntimeEnabledFeatures::AbortSignalCompositionEnabled() ||
      signal_type_ != SignalType::kComposite) {
    return;
  }
  absl::optional<AbortSignalCompositionType> composition_type;
  if (event_type == event_type_names::kAbort) {
    composition_type = AbortSignalCompositionType::kAbort;
  } else if (event_type == event_type_names::kPrioritychange) {
    composition_type = AbortSignalCompositionType::kPriority;
  } else {
    return;
  }
  if (IsSettledFor(*composition_type)) {
    // Signals are unregistered when they're settled for `composition_type`
    // since the event will no longer be propagated. In that case, the signal
    // doesn't need to be unregistered on removal, and it shouldn't be
    // registered on adding a listener, since that could leak it.
    return;
  }
  if (add_or_remove == AddRemoveType::kRemoved &&
      HasEventListeners(event_type)) {
    // Unsettled composite signals need to be kept alive while they have active
    // event listeners for `event_type`, so only unregister the signal if
    // removing the last one.
    return;
  }
  // `manager` will be null if this signal doesn't handle composition for
  // `composition_type`.
  if (auto* manager = GetCompositionManager(*composition_type)) {
    InvokeRegistryCallback([&](AbortSignalRegistry& registry) {
      switch (add_or_remove) {
        case AddRemoveType::kAdded:
          registry.RegisterSignal(*this, *composition_type);
          break;
        case AddRemoveType::kRemoved:
          registry.UnregisterSignal(*this, *composition_type);
          break;
      }
    });
  }
}

bool AbortSignal::IsSettledFor(
    AbortSignalCompositionType composition_type) const {
  CHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  return composition_type == AbortSignalCompositionType::kAbort &&
         composition_manager_->IsSettled();
}

AbortSignal::AlgorithmHandle::AlgorithmHandle(AbortSignal::Algorithm* algorithm,
                                              AbortSignal* signal)
    : algorithm_(algorithm), signal_(signal) {}

AbortSignal::AlgorithmHandle::~AlgorithmHandle() = default;

void AbortSignal::AlgorithmHandle::Trace(Visitor* visitor) const {
  visitor->Trace(algorithm_);
  visitor->Trace(signal_);
}

}  // namespace blink
