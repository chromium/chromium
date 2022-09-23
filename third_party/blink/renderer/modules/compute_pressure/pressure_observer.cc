// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

PressureObserver::PressureObserver(V8PressureUpdateCallback* observer_callback,
                                   PressureObserverOptions* options)
    // TODO(crbug.com/1356529): sampleRate in PressureObserverOptions needs to
    // be processed and passed to lower stacks for compute pressure
    // implementation.
    : observer_callback_(observer_callback), options_(options) {}

PressureObserver::~PressureObserver() = default;

// static
PressureObserver* PressureObserver::Create(V8PressureUpdateCallback* callback,
                                           PressureObserverOptions* options) {
  return MakeGarbageCollected<PressureObserver>(callback, options);
}

// static
Vector<V8PressureSource> PressureObserver::supportedSources() {
  return Vector<V8PressureSource>(
      {V8PressureSource(V8PressureSource::Enum::kCpu)});
}

// TODO(crbug.com/1308303): Remove ScriptPromise to match specs, whenever
// we redesign the interface with browser.
ScriptPromise PressureObserver::observe(ScriptState* script_state,
                                        V8PressureSource source,
                                        ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return ScriptPromise();
  }

  if (!manager_) {
    LocalDOMWindow* window = To<LocalDOMWindow>(execution_context);
    manager_ = PressureObserverManager::From(*window);
  }

  return manager_->AddObserver(source, this, script_state, exception_state);
}

// TODO(crbug.com/1306819): Unobserve is supposed to only stop observing
// one source but should continue to observe other sources.
// For now, since "cpu" is the only source, unobserve() has the same
// functionality as disconnect().
void PressureObserver::unobserve(V8PressureSource source) {
  // Wrong order of calls.
  if (!manager_)
    return;

  // TODO(crbug.com/1306819):
  // 1. observer needs to be dequeued from active observer list of
  // requested source.
  // 2. observer records from the source need to be removed from `records_`
  // For now 'cpu' is the only source.
  manager_->RemoveObserver(source, this);
  switch (source.AsEnum()) {
    case V8PressureSource::Enum::kCpu:
      records_.clear();
      break;
  }
}

void PressureObserver::disconnect() {
  // Wrong order of calls.
  if (!manager_)
    return;

  manager_->RemoveObserverFromAllSources(this);
  records_.clear();
}

void PressureObserver::Trace(blink::Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(options_);
  visitor->Trace(observer_callback_);
  for (const auto& last_record : last_record_map_)
    visitor->Trace(last_record);
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

void PressureObserver::OnUpdate(ExecutionContext* execution_context,
                                V8PressureSource::Enum source,
                                V8PressureState::Enum state,
                                DOMHighResTimeStamp timestamp) {
  if (!HasChangeInData(source, state))
    return;

  auto* record = PressureRecord::Create();
  record->setSource(V8PressureSource(source));
  record->setState(V8PressureState(state));
  record->setTime(timestamp);

  last_record_map_[static_cast<size_t>(source)] = record;

  // This should happen infrequently since `records_` is supposed
  // to be emptied at every callback invoking or takeRecords().
  if (records_.size() >= kMaxQueuedRecords)
    records_.erase(records_.begin());

  records_.push_back(record);
  DCHECK_LE(records_.size(), kMaxQueuedRecords);

  if (pending_report_to_callback_.IsActive())
    return;

  pending_report_to_callback_ = PostCancellableTask(
      *execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI), FROM_HERE,
      WTF::BindOnce(&PressureObserver::ReportToCallback,
                    WrapWeakPersistent(this),
                    WrapWeakPersistent(execution_context)));
}

void PressureObserver::ReportToCallback(ExecutionContext* execution_context) {
  DCHECK(observer_callback_);
  if (!execution_context || execution_context->IsContextDestroyed())
    return;

  // Cleared by takeRecords, for example.
  if (records_.empty())
    return;

  HeapVector<Member<PressureRecord>, kMaxQueuedRecords> records;
  records_.swap(records);
  observer_callback_->InvokeAndReportException(this, records, this);
}

HeapVector<Member<PressureRecord>> PressureObserver::takeRecords() {
  // This method clears records_.
  HeapVector<Member<PressureRecord>, kMaxQueuedRecords> records;
  records.swap(records_);
  return records;
}

// https://wicg.github.io/compute-pressure/#dfn-has-change-in-data
bool PressureObserver::HasChangeInData(V8PressureSource::Enum source,
                                       V8PressureState::Enum state) const {
  const auto& last_record = last_record_map_[static_cast<size_t>(source)];
  return last_record ? last_record->state() != state : true;
}

}  // namespace blink
