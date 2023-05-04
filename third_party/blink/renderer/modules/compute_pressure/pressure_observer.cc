// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer.h"

#include "base/ranges/algorithm.h"
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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

constexpr char kFeaturePolicyBlocked[] =
    "Access to the feature \"compute pressure\" is disallowed by permissions "
    "policy.";

}  // namespace

PressureObserver::PressureObserver(V8PressureUpdateCallback* observer_callback,
                                   PressureObserverOptions* options,
                                   ExceptionState& exception_state)
    : observer_callback_(observer_callback),
      sample_rate_(options->sampleRate()) {
  if (sample_rate_ <= 0.0) {
    exception_state.ThrowRangeError("sampleRate must be positive");
    return;
  }
}

PressureObserver::~PressureObserver() = default;

// static
PressureObserver* PressureObserver::Create(V8PressureUpdateCallback* callback,
                                           PressureObserverOptions* options,
                                           ExceptionState& exception_state) {
  return MakeGarbageCollected<PressureObserver>(callback, options,
                                                exception_state);
}

// static
wtf_size_t PressureObserver::ToSourceIndex(V8PressureSource::Enum source) {
  wtf_size_t index = static_cast<wtf_size_t>(source);
  CHECK_LT(index, V8PressureSource::kEnumSize);
  return index;
}

// static
Vector<V8PressureSource> PressureObserver::supportedSources() {
  return Vector<V8PressureSource>(
      {V8PressureSource(V8PressureSource::Enum::kCpu)});
}

ScriptPromise PressureObserver::observe(ScriptState* script_state,
                                        V8PressureSource source,
                                        ExceptionState& exception_state) {
  if (!base::FeatureList::IsEnabled(blink::features::kComputePressure)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Compute Pressure API is not available.");
    return ScriptPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return ScriptPromise();
  }

  // Checks whether the document is allowed by Permissions Policy to call
  // Compute Pressure API.
  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kComputePressure,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
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
  if (!manager_)
    return;

  // https://wicg.github.io/compute-pressure/#the-unobserve-method
  manager_->RemoveObserver(source.AsEnum(), this);
  last_record_map_[ToSourceIndex(source.AsEnum())].Clear();
  // Reject all pending promises for `source`.
  RejectPendingResolvers(source.AsEnum(), DOMExceptionCode::kNotSupportedError,
                         "Called unobserve method.");
  records_.erase(base::ranges::remove_if(records_,
                                         [source](const auto& record) {
                                           return record->source() == source;
                                         }),
                 records_.end());
}

void PressureObserver::disconnect() {
  // Wrong order of calls.
  if (!manager_)
    return;

  // https://wicg.github.io/compute-pressure/#the-disconnect-method
  manager_->RemoveObserverFromAllSources(this);
  for (auto& last_record : last_record_map_)
    last_record.Clear();
  // Reject all pending promises.
  for (const auto& source : supportedSources()) {
    RejectPendingResolvers(source.AsEnum(),
                           DOMExceptionCode::kNotSupportedError,
                           "Called disconnect method.");
  }
  records_.clear();
}

void PressureObserver::Trace(blink::Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(observer_callback_);
  for (const auto& last_record : last_record_map_)
    visitor->Trace(last_record);
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
  if (!PassesRateTest(source, timestamp))
    return;

  if (!HasChangeInData(source, state)) {
    return;
  }

  auto* record = MakeGarbageCollected<PressureRecord>(source, state, timestamp);

  last_record_map_[ToSourceIndex(source)] = record;

  // This should happen infrequently since `records_` is supposed
  // to be emptied at every callback invoking or takeRecords().
  if (records_.size() >= kMaxQueuedRecords)
    records_.erase(records_.begin());

  records_.push_back(record);
  CHECK_LE(records_.size(), kMaxQueuedRecords);

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
  for (const auto& source : supportedSources()) {
    RejectPendingResolvers(source.AsEnum(),
                           DOMExceptionCode::kNotSupportedError,
                           "Connection error.");
  }
}

void PressureObserver::ReportToCallback(ExecutionContext* execution_context) {
  CHECK(observer_callback_);
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

// https://wicg.github.io/compute-pressure/#dfn-passes-rate-test
bool PressureObserver::PassesRateTest(
    V8PressureSource::Enum source,
    const DOMHighResTimeStamp& timestamp) const {
  const auto& last_record = last_record_map_[ToSourceIndex(source)];

  if (!last_record)
    return true;

  const double time_delta_milliseconds = timestamp - last_record->time();
  const double interval_seconds = 1.0 / sample_rate_;
  return (time_delta_milliseconds / 1000.0) >= interval_seconds;
}

// https://wicg.github.io/compute-pressure/#dfn-has-change-in-data
bool PressureObserver::HasChangeInData(V8PressureSource::Enum source,
                                       V8PressureState::Enum state) const {
  const auto& last_record = last_record_map_[ToSourceIndex(source)];

  if (!last_record)
    return true;

  return last_record->state() != state;
}

void PressureObserver::ResolvePendingResolvers(V8PressureSource::Enum source) {
  for (const auto& resolver : pending_resolvers_[ToSourceIndex(source)]) {
    ScriptState* const script_state = resolver->GetScriptState();
    // Check if callback's resolver is still valid.
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      continue;
    }
    resolver->Resolve();
  }
  pending_resolvers_[ToSourceIndex(source)].clear();
}

void PressureObserver::RejectPendingResolvers(V8PressureSource::Enum source,
                                              DOMExceptionCode exception_code,
                                              const String& message) {
  for (const auto& resolver : pending_resolvers_[ToSourceIndex(source)]) {
    ScriptState* const script_state = resolver->GetScriptState();
    // Check if callback's resolver is still valid.
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       script_state)) {
      continue;
    }
    // Enter into resolver's context to support creating DOMException.
    ScriptState::Scope script_state_scope(resolver->GetScriptState());
    resolver->RejectWithDOMException(exception_code, message);
  }
  pending_resolvers_[ToSourceIndex(source)].clear();
}

}  // namespace blink
