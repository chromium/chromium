// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/time/time.h"
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
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
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

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : execution_context_(execution_context),
      signal_type_(SignalType::kComposite) {
  InitializeCompositeSignal(HeapVector<Member<AbortSignal>>());
}

AbortSignal::AbortSignal(ExecutionContext* execution_context,
                         SignalType signal_type)
    : execution_context_(execution_context),
      signal_type_(signal_type),
      composition_manager_(MakeGarbageCollected<SourceSignalCompositionManager>(
          *this,
          AbortSignalCompositionType::kAbort)) {
  DCHECK_NE(signal_type, SignalType::kComposite);
}

AbortSignal::AbortSignal(ScriptState* script_state,
                         const HeapVector<Member<AbortSignal>>& source_signals)
    : execution_context_(ExecutionContext::From(script_state)),
      signal_type_(SignalType::kComposite) {
  // If any of the signals are aborted, skip the linking and just abort this
  // signal.
  for (auto& source : source_signals) {
    CHECK(source.Get());
    if (source->aborted()) {
      abort_reason_ = source->reason(script_state);
      break;
    }
  }
  InitializeCompositeSignal(aborted() ? HeapVector<Member<AbortSignal>>()
                                      : source_signals);
}

void AbortSignal::InitializeCompositeSignal(
    const HeapVector<Member<AbortSignal>>& source_signals) {
  CHECK_EQ(signal_type_, SignalType::kComposite);
  composition_manager_ =
      MakeGarbageCollected<DependentSignalCompositionManager>(
          *this, AbortSignalCompositionType::kAbort, source_signals);
  // Ensure the registry isn't created during GC, e.g. during an abort
  // controller's prefinalizer.
  AbortSignalRegistry::From(CHECK_DEREF(execution_context_.Get()));
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
  signal->composition_manager_->Settle();
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

void AbortSignal::throwIfAborted() const {
  if (!aborted())
    return;
  V8ThrowException::ThrowException(execution_context_->GetIsolate(),
                                   abort_reason_.V8Value());
}

const AtomicString& AbortSignal::InterfaceName() const {
  return event_target_names::kAbortSignal;
}

ExecutionContext* AbortSignal::GetExecutionContext() const {
  return execution_context_.Get();
}

AbortSignal::AlgorithmHandle* AbortSignal::AddAlgorithm(Algorithm* algorithm) {
  if (aborted() || composition_manager_->IsSettled()) {
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
  if (aborted() || composition_manager_->IsSettled()) {
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
  if (aborted() || composition_manager_->IsSettled()) {
    return;
  }
  abort_algorithms_.erase(handle);
}

void AbortSignal::SignalAbort(ScriptState* script_state,
                              ScriptValue reason,
                              SignalAbortPassKey) {
  DCHECK(!reason.IsEmpty());
  if (aborted()) {
    return;
  }

  CHECK(composition_manager_);
  auto* source_signal_manager =
      DynamicTo<SourceSignalCompositionManager>(composition_manager_.Get());
  // `SignalAbort` can only be called on source signals.
  CHECK(source_signal_manager);
  HeapVector<Member<AbortSignal>> dependent_signals_to_abort;
  dependent_signals_to_abort.ReserveInitialCapacity(
      source_signal_manager->GetDependentSignals().size());

  // Set the abort reason for this signal and any unaborted dependent signals so
  // that all dependent signals are aborted before JS runs in abort algorithms
  // or event dispatch.
  SetAbortReason(script_state, reason);

  for (auto& signal : source_signal_manager->GetDependentSignals()) {
    CHECK(signal.Get());
    if (!signal->aborted()) {
      signal->SetAbortReason(script_state, abort_reason_);
      dependent_signals_to_abort.push_back(signal);
    }
  }

  RunAbortSteps();

  for (auto& signal : dependent_signals_to_abort) {
    signal->RunAbortSteps();
    signal->composition_manager_->Settle();
  }

  composition_manager_->Settle();
}

void AbortSignal::SetAbortReason(ScriptState* script_state,
                                 ScriptValue reason) {
  CHECK(!aborted());
  if (reason.IsUndefined()) {
    abort_reason_ = ScriptValue(
        script_state->GetIsolate(),
        V8ThrowDOMException::CreateOrEmpty(
            script_state->GetIsolate(), DOMExceptionCode::kAbortError,
            "signal is aborted with undefined reason"));
  } else {
    abort_reason_ = reason;
  }
}

void AbortSignal::RunAbortSteps() {
  for (AbortSignal::AlgorithmHandle* handle : abort_algorithms_) {
    CHECK(handle);
    CHECK(handle->GetAlgorithm());
    handle->GetAlgorithm()->Run();
  }

  DispatchEvent(*Event::Create(event_type_names::kAbort));
}

void AbortSignal::Trace(Visitor* visitor) const {
  visitor->Trace(abort_reason_);
  visitor->Trace(execution_context_);
  visitor->Trace(abort_algorithms_);
  visitor->Trace(composition_manager_);
  EventTarget::Trace(visitor);
}

AbortSignalCompositionManager* AbortSignal::GetCompositionManager(
    AbortSignalCompositionType type) {
  if (type == AbortSignalCompositionType::kAbort) {
    return composition_manager_.Get();
  }
  return nullptr;
}

void AbortSignal::DetachFromController() {
  if (aborted()) {
    return;
  }
  composition_manager_->Settle();
}

void AbortSignal::OnSignalSettled(AbortSignalCompositionType type) {
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
  return !composition_manager_->IsSettled();
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
  if (signal_type_ != SignalType::kComposite) {
    return;
  }
  std::optional<AbortSignalCompositionType> composition_type;
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
  if (GetCompositionManager(*composition_type)) {
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
  return composition_type == AbortSignalCompositionType::kAbort &&
         composition_manager_->IsSettled();
}

AbortSignal::AlgorithmHandle::AlgorithmHandle(AbortSignal::Algorithm* algorithm,
                                              AbortSignal* signal)
    : algorithm_(algorithm), signal_(signal) {
  CHECK(algorithm_);
  CHECK(signal_);
}

AbortSignal::AlgorithmHandle::~AlgorithmHandle() = default;

void AbortSignal::AlgorithmHandle::Trace(Visitor* visitor) const {
  visitor->Trace(algorithm_);
  visitor->Trace(signal_);
}

}  // namespace blink
