// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"

#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_record.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_record.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_source_index.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

constexpr char kFeaturePolicyBlocked[] =
    "Access to the feature \"compute pressure\" is disallowed by permissions "
    "policy.";

}  // namespace

PressureObserver::PressureObserver(V8PressureUpdateCallback* observer_callback)
    : observer_callback_(observer_callback) {}

PressureObserver::~PressureObserver() = default;

// static
PressureObserver* PressureObserver::Create(V8PressureUpdateCallback* callback) {
  return MakeGarbageCollected<PressureObserver>(callback);
}

// static
Vector<V8PressureSource> PressureObserver::knownSources() {
  return Vector<V8PressureSource>(
      {V8PressureSource(V8PressureSource::Enum::kCpu)});
}

ScriptPromise<IDLUndefined> PressureObserver::observe(
    ScriptState* script_state,
    V8PressureSource source,
    PressureObserverOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return EmptyPromise();
  }

  // Checks whether the document is allowed by Permissions Policy to call
  // Compute Pressure API.
  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kComputePressure,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kFeaturePolicyBlocked);
    return EmptyPromise();
  }

  sample_interval_ = options->sampleInterval();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_resolvers_[ToSourceIndex(source.AsEnum())].insert(resolver);

  if (!manager_) {
    manager_ = PressureObserverManager::From(execution_context);
  }
  manager_->AddObserver(source.AsEnum(), this);

  return resolver->Promise();
}

void PressureObserver::unobserve(V8PressureSource source) {
  // Wrong order of calls.
  if (!manager_) {
    return;
  }
  const auto source_index = ToSourceIndex(source.AsEnum());
  // https://w3c.github.io/compute-pressure/#the-unobserve-method
  manager_->RemoveObserver(source.AsEnum(), this);
  last_record_map_[source_index].Clear();
  after_penalty_records_[source_index].Clear();
  pending_delayed_report_to_callback_[source_index].Cancel();
  // Reject all pending promises for `source`.
  RejectPendingResolvers(source.AsEnum(), DOMExceptionCode::kAbortError,
                         "Called unobserve method.");
  records_.erase(base::ranges::remove_if(records_,
                                         [source](const auto& record) {
                                           return record->source() == source;
                                         }),
                 records_.end());
}

void PressureObserver::disconnect() {
  // Wrong order of calls.
  if (!manager_) {
    return;
  }
  // https://w3c.github.io/compute-pressure/#the-disconnect-method
  manager_->RemoveObserverFromAllSources(this);
  for (auto& last_record : last_record_map_) {
    last_record.Clear();
  }
  for (auto& after_penalty_record : after_penalty_records_) {
    after_penalty_record.Clear();
  }

  for (auto& pending_callback : pending_delayed_report_to_callback_) {
    pending_callback.Cancel();
  }

  // Reject all pending promises.
  for (const auto& source : knownSources()) {
    RejectPendingResolvers(source.AsEnum(), DOMExceptionCode::kAbortError,
                           "Called disconnect method.");
  }
  records_.clear();
}

void PressureObserver::Trace(blink::Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(observer_callback_);
  for (const auto& after_penalty_record : after_penalty_records_) {
    visitor->Trace(after_penalty_record);
  }
  for (const auto& last_record : last_record_map_) {
    visitor->Trace(last_record);
  }
  for (const auto& pending_resolver_set : pending_resolvers_) {
    visitor->Trace(pending_resolver_set);
  }
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

void PressureObserver::OnUpdate(ExecutionContext* execution_context,
                                V8PressureSource::Enum source,
                                V8PressureState::Enum state,
                                DOMHighResTimeStamp timestamp) {
  if (!PassesRateTest(source, timestamp)) {
    return;
  }

  if (!HasChangeInData(source, state)) {
    return;
  }

  auto* record = MakeGarbageCollected<PressureRecord>(source, state, timestamp);

  if (base::FeatureList::IsEnabled(
          features::kComputePressureRateObfuscationMitigation)) {
    const auto source_index = ToSourceIndex(source);
    // Steps 4.5.1 and 4.5.2
    // https://w3c.github.io/compute-pressure/#dfn-data-delivery
    if (pending_delayed_report_to_callback_[source_index].IsActive()) {
      after_penalty_records_[source_index] = record;
      return;
    }

    change_rate_monitor_.ResetIfNeeded();
    change_rate_monitor_.IncreaseChangeCount(source);

    if (!PassesRateObfuscation(source)) {
      // Steps 4.6.1 and 4.6.2
      // https://w3c.github.io/compute-pressure/#dfn-data-delivery
      after_penalty_records_[source_index] = record;
      pending_delayed_report_to_callback_[source_index] =
          PostDelayedCancellableTask(
              *execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI),
              FROM_HERE,
              WTF::BindOnce(&PressureObserver::QueueAfterPenaltyRecord,
                            WrapWeakPersistent(this),
                            WrapWeakPersistent(execution_context), source),
              change_rate_monitor_.penalty_duration());
      change_rate_monitor_.ResetChangeCount(source);
      return;
    }
  }

  QueuePressureRecord(execution_context, source, record);
}

