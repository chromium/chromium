// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_client_impl.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_update.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::PressureSource;
using device::mojom::blink::PressureState;

namespace blink {

namespace {

V8PressureState::Enum PressureStateToV8PressureState(PressureState state) {
  switch (state) {
    case PressureState::kNominal:
      return V8PressureState::Enum::kNominal;
    case PressureState::kFair:
      return V8PressureState::Enum::kFair;
    case PressureState::kSerious:
      return V8PressureState::Enum::kSerious;
    case PressureState::kCritical:
      return V8PressureState::Enum::kCritical;
  }
  NOTREACHED();
}

V8PressureSource::Enum PressureSourceToV8PressureSource(PressureSource source) {
  switch (source) {
    case PressureSource::kCpu:
      return V8PressureSource::Enum::kCpu;
  }
  NOTREACHED();
}

}  // namespace

PressureClientImpl::PressureClientImpl(ExecutionContext* context,
                                       PressureObserverManager* manager)
    : ExecutionContextClient(context),
      manager_(manager),
      associated_receiver_(this, context) {}

PressureClientImpl::~PressureClientImpl() = default;

void PressureClientImpl::OnPressureUpdated(
    mojom::blink::WebPressureUpdatePtr update) {
  auto source = PressureSourceToV8PressureSource(update->source);
  // New observers may be created and added. Take a snapshot so as
  // to safely iterate.
  HeapVector<Member<blink::PressureObserver>> observers(observers_);
  for (const auto& observer : observers) {
    observer->OnUpdate(GetExecutionContext(), source,
                       PressureStateToV8PressureState(update->state),
                       update->own_contribution_estimate,
                       CalculateTimestamp(update->timestamp));
  }
}

void PressureClientImpl::AddObserver(PressureObserver* observer) {
  observers_.insert(observer);
}

void PressureClientImpl::RemoveObserver(PressureObserver* observer) {
  observers_.erase(observer);
  if (observers_.empty()) {
    Reset();
  }
}

mojo::PendingAssociatedRemote<mojom::blink::WebPressureClient>
PressureClientImpl::BindNewEndpointAndPassRemote(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  auto associated_pending_remote =
      associated_receiver_.BindNewEndpointAndPassRemote(task_runner);

  associated_receiver_.set_disconnect_handler(
      BindOnce(&PressureClientImpl::Reset, WrapWeakPersistent(this)));

  return associated_pending_remote;
}

void PressureClientImpl::Reset() {
  state_ = State::kUninitialized;
  observers_.clear();
  associated_receiver_.reset();
}

DOMHighResTimeStamp PressureClientImpl::CalculateTimestamp(
    base::TimeTicks timeticks) const {
  auto* context = GetExecutionContext();
  Performance* performance;
  if (auto* window = DynamicTo<LocalDOMWindow>(context); window) {
    performance = DOMWindowPerformance::performance(*window);
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(context); worker) {
    performance = WorkerGlobalScopePerformance::performance(*worker);
  } else {
    NOTREACHED();
  }
  CHECK(performance);
  return performance->MonotonicTimeToDOMHighResTimeStamp(timeticks);
}

void PressureClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(associated_receiver_);
  visitor->Trace(manager_);
  visitor->Trace(observers_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
