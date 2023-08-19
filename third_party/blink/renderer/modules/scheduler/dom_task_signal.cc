// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

#include <utility>

#include "base/functional/callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_priority_change_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_task_signal_any_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_taskpriority_tasksignal.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_manager.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/task_priority_change_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

class RepeatingCallbackAlgorithm final : public DOMTaskSignal::Algorithm {
 public:
  explicit RepeatingCallbackAlgorithm(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}
  ~RepeatingCallbackAlgorithm() override = default;

  void Run() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

// static
DOMTaskSignal* DOMTaskSignal::CreateFixedPriorityTaskSignal(
    ScriptState* script_state,
    const AtomicString& priority) {
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    HeapVector<Member<AbortSignal>> source_abort_signals;
    return MakeGarbageCollected<DOMTaskSignal>(script_state, priority, nullptr,
                                               source_abort_signals);
  } else {
    return MakeGarbageCollected<DOMTaskSignal>(
        ExecutionContext::From(script_state), priority, SignalType::kInternal);
  }
}

DOMTaskSignal::DOMTaskSignal(ExecutionContext* context,
                             const AtomicString& priority,
                             SignalType signal_type)
    : AbortSignal(context, signal_type), priority_(priority) {
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    DCHECK_NE(signal_type, AbortSignal::SignalType::kComposite);
    priority_composition_manager_ =
        MakeGarbageCollected<SourceSignalCompositionManager>(
            *this, AbortSignalCompositionType::kPriority);
  }
}

DOMTaskSignal::DOMTaskSignal(
    ScriptState* script_state,
    const AtomicString& priority,
    DOMTaskSignal* priority_source_signal,
    HeapVector<Member<AbortSignal>>& abort_source_signals)
    : AbortSignal(script_state, abort_source_signals), priority_(priority) {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());

  HeapVector<Member<AbortSignal>> signals;
  if (priority_source_signal) {
    signals.push_back(priority_source_signal);
  }
  priority_composition_manager_ =
      MakeGarbageCollected<DependentSignalCompositionManager>(
          *this, AbortSignalCompositionType::kPriority, signals);
}

DOMTaskSignal::~DOMTaskSignal() = default;

DOMTaskSignal* DOMTaskSignal::any(ScriptState* script_state,
                                  HeapVector<Member<AbortSignal>> signals,
                                  TaskSignalAnyInit* init) {
  DOMTaskSignal* priority_source = init->priority()->IsTaskSignal()
                                       ? init->priority()->GetAsTaskSignal()
                                       : nullptr;
  AtomicString priority = priority_source
                              ? priority_source->priority()
                              : init->priority()->GetAsTaskPriority();
  return MakeGarbageCollected<DOMTaskSignal>(script_state, priority,
                                             priority_source, signals);
}

AtomicString DOMTaskSignal::priority() {
  return priority_;
}

DOMTaskSignal::AlgorithmHandle* DOMTaskSignal::AddPriorityChangeAlgorithm(
    base::RepeatingClosure algorithm) {
  CHECK_NE(GetSignalType(), SignalType::kInternal);
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled() &&
      priority_composition_manager_->IsSettled()) {
    return nullptr;
  }
  auto* callback_algorithm =
      MakeGarbageCollected<RepeatingCallbackAlgorithm>(std::move(algorithm));
  auto* handle =
      MakeGarbageCollected<AlgorithmHandle>(callback_algorithm, this);
  // This always appends since `handle` is not already in the collection.
  priority_change_algorithms_.insert(handle);
  return handle;
}

void DOMTaskSignal::SignalPriorityChange(const AtomicString& priority,
                                         ExceptionState& exception_state) {
  CHECK_NE(GetSignalType(), SignalType::kInternal);
  if (is_priority_changing_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Cannot change priority when a prioritychange event is in progress.");
    return;
  }
  if (priority_ == priority)
    return;
  is_priority_changing_ = true;
  const AtomicString previous_priority = priority_;
  priority_ = priority;

  for (AlgorithmHandle* handle : priority_change_algorithms_) {
    handle->GetAlgorithm()->Run();
  }

  auto* init = TaskPriorityChangeEventInit::Create();
  init->setPreviousPriority(previous_priority);
  DispatchEvent(*TaskPriorityChangeEvent::Create(
      event_type_names::kPrioritychange, init));

  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    if (auto* source_signal_manager = DynamicTo<SourceSignalCompositionManager>(
            *priority_composition_manager_.Get())) {
      // Dependents can be added while dispatching events, but none are removed
      // since having an active iterator will strongify weak references, making
      // the following iteration safe. Signaling priority change on newly added
      // dependent signals has no effect since the new priority is already set.
      for (auto& abort_signal : source_signal_manager->GetDependentSignals()) {
        To<DOMTaskSignal>(abort_signal.Get())
            ->SignalPriorityChange(priority, exception_state);
      }
    }
  }

  is_priority_changing_ = false;
}

void DOMTaskSignal::Trace(Visitor* visitor) const {
  AbortSignal::Trace(visitor);
  visitor->Trace(priority_change_algorithms_);
  visitor->Trace(priority_composition_manager_);
}

bool DOMTaskSignal::HasFixedPriority() const {
  if (RuntimeEnabledFeatures::AbortSignalCompositionEnabled()) {
    return priority_composition_manager_->IsSettled();
  }
  return GetSignalType() == SignalType::kInternal;
}

void DOMTaskSignal::DetachFromController() {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  AbortSignal::DetachFromController();

  priority_composition_manager_->Settle();
}

AbortSignalCompositionManager* DOMTaskSignal::GetCompositionManager(
    AbortSignalCompositionType composition_type) {
  DCHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  if (composition_type != AbortSignalCompositionType::kPriority) {
    return AbortSignal::GetCompositionManager(composition_type);
  }
  return priority_composition_manager_;
}

void DOMTaskSignal::OnSignalSettled(
    AbortSignalCompositionType composition_type) {
  if (composition_type == AbortSignalCompositionType::kPriority) {
    priority_change_algorithms_.clear();
  }
  AbortSignal::OnSignalSettled(composition_type);
}

bool DOMTaskSignal::IsSettledFor(
    AbortSignalCompositionType composition_type) const {
  CHECK(RuntimeEnabledFeatures::AbortSignalCompositionEnabled());
  if (composition_type == AbortSignalCompositionType::kPriority) {
    return priority_composition_manager_->IsSettled();
  }
  return AbortSignal::IsSettledFor(composition_type);
}

}  // namespace blink
