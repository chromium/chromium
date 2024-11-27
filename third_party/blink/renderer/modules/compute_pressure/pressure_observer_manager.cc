// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/pressure_update.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using device::mojom::blink::PressureSource;

namespace blink {

namespace {

PressureSource V8PressureSourceToPressureSource(V8PressureSource::Enum source) {
  switch (source) {
    case V8PressureSource::Enum::kCpu:
      return PressureSource::kCpu;
  }
  NOTREACHED();
}

}  // namespace

// static
const char PressureObserverManager::kSupplementName[] =
    "PressureObserverManager";

// static
PressureObserverManager* PressureObserverManager::From(
    ExecutionContext* context) {
  PressureObserverManager* manager =
      Supplement<ExecutionContext>::From<PressureObserverManager>(context);
  if (!manager) {
    manager = MakeGarbageCollected<PressureObserverManager>(context);
    Supplement<ExecutionContext>::ProvideTo(*context, manager);
  }
  return manager;
}

PressureObserverManager::PressureObserverManager(ExecutionContext* context)
    : ExecutionContextLifecycleStateObserver(context),
      Supplement<ExecutionContext>(*context),
      pressure_manager_(context) {
  UpdateStateIfNeeded();
  for (const auto& source : PressureObserver::knownSources()) {
    source_to_client_.insert(
        source.AsEnum(),
        MakeGarbageCollected<PressureClientImpl>(context, this));
  }
}

PressureObserverManager::~PressureObserverManager() = default;

void PressureObserverManager::AddObserver(V8PressureSource::Enum source,
                                          PressureObserver* observer) {
  PressureClientImpl* client = source_to_client_.at(source);
  client->AddObserver(observer);
  const PressureClientImpl::State state = client->state();
  if (state == PressureClientImpl::State::kUninitialized) {
    client->set_state(PressureClientImpl::State::kInitializing);
    EnsureConnection();
    // Not connected to the browser side for `source` yet. Make the binding.
    pressure_manager_->AddClient(
        V8PressureSourceToPressureSource(source),
        WTF::BindOnce(&PressureObserverManager::DidAddClient,
                      WrapWeakPersistent(this), source));
  } else if (state == PressureClientImpl::State::kInitialized) {
    observer->OnBindingSucceeded(source);
  }
}

void PressureObserverManager::RemoveObserver(V8PressureSource::Enum source,
                                             PressureObserver* observer) {
  PressureClientImpl* client = source_to_client_.at(source);
  client->RemoveObserver(observer);
  if (client->state() == PressureClientImpl::State::kUninitialized) {
    ResetPressureManagerIfNeeded();
  }
}

void PressureObserverManager::RemoveObserverFromAllSources(
    PressureObserver* observer) {
  for (auto source : source_to_client_.Keys()) {
    RemoveObserver(source, observer);
  }
}

void PressureObserverManager::ContextDestroyed() {
  Reset();
}

void PressureObserverManager::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  // TODO(https://crbug.com/1186433): Disconnect and re-establish a connection
  // when frozen or send a disconnect event.
}

void PressureObserverManager::Trace(Visitor* visitor) const {
  visitor->Trace(pressure_manager_);
  visitor->Trace(source_to_client_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

void PressureObserverManager::EnsureConnection() {
  CHECK(GetExecutionContext());

  if (pressure_manager_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kUserInteraction);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      pressure_manager_.BindNewPipeAndPassReceiver(task_runner));
  pressure_manager_.set_disconnect_handler(WTF::BindOnce(
      &PressureObserverManager::OnConnectionError, WrapWeakPersistent(this)));
}

void PressureObserverManager::OnConnectionError() {
  for (PressureClientImpl* client : source_to_client_.Values()) {
    // Take a snapshot so as to safely iterate.
    HeapVector<Member<PressureObserver>> observers(client->observers());
    for (const auto& observer : observers) {
      observer->OnConnectionError();
    }
  }
  Reset();
}

void PressureObserverManager::ResetPressureManagerIfNeeded() {
  if (base::ranges::all_of(
          source_to_client_.Values(), [](const PressureClientImpl* client) {
            return client->state() == PressureClientImpl::State::kUninitialized;
          })) {
    pressure_manager_.reset();
  }
}

void PressureObserverManager::Reset() {
  for (PressureClientImpl* client : source_to_client_.Values()) {
    client->Reset();
  }
  pressure_manager_.reset();
}

void PressureObserverManager::DidAddClient(
    V8PressureSource::Enum source,
    device::mojom::blink::PressureManagerAddClientResultPtr result) {
  PressureClientImpl* client = source_to_client_.at(source);
  // PressureClientImpl may be reset by PressureObserver's
  // unobserve()/disconnect() before this function is called.
  if (client->state() != PressureClientImpl::State::kInitializing) {
    return;
  }
  CHECK(pressure_manager_.is_bound());

  // Take a snapshot so as to safely iterate.
  HeapVector<Member<PressureObserver>> observers(client->observers());
  switch (result->which()) {
    case device::mojom::blink::PressureManagerAddClientResult::Tag::
        kPressureClient: {
      client->set_state(PressureClientImpl::State::kInitialized);
      client->BindPressureClient(std::move(result->get_pressure_client()));
      for (const auto& observer : observers) {
        observer->OnBindingSucceeded(source);
      }
      break;
    }
    case device::mojom::blink::PressureManagerAddClientResult::Tag::kError: {
      switch (result->get_error()) {
        case device::mojom::blink::PressureManagerAddClientError::kNotSupported:
          client->Reset();
          ResetPressureManagerIfNeeded();
          for (const auto& observer : observers) {
            observer->OnBindingFailed(source,
                                      DOMExceptionCode::kNotSupportedError);
          }
          break;
      }
      break;
    }
  }
}

}  // namespace blink
