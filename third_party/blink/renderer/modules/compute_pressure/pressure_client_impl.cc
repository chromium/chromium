// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_client_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "services/device/public/mojom/pressure_manager.mojom-blink.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"
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
  NOTREACHED_NORETURN();
}

V8PressureSource::Enum PressureSourceToV8PressureSource(PressureSource source) {
  switch (source) {
    case PressureSource::kCpu:
      return V8PressureSource::Enum::kCpu;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

PressureClientImpl::PressureClientImpl(ExecutionContext* context,
                                       PressureObserverManager* manager)
    : ExecutionContextClient(context),
      manager_(manager),
      receiver_(this, context) {}

PressureClientImpl::~PressureClientImpl() = default;

void PressureClientImpl::OnPressureUpdated(
    device::mojom::blink::PressureUpdatePtr update) {
  auto source = PressureSourceToV8PressureSource(update->source);
  // New observers may be created and added. Take a snapshot so as
  // to safely iterate.
  HeapVector<Member<blink::PressureObserver>> observers(observers_);
  for (const auto& observer : observers) {
    observer->OnUpdate(GetExecutionContext(), source,
                       PressureStateToV8PressureState(update->state),
                       ConvertTimeToDOMHighResTimeStamp(update->timestamp));
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

mojo::PendingRemote<device::mojom::blink::PressureClient>
PressureClientImpl::BindNewPipeAndPassRemote() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  auto remote = receiver_.BindNewPipeAndPassRemote(std::move(task_runner));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&PressureClientImpl::Reset, WrapWeakPersistent(this)));
  return remote;
}

void PressureClientImpl::Reset() {
  state_ = State::kUninitialized;
  observers_.clear();
  receiver_.reset();
}

void PressureClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(receiver_);
  visitor->Trace(observers_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
