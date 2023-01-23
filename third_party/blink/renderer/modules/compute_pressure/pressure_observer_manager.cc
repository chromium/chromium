// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using device::mojom::blink::PressureFactor;
using device::mojom::blink::PressureState;

namespace blink {

namespace {

constexpr auto ToSourceIndex = &blink::PressureObserver::ToSourceIndex;

V8PressureFactor::Enum PressureFactorToV8PressureFactor(PressureFactor factor) {
  switch (factor) {
    case PressureFactor::kThermal:
      return V8PressureFactor::Enum::kThermal;
    case PressureFactor::kPowerSupply:
      return V8PressureFactor::Enum::kPowerSupply;
  }
  NOTREACHED();
}

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

}  // namespace

// static
const char PressureObserverManager::kSupplementName[] =
    "PressureObserverManager";

// static
PressureObserverManager* PressureObserverManager::From(LocalDOMWindow& window) {
  PressureObserverManager* manager =
      Supplement<LocalDOMWindow>::From<PressureObserverManager>(window);
  if (!manager) {
    manager = MakeGarbageCollected<PressureObserverManager>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, manager);
  }
  return manager;
}

PressureObserverManager::PressureObserverManager(LocalDOMWindow& window)
    : ExecutionContextLifecycleStateObserver(&window),
      Supplement<LocalDOMWindow>(window),
      pressure_manager_(GetSupplementable()->GetExecutionContext()),
      receiver_(this, GetSupplementable()->GetExecutionContext()) {
  UpdateStateIfNeeded();
}

PressureObserverManager::~PressureObserverManager() = default;

void PressureObserverManager::AddObserver(V8PressureSource::Enum source,
                                          blink::PressureObserver* observer) {
  observers_[ToSourceIndex(source)].insert(observer);

  if (state_ == State::kUninitialized) {
    DCHECK(!receiver_.is_bound());
    state_ = State::kInitializing;
    EnsureServiceConnection();
    // Not connected to the browser process yet. Make the binding.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    pressure_manager_->AddClient(
        receiver_.BindNewPipeAndPassRemote(std::move(task_runner)),
        WTF::BindOnce(&PressureObserverManager::DidAddClient,
                      WrapWeakPersistent(this), source));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &PressureObserverManager::Reset, WrapWeakPersistent(this)));
  } else if (state_ == State::kInitialized) {
    observer->OnBindingSucceeded(source);
  }
}

void PressureObserverManager::RemoveObserver(
    V8PressureSource::Enum source,
    blink::PressureObserver* observer) {
  observers_[ToSourceIndex(source)].erase(observer);

  // Disconnected from the browser process only when PressureObserverManager is
  // active and there is no other observers.
  if (receiver_.is_bound() && observers_[ToSourceIndex(source)].empty()) {
    // TODO(crbug.com/1342184): Consider other sources.
    // For now, "cpu" is the only source, so disconnect directly.
    Reset();
  }
}

void PressureObserverManager::RemoveObserverFromAllSources(
    blink::PressureObserver* observer) {
  // TODO(crbug.com/1342184): Consider other sources.
  // For now, "cpu" is the only source.
  auto source = V8PressureSource::Enum::kCpu;
  RemoveObserver(source, observer);
}

void PressureObserverManager::ContextDestroyed() {
  Reset();
}

void PressureObserverManager::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  // TODO(https://crbug.com/1186433): Disconnect and re-establish a connection
  // when frozen or send a disconnect event.
}

void PressureObserverManager::OnPressureUpdated(
    device::mojom::blink::PressureUpdatePtr update) {
  if (!PassesPrivacyTest())
    return;

  // New observers may be created and added. Take a snapshot so as
  // to safely iterate.
  //
  // TODO(crbug.com/1342184): Consider other sources.
  // For now, "cpu" is the only source.
  HeapVector<Member<blink::PressureObserver>> observers(
      observers_[ToSourceIndex(V8PressureSource::Enum::kCpu)]);
  for (const auto& observer : observers) {
    Vector<V8PressureFactor> v8_factors;
    for (const auto& factor : update->factors) {
      v8_factors.push_back(
          V8PressureFactor(PressureFactorToV8PressureFactor(factor)));
    }
    // TODO(crbug.com/1342184): Consider other sources.
    // For now, "cpu" is the only source.
    observer->OnUpdate(GetExecutionContext(), V8PressureSource::Enum::kCpu,
                       PressureStateToV8PressureState(update->state),
                       std::move(v8_factors),
                       static_cast<DOMHighResTimeStamp>(
                           update->timestamp.ToJsTimeIgnoringNull()));
  }
}

