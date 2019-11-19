// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include <utility>

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_characteristic_properties.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTCharacteristic::BluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device)
    : ContextLifecycleObserver(context),
      characteristic_(std::move(characteristic)),
      service_(service),
      device_(device) {
  properties_ = MakeGarbageCollected<BluetoothCharacteristicProperties>(
      characteristic_->properties);
}

void BluetoothRemoteGATTCharacteristic::SetValue(DOMDataView* dom_data_view) {
  value_ = dom_data_view;
}

void BluetoothRemoteGATTCharacteristic::RemoteCharacteristicValueChanged(
    const Vector<uint8_t>& value) {
  if (!GetGatt()->connected())
    return;
  this->SetValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
  DispatchEvent(*Event::Create(event_type_names::kCharacteristicvaluechanged));
}

void BluetoothRemoteGATTCharacteristic::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

void BluetoothRemoteGATTCharacteristic::Dispose() {
  receivers_.Clear();
}

const WTF::AtomicString& BluetoothRemoteGATTCharacteristic::InterfaceName()
    const {
  return event_target_names::kBluetoothRemoteGATTCharacteristic;
}

ExecutionContext* BluetoothRemoteGATTCharacteristic::GetExecutionContext()
    const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool BluetoothRemoteGATTCharacteristic::HasPendingActivity() const {
  // This object should be considered active as long as there are registered
  // event listeners. Even if script drops all references this can still be
  // found again through the BluetoothRemoteGATTServer object.
  return GetExecutionContext() && HasEventListeners();
}

void BluetoothRemoteGATTCharacteristic::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
}

void BluetoothRemoteGATTCharacteristic::ReadValueCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    const base::Optional<Vector<uint8_t>>& value) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(value);
    DOMDataView* dom_data_view =
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value.value());
    SetValue(dom_data_view);
    DispatchEvent(
        *Event::Create(event_type_names::kCharacteristicvaluechanged));
    resolver->Resolve(dom_data_view);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::readValue(
    ScriptState* script_state) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, CreateInvalidCharacteristicError());
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteCharacteristicReadValue(
      characteristic_->instance_id,
      WTF::Bind(&BluetoothRemoteGATTCharacteristic::ReadValueCallback,
                WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BluetoothRemoteGATTCharacteristic::WriteValueCallback(
    ScriptPromiseResolver* resolver,
    const Vector<uint8_t>& value,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    SetValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
    resolver->Resolve();
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::writeValue(
    ScriptState* script_state,
    const DOMArrayPiece& value) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, CreateInvalidCharacteristicError());
  }

  if (value.IsDetached()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Value buffer has been detached."));
  }

  // Partial implementation of writeValue algorithm:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattcharacteristic-writevalue

  // If bytes is more than 512 bytes long (the maximum length of an attribute
  // value, per Long Attribute Values) return a promise rejected with an
  // InvalidModificationError and abort.
  if (value.ByteLength() > 512) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidModificationError,
                          "Value can't exceed 512 bytes."));
  }

  // Let valueVector be a copy of the bytes held by value.
  Vector<uint8_t> value_vector;
  value_vector.Append(value.Bytes(), value.ByteLength());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteCharacteristicWriteValue(
      characteristic_->instance_id, value_vector,
      WTF::Bind(&BluetoothRemoteGATTCharacteristic::WriteValueCallback,
                WrapPersistent(this), WrapPersistent(resolver), value_vector));

  return promise;
}

void BluetoothRemoteGATTCharacteristic::NotificationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->Resolve(this);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::startNotifications(
    ScriptState* script_state) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, CreateInvalidCharacteristicError());
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothCharacteristicClient>
      client;
  // See https://bit.ly/2S0zRAS for task types.
  receivers_.Add(
      this, client.InitWithNewEndpointAndPassReceiver(),
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));

  service->RemoteCharacteristicStartNotifications(
      characteristic_->instance_id, std::move(client),
      WTF::Bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::stopNotifications(
    ScriptState* script_state) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, CreateInvalidCharacteristicError());
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteCharacteristicStopNotifications(
      characteristic_->instance_id,
      WTF::Bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                WrapPersistent(this), WrapPersistent(resolver),
                mojom::blink::WebBluetoothResult::SUCCESS));
  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptor(
    ScriptState* script_state,
    const StringOrUnsignedLong& descriptor_uuid,
    ExceptionState& exception_state) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptor_uuid, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return GetDescriptorsImpl(script_state,
                            mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                            descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* script_state,
    ExceptionState&) {
  return GetDescriptorsImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* script_state,
    const StringOrUnsignedLong& descriptor_uuid,
    ExceptionState& exception_state) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptor_uuid, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return GetDescriptorsImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::GetDescriptorsImpl(
    ScriptState* script_state,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& descriptors_uuid) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, BluetoothError::CreateNotConnectedException(
                          BluetoothOperation::kDescriptorsRetrieval));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, CreateInvalidCharacteristicError());
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteCharacteristicGetDescriptors(
      characteristic_->instance_id, quantity, descriptors_uuid,
      WTF::Bind(&BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback,
                WrapPersistent(this), descriptors_uuid,
                characteristic_->instance_id, quantity,
                WrapPersistent(resolver)));

  return promise;
}

// Callback that allows us to resolve the promise with a single descriptor
// or with a vector owning the descriptors.
void BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback(
    const String& requested_descriptor_uuid,
    const String& characteristic_instance_id,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    base::Optional<Vector<mojom::blink::WebBluetoothRemoteGATTDescriptorPtr>>
        descriptors) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!service_->device()->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::kDescriptorsRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(descriptors);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, descriptors->size());
      resolver->Resolve(
          service_->device()->GetOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptors.value()[0]), this));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTDescriptor>> gatt_descriptors;
    gatt_descriptors.ReserveInitialCapacity(descriptors->size());
    for (auto& descriptor : descriptors.value()) {
      gatt_descriptors.push_back(
          service_->device()->GetOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptor), this));
    }
    resolver->Resolve(gatt_descriptors);
  } else {
    if (result == mojom::blink::WebBluetoothResult::DESCRIPTOR_NOT_FOUND) {
      resolver->Reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::kDescriptorNotFound,
          "No Descriptors matching UUID " + requested_descriptor_uuid +
              " found in Characteristic with UUID " + uuid() + "."));
    } else {
      resolver->Reject(BluetoothError::CreateDOMException(result));
    }
  }
}

DOMException*
BluetoothRemoteGATTCharacteristic::CreateInvalidCharacteristicError() {
  return BluetoothError::CreateDOMException(
      BluetoothErrorCode::kInvalidCharacteristic,
      "Characteristic with UUID " + uuid() +
          " is no longer valid. Remember to retrieve the characteristic again "
          "after reconnecting.");
}

void BluetoothRemoteGATTCharacteristic::Trace(blink::Visitor* visitor) {
  visitor->Trace(service_);
  visitor->Trace(properties_);
  visitor->Trace(value_);
  visitor->Trace(device_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
