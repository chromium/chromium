// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_server.h"

#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(ExecutionContext* context,
                                                     BluetoothDevice* device)
    : ContextLifecycleObserver(context), device_(device), connected_(false) {}

void BluetoothRemoteGATTServer::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void BluetoothRemoteGATTServer::GATTServerDisconnected() {
  DispatchDisconnected();
}

void BluetoothRemoteGATTServer::AddToActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  auto result = active_algorithms_.insert(resolver);
  CHECK(result.is_new_entry);
}

bool BluetoothRemoteGATTServer::RemoveFromActiveAlgorithms(
    ScriptPromiseResolver* resolver) {
  if (!active_algorithms_.Contains(resolver)) {
    return false;
  }
  active_algorithms_.erase(resolver);
  return true;
}

void BluetoothRemoteGATTServer::DisconnectIfConnected() {
  if (connected_) {
    SetConnected(false);
    ClearActiveAlgorithms();
    mojom::blink::WebBluetoothService* service =
        device_->GetBluetooth()->Service();
    service->RemoteServerDisconnect(device_->id());
  }
}

void BluetoothRemoteGATTServer::CleanupDisconnectedDeviceAndFireEvent() {
  DCHECK(connected_);
  SetConnected(false);
  ClearActiveAlgorithms();
  device_->ClearAttributeInstanceMapAndFireEvent();
}

void BluetoothRemoteGATTServer::DispatchDisconnected() {
  if (!connected_) {
    return;
  }
  CleanupDisconnectedDeviceAndFireEvent();
}

void BluetoothRemoteGATTServer::Dispose() {
  DisconnectIfConnected();
  // The pipe to this object must be closed when is marked unreachable to
  // prevent messages from being dispatched before lazy sweeping.
  client_receivers_.Clear();
}

void BluetoothRemoteGATTServer::Trace(blink::Visitor* visitor) {
  visitor->Trace(active_algorithms_);
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void BluetoothRemoteGATTServer::ConnectCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    SetConnected(true);
    resolver->Resolve(this);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothServerClient> client;
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  client_receivers_.Add(this, client.InitWithNewEndpointAndPassReceiver(),
                        std::move(task_runner));

  service->RemoteServerConnect(
      device_->id(), std::move(client),
      WTF::Bind(&BluetoothRemoteGATTServer::ConnectCallback,
                WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* script_state) {
  if (!connected_)
    return;
  CleanupDisconnectedDeviceAndFireEvent();
  client_receivers_.Clear();
  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServerDisconnect(device_->id());
}

// Callback that allows us to resolve the promise with a single service or
// with a vector owning the services.
void BluetoothRemoteGATTServer::GetPrimaryServicesCallback(
    const String& requested_service_uuid,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    base::Optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>>
        services) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::kServicesRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(services);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, services->size());
      resolver->Resolve(device_->GetOrCreateRemoteGATTService(
          std::move(services.value()[0]), true /* isPrimary */, device_->id()));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTService>> gatt_services;
    gatt_services.ReserveInitialCapacity(services->size());

    for (auto& service : services.value()) {
      gatt_services.push_back(device_->GetOrCreateRemoteGATTService(
          std::move(service), true /* isPrimary */, device_->id()));
    }
    resolver->Resolve(gatt_services);
  } else {
    if (result == mojom::blink::WebBluetoothResult::SERVICE_NOT_FOUND) {
      resolver->Reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::kServiceNotFound, "No Services matching UUID " +
                                                    requested_service_uuid +
                                                    " found in Device."));
    } else {
      resolver->Reject(BluetoothError::CreateDOMException(result));
    }
  }
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(
    ScriptState* script_state,
    const StringOrUnsignedLong& service,
    ExceptionState& exception_state) {
  String service_uuid = BluetoothUUID::getService(service, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return GetPrimaryServicesImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      service_uuid);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* script_state,
    const StringOrUnsignedLong& service,
    ExceptionState& exception_state) {
  String service_uuid = BluetoothUUID::getService(service, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return GetPrimaryServicesImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      service_uuid);
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* script_state,
    ExceptionState&) {
  return GetPrimaryServicesImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTServer::GetPrimaryServicesImpl(
    ScriptState* script_state,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String services_uuid) {
  if (!connected_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, BluetoothError::CreateNotConnectedException(
                          BluetoothOperation::kServicesRetrieval));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServerGetPrimaryServices(
      device_->id(), quantity, services_uuid,
      WTF::Bind(&BluetoothRemoteGATTServer::GetPrimaryServicesCallback,
                WrapPersistent(this), services_uuid, quantity,
                WrapPersistent(resolver)));
  return promise;
}

}  // namespace blink