void PressureObserverManager::Trace(blink::Visitor* visitor) const {
  for (const auto& observer_set : observers_) {
    visitor->Trace(observer_set);
  }
  visitor->Trace(pressure_manager_);
  visitor->Trace(receiver_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void PressureObserverManager::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (pressure_manager_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kUserInteraction);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      pressure_manager_.BindNewPipeAndPassReceiver(task_runner));
  pressure_manager_.set_disconnect_handler(
      WTF::BindOnce(&PressureObserverManager::OnServiceConnectionError,
                    WrapWeakPersistent(this)));
}

// https://wicg.github.io/compute-pressure/#dfn-passes-privacy-test
bool PressureObserverManager::PassesPrivacyTest() const {
  LocalFrame* this_frame = GetSupplementable()->GetFrame();
  // 2. If associated document is not fully active, return false.
  if (GetSupplementable()->IsContextDestroyed() || !this_frame)
    return false;

  // 4. If associated document is same-domain with initiators of active
  // Picture-in-Picture sessions, return true.
  //
  // TODO(crbug.com/1396177): A frame should be able to access to
  // PressureRecord if it is same-domain with initiators of active
  // Picture-in-Picture sessions. However, it is hard to implement now. In
  // current implementation, only the frame that triggers Picture-in-Picture
  // can access to PressureRecord.
  auto& pip_controller =
      PictureInPictureControllerImpl::From(*(this_frame->GetDocument()));
  if (pip_controller.PictureInPictureElement())
    return true;

  // 5. If browsing context is capturing, return true.
  if (this_frame->IsCapturingMedia())
    return true;

  // 7. If top-level browsing context does not have system focus, return false.
  DCHECK(this_frame->GetPage());
  LocalFrame* focused_frame =
      this_frame->GetPage()->GetFocusController().FocusedFrame();
  if (!focused_frame || !focused_frame->IsOutermostMainFrame())
    return false;

  // 9. If origin is same origin-domain with focused document, return true.
  // 10. Otherwise, return false.
  const SecurityOrigin* focused_frame_origin =
      focused_frame->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* this_origin =
      this_frame->GetSecurityContext()->GetSecurityOrigin();
  return focused_frame_origin->CanAccess(this_origin);
}

void PressureObserverManager::OnServiceConnectionError() {
  for (const auto& observer_set : observers_) {
    // Take a snapshot so as to safely iterate.
    HeapVector<Member<blink::PressureObserver>> observers(observer_set);
    for (const auto& observer : observers) {
      observer->OnConnectionError();
    }
  }
  Reset();
}

void PressureObserverManager::Reset() {
  state_ = State::kUninitialized;
  receiver_.reset();
  pressure_manager_.reset();
  for (auto& observer_set : observers_) {
    observer_set.clear();
  }
}

void PressureObserverManager::DidAddClient(
    V8PressureSource::Enum source,
    device::mojom::blink::PressureStatus status) {
  DCHECK_EQ(state_, State::kInitializing);
  DCHECK(receiver_.is_bound());
  DCHECK(pressure_manager_.is_bound());

  // Take a snapshot so as to safely iterate.
  HeapVector<Member<blink::PressureObserver>> observers(
      observers_[ToSourceIndex(source)]);
  switch (status) {
    case device::mojom::blink::PressureStatus::kOk: {
      state_ = State::kInitialized;
      for (const auto& observer : observers) {
        observer->OnBindingSucceeded(source);
      }
      break;
    }
    case device::mojom::blink::PressureStatus::kNotSupported: {
      // TODO(crbug.com/1342184): Consider other sources.
      // For now, "cpu" is the only source.
      Reset();
      for (const auto& observer : observers) {
        observer->OnBindingFailed(source, DOMExceptionCode::kNotSupportedError);
      }
      break;
    }
  }
}

}  // namespace blink