// Steps 4.6.3.1.1-3 of
// https://w3c.github.io/compute-pressure/#dfn-data-delivery
void PressureObserver::QueueAfterPenaltyRecord(
    ExecutionContext* execution_context,
    V8PressureSource::Enum source) {
  const auto source_index = ToSourceIndex(source);
  CHECK(after_penalty_records_[source_index]);
  auto& record = after_penalty_records_[source_index];
  QueuePressureRecord(execution_context, source, record);
}

// https://w3c.github.io/compute-pressure/#queue-a-pressurerecord
void PressureObserver::QueuePressureRecord(ExecutionContext* execution_context,
                                           V8PressureSource::Enum source,
                                           PressureRecord* record) {
  // This should happen infrequently since `records_` is supposed
  // to be emptied at every callback invoking or takeRecords().
  if (records_.size() >= kMaxQueuedRecords)
    records_.erase(records_.begin());

  records_.push_back(record);
  CHECK_LE(records_.size(), kMaxQueuedRecords);

  last_record_map_[ToSourceIndex(source)] = record;
  if (pending_report_to_callback_.IsActive())
    return;

  pending_report_to_callback_ = PostCancellableTask(
      *execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI), FROM_HERE,
      WTF::BindOnce(&PressureObserver::ReportToCallback,
                    WrapWeakPersistent(this),
                    WrapWeakPersistent(execution_context)));
}

void PressureObserver::OnBindingSucceeded(V8PressureSource::Enum source) {
  ResolvePendingResolvers(source);
}

void PressureObserver::OnBindingFailed(V8PressureSource::Enum source,
                                       DOMExceptionCode exception_code) {
  RejectPendingResolvers(source, exception_code,
                         "Not available on this platform.");
}

void PressureObserver::OnConnectionError() {
  for (const auto& source : knownSources()) {
    RejectPendingResolvers(source.AsEnum(),
                           DOMExceptionCode::kNotSupportedError,
                           "Connection error.");
  }
}

void PressureObserver::ReportToCallback(ExecutionContext* execution_context) {
  CHECK(observer_callback_);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return;
  }

  // Cleared by takeRecords, for example.
  if (records_.empty()) {
    return;
  }

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

// https://w3c.github.io/compute-pressure/#dfn-passes-rate-test
bool PressureObserver::PassesRateTest(
    V8PressureSource::Enum source,
    const DOMHighResTimeStamp& timestamp) const {
  const auto& last_record = last_record_map_[ToSourceIndex(source)];

  if (!last_record)
    return true;

  const double time_delta_milliseconds = timestamp - last_record->time();
  return time_delta_milliseconds >= static_cast<double>(sample_interval_);
}

// https://w3c.github.io/compute-pressure/#dfn-has-change-in-data
bool PressureObserver::HasChangeInData(V8PressureSource::Enum source,
                                       V8PressureState::Enum state) const {
  const auto& last_record = last_record_map_[ToSourceIndex(source)];

  if (!last_record)
    return true;

  return last_record->state() != state;
}

// This function only checks the status of the rate obfuscation test.
// Incrementing of change count should happen before this call as described in
// https://w3c.github.io/compute-pressure/#dfn-passes-rate-obfuscation-test
bool PressureObserver::PassesRateObfuscation(
    V8PressureSource::Enum source) const {
  return !change_rate_monitor_.ChangeCountExceedsLimit(source);
}

void PressureObserver::ResolvePendingResolvers(V8PressureSource::Enum source) {
  const auto source_index = ToSourceIndex(source);
  for (const auto& resolver : pending_resolvers_[source_index]) {
    resolver->Resolve();
  }
  pending_resolvers_[source_index].clear();
}

void PressureObserver::RejectPendingResolvers(V8PressureSource::Enum source,
                                              DOMExceptionCode exception_code,
                                              const String& message) {
  const auto source_index = ToSourceIndex(source);
  for (const auto& resolver : pending_resolvers_[source_index]) {
    resolver->RejectWithDOMException(exception_code, message);
  }
  pending_resolvers_[source_index].clear();
}

}  // namespace blink
