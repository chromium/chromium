// Copyright 2015 The Chromium Authors
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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(ExecutionContext* context,
                                                     BluetoothDevice* device)
    :  // See https://bit.ly/2S0zRAS for task types.
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      client_receivers_(this, context),
      device_(device),
      connected_(false) {}

void BluetoothRemoteGATTServer::GATTServerDisconnected() {
  DispatchDisconnected();
}

void BluetoothRemoteGATTServer::AddToActiveAlgorithms(
    ScriptPromiseResolverBase* resolver) {
  auto result = active_algorithms_.insert(resolver);
  CHECK(result.is_new_entry);
}

bool BluetoothRemoteGATTServer::RemoveFromActiveAlgorithms(
    ScriptPromiseResolverBase* resolver) {
  if (!active_algorithms_.Contains(resolver)) {
    return false;
  }
  active_algorithms_.erase(resolver);
  return true;
}

void BluetoothRemoteGATTServer::CleanupDisconnectedDeviceAndFireEvent() {
  DCHECK(connected_);
  connected_ = false;
  active_algorithms_.clear();
  device_->ClearAttributeInstanceMapAndFireEvent();
}

void BluetoothRemoteGATTServer::DispatchDisconnected() {
  if (!connected_) {
    return;
  }
  CleanupDisconnectedDeviceAndFireEvent();
}

void BluetoothRemoteGATTServer::Trace(Visitor* visitor) const {
  visitor->Trace(client_receivers_);
  visitor->Trace(active_algorithms_);
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

void BluetoothRemoteGATTServer::ConnectCallback(
    ScriptPromiseResolver<BluetoothRemoteGATTServer>* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    connected_ = true;
    resolver->Resolve(this);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise<BluetoothRemoteGATTServer> BluetoothRemoteGATTServer::connect(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<BluetoothRemoteGATTServer>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (!device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kServicesRetrieval));
    return EmptyPromise();
  }

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothServerClient> client;
  client_receivers_.Add(client.InitWithNewEndpointAndPassReceiver(),
                        task_runner_);

  service->RemoteServerConnect(
      device_->GetDevice()->id, std::move(client),
      WTF::BindOnce(&BluetoothRemoteGATTServer::ConnectCallback,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (!connected_)
    return;

  if (!device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kServicesRetrieval));
    return;
  }

  CleanupDisconnectedDeviceAndFireEvent();
  client_receivers_.Clear();
  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServerDisconnect(device_->GetDevice()->id);
}

// Callback that allows us to resolve the promise with a single service or
// with a vector owning the services.
void BluetoothRemoteGATTServer::GetPrimaryServicesCallback(
    const String& requested_service_uuid,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolverBase* resolver,
    mojom::blink::WebBluetoothResult result,
    std::optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>>
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
      resolver->DowncastTo<BluetoothRemoteGATTService>()->Resolve(
          device_->GetOrCreateRemoteGATTService(std::move(services.value()[0]),
                                                true /* isPrimary */,
                                                device_->id()));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTService>> gatt_services;
    gatt_services.ReserveInitialCapacity(services->size());

    for (auto& service : services.value()) {
      gatt_services.push_back(device_->GetOrCreateRemoteGATTService(
          std::move(service), true /* isPrimary */, device_->id()));
    }
    resolver->DowncastTo<IDLSequence<BluetoothRemoteGATTService>>()->Resolve(
        gatt_services);
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

ScriptPromise<BluetoothRemoteGATTService>
BluetoothRemoteGATTServer::getPrimaryService(
    ScriptState* script_state,
    const V8BluetoothServiceUUID* service,
    ExceptionState& exception_state) {
  String service_uuid = BluetoothUUID::getService(service, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  if (!connected_ || !device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kServicesRetrieval));
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<BluetoothRemoteGATTService>>(
          script_state, exception_state.GetContext());
  GetPrimaryServicesImpl(resolver,
                         mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                         service_uuid);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTService>>
BluetoothRemoteGATTServer::getPrimaryServices(
    ScriptState* script_state,
    const V8BluetoothServiceUUID* service,
    ExceptionState& exception_state) {
  String service_uuid = BluetoothUUID::getService(service, exception_state);
  if (exception_state.HadException())
    return ScriptPromise<IDLSequence<BluetoothRemoteGATTService>>();

  if (!connected_ || !device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kServicesRetrieval));
    return ScriptPromise<IDLSequence<BluetoothRemoteGATTService>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTService>>>(
      script_state, exception_state.GetContext());
  GetPrimaryServicesImpl(resolver,
                         mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
                         service_uuid);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTService>>
BluetoothRemoteGATTServer::getPrimaryServices(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  if (!connected_ || !device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kServicesRetrieval));
    return ScriptPromise<IDLSequence<BluetoothRemoteGATTService>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTService>>>(
      script_state, exception_state.GetContext());
  GetPrimaryServicesImpl(resolver,
                         mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
  return resolver->Promise();
}

void BluetoothRemoteGATTServer::GetPrimaryServicesImpl(
    ScriptPromiseResolverBase* resolver,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    String services_uuid) {

  AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServerGetPrimaryServices(
      device_->GetDevice()->id, quantity, services_uuid,
      WTF::BindOnce(&BluetoothRemoteGATTServer::GetPrimaryServicesCallback,
                    WrapPersistent(this), services_uuid, quantity,
                    WrapPersistent(resolver)));
}

}  // namespace blink
