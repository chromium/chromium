// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTService::BluetoothRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr service,
    bool is_primary,
    const String& device_instance_id,
    BluetoothDevice* device)
    : service_(std::move(service)),
      is_primary_(is_primary),
      device_instance_id_(device_instance_id),
      device_(device) {}

void BluetoothRemoteGATTService::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

// Callback that allows us to resolve the promise with a single characteristic
// or with a vector owning the characteristics.
void BluetoothRemoteGATTService::GetCharacteristicsCallback(
    const String& service_instance_id,
    const String& requested_characteristic_uuid,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolverBase* resolver,
    mojom::blink::WebBluetoothResult result,
    std::optional<Vector<mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr>>
        characteristics) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!device_->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::kCharacteristicsRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(characteristics);
    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, characteristics->size());
      resolver->DowncastTo<BluetoothRemoteGATTCharacteristic>()->Resolve(
          device_->GetOrCreateRemoteGATTCharacteristic(
              resolver->GetExecutionContext(),
              std::move(characteristics.value()[0]), this));
      return;
    }
    HeapVector<Member<BluetoothRemoteGATTCharacteristic>> gatt_characteristics;
    gatt_characteristics.ReserveInitialCapacity(characteristics->size());
    for (auto& characteristic : characteristics.value()) {
      gatt_characteristics.push_back(
          device_->GetOrCreateRemoteGATTCharacteristic(
              resolver->GetExecutionContext(), std::move(characteristic),
              this));
    }
    resolver->DowncastTo<IDLSequence<BluetoothRemoteGATTCharacteristic>>()
        ->Resolve(gatt_characteristics);
  } else {
    if (result == mojom::blink::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND) {
      resolver->Reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::kCharacteristicNotFound,
          "No Characteristics matching UUID " + requested_characteristic_uuid +
              " found in Service with UUID " + uuid() + "."));
    } else {
      resolver->Reject(BluetoothError::CreateDOMException(result));
    }
  }
}

ScriptPromise<BluetoothRemoteGATTCharacteristic>
BluetoothRemoteGATTService::getCharacteristic(
    ScriptState* script_state,
    const V8BluetoothCharacteristicUUID* characteristic,
    ExceptionState& exception_state) {
  String characteristic_uuid =
      BluetoothUUID::getCharacteristic(characteristic, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<BluetoothRemoteGATTCharacteristic>>(
      script_state, exception_state.GetContext());
  GetCharacteristicsImpl(resolver, exception_state,
                         mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                         characteristic_uuid);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTCharacteristic>>
BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* script_state,
    const V8BluetoothCharacteristicUUID* characteristic,
    ExceptionState& exception_state) {
  String characteristic_uuid =
      BluetoothUUID::getCharacteristic(characteristic, exception_state);
  if (exception_state.HadException())
    return ScriptPromise<IDLSequence<BluetoothRemoteGATTCharacteristic>>();

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTCharacteristic>>>(
      script_state, exception_state.GetContext());
  GetCharacteristicsImpl(resolver, exception_state,
                         mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
                         characteristic_uuid);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTCharacteristic>>
BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTCharacteristic>>>(
      script_state, exception_state.GetContext());
  GetCharacteristicsImpl(resolver, exception_state,
                         mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
  return resolver->Promise();
}

void BluetoothRemoteGATTService::GetCharacteristicsImpl(
    ScriptPromiseResolverBase* resolver,
    ExceptionState& exception_state,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& characteristics_uuid) {
  if (!device_->gatt()->connected() ||
      !device_->GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kCharacteristicsRetrieval));
    return;
  }

  if (!device_->IsValidService(service_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Service with UUID " + service_->uuid +
            " is no longer valid. Remember to retrieve "
            "the service again after reconnecting.");
    return;
  }

  device_->gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServiceGetCharacteristics(
      service_->instance_id, quantity, characteristics_uuid,
      WTF::BindOnce(&BluetoothRemoteGATTService::GetCharacteristicsCallback,
                    WrapPersistent(this), service_->instance_id,
                    characteristics_uuid, quantity, WrapPersistent(resolver)));
}

}  // namespace blink
