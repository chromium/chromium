// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_manager_supplement.h"

#include <utility>

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_registration_observer.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
DeviceBoundSessionManagerSupplement& DeviceBoundSessionManagerSupplement::From(
    LocalDOMWindow& window) {
  DeviceBoundSessionManagerSupplement* supplement =
      Supplement<LocalDOMWindow>::From<DeviceBoundSessionManagerSupplement>(
          window);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<DeviceBoundSessionManagerSupplement>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

DeviceBoundSessionManagerSupplement::DeviceBoundSessionManagerSupplement(
    LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      ExecutionContextLifecycleObserver(&window),
      manager_(&window),
      receivers_(this, &window) {
  receivers_.set_disconnect_handler(BindRepeating(
      &DeviceBoundSessionManagerSupplement::OnEndpointDisconnected,
      WrapWeakPersistent(this)));
}

network::mojom::blink::RestrictedDeviceBoundSessionManager*
DeviceBoundSessionManagerSupplement::GetManager() {
  // The remote stays bound after its pipe closes, for example when the network
  // service restarts, so bind a new pipe then.
  if (!manager_.is_bound() || !manager_.is_connected()) {
    manager_.reset();
    LocalDOMWindow* window = GetSupplementable();
    // Like `USB::EnsureServiceConnection()`, the only bind site asserts that
    // `observe()` ran the check.
    CHECK(window->CanUseDeviceBoundSessionJsApi());
    window->GetBrowserInterfaceBroker().GetInterface(
        manager_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kInternalDefault)));
  }
  return manager_.get();
}

mojo::ReceiverId DeviceBoundSessionManagerSupplement::RegisterObserver(
    DeviceBoundSessionRegistrationObserver* observer) {
  mojo::PendingAssociatedRemote<
      network::mojom::blink::RestrictedDeviceBoundSessionObserver>
      endpoint;
  mojo::ReceiverId id = receivers_.Add(
      endpoint.InitWithNewEndpointAndPassReceiver(),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalDefault));
  observers_.insert(id, observer);
  GetManager()->SubscribeToRegistrations(
      std::move(endpoint),
      BindOnce(
          &DeviceBoundSessionManagerSupplement::OnExistingSessionsRetrieved,
          WrapWeakPersistent(this), id));
  return id;
}

void DeviceBoundSessionManagerSupplement::UnregisterObserver(
    mojo::ReceiverId id) {
  receivers_.Remove(id);
  observers_.erase(id);
}

bool DeviceBoundSessionManagerSupplement::IsRegistered(
    mojo::ReceiverId id) const {
  return observers_.Contains(id);
}

bool DeviceBoundSessionManagerSupplement::HasObserverForTesting(
    DeviceBoundSessionRegistrationObserver* observer) const {
  for (const auto& entry : observers_) {
    if (entry.value == observer) {
      return true;
    }
  }
  return false;
}

void DeviceBoundSessionManagerSupplement::ContextDestroyed() {
  // `receivers_` closes every endpoint with the context, so nothing can call
  // the observers again.
  observers_.clear();
}

void DeviceBoundSessionManagerSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(manager_);
  visitor->Trace(receivers_);
  visitor->Trace(observers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void DeviceBoundSessionManagerSupplement::OnDeviceBoundSessionRegistered(
    network::mojom::blink::RestrictedDeviceBoundSessionPtr session) {
  // A subscription's endpoint and its entry in `observers_` are added and
  // removed together.
  DeviceBoundSessionRegistrationObserver* observer =
      observers_.at(receivers_.current_receiver());
  CHECK(observer);
  observer->OnDeviceBoundSessionRegistered(std::move(session));
}

void DeviceBoundSessionManagerSupplement::OnExistingSessionsRetrieved(
    mojo::ReceiverId id,
    Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr>
        existing_sessions) {
  // After `disconnect()` and a new `observe()`, the reply to the old
  // subscription can still arrive on the manager pipe. That subscription has
  // ended, so its reply is dropped.
  auto it = observers_.find(id);
  if (it != observers_.end()) {
    it->value->OnExistingSessionsRetrieved(std::move(existing_sessions));
  }
}

void DeviceBoundSessionManagerSupplement::OnEndpointDisconnected() {
  // The endpoint closes when the network service drops the manager, as it does
  // when DBSC is off for the profile, or goes away. The subscription ends
  // then, so the supplement stops keeping its observer alive.
  observers_.erase(receivers_.current_receiver());
}

}  // namespace blink
