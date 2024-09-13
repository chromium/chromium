// Copyright 2015 The Chromium Authors
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
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_characteristic_properties.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BluetoothRemoteGATTCharacteristic::BluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device)
    : ActiveScriptWrappable<BluetoothRemoteGATTCharacteristic>({}),
      ExecutionContextLifecycleObserver(context),
      characteristic_(std::move(characteristic)),
      service_(service),
      device_(device),
      receivers_(this, context) {
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
  SetValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
  if (notification_registration_in_progress()) {
    // Save event and value to be dispatched after notification is registered.
    deferred_value_change_data_.push_back(
        MakeGarbageCollected<DeferredValueChange>(
            Event::Create(event_type_names::kCharacteristicvaluechanged),
            value_, /*promise=*/nullptr));
  } else {
    DispatchEvent(
        *Event::Create(event_type_names::kCharacteristicvaluechanged));
  }
}

const WTF::AtomicString& BluetoothRemoteGATTCharacteristic::InterfaceName()
    const {
  return event_target_names::kBluetoothRemoteGATTCharacteristic;
}

ExecutionContext* BluetoothRemoteGATTCharacteristic::GetExecutionContext()
    const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
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
  EventTarget::AddedEventListener(event_type, registered_listener);
}

void BluetoothRemoteGATTCharacteristic::ReadValueCallback(
    ScriptPromiseResolver<NotShared<DOMDataView>>* resolver,
    mojom::blink::WebBluetoothResult result,
    const std::optional<Vector<uint8_t>>& value) {
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
    if (notification_registration_in_progress()) {
      // Save event to be dispatched after notification is registered.
      deferred_value_change_data_.push_back(
          MakeGarbageCollected<DeferredValueChange>(
              Event::Create(event_type_names::kCharacteristicvaluechanged),
              dom_data_view, resolver));
    } else {
      DispatchEvent(
          *Event::Create(event_type_names::kCharacteristicvaluechanged));
      resolver->Resolve(NotShared(dom_data_view));
    }
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise<NotShared<DOMDataView>>
BluetoothRemoteGATTCharacteristic::readValue(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return ScriptPromise<NotShared<DOMDataView>>();
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        CreateInvalidCharacteristicErrorMessage());
    return ScriptPromise<NotShared<DOMDataView>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<NotShared<DOMDataView>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = GetBluetooth()->Service();
  service->RemoteCharacteristicReadValue(
      characteristic_->instance_id,
      WTF::BindOnce(&BluetoothRemoteGATTCharacteristic::ReadValueCallback,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BluetoothRemoteGATTCharacteristic::WriteValueCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
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

ScriptPromise<IDLUndefined>
BluetoothRemoteGATTCharacteristic::WriteCharacteristicValue(
    ScriptState* script_state,
    const DOMArrayPiece& value,
    mojom::blink::WebBluetoothWriteType write_type,
    ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return EmptyPromise();
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        CreateInvalidCharacteristicErrorMessage());
    return EmptyPromise();
  }

  if (value.IsDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Value buffer has been detached.");
    return EmptyPromise();
  }

  // Partial implementation of writeValue algorithm:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattcharacteristic-writevalue

  // If bytes is more than 512 bytes long (the maximum length of an attribute
  // value, per Long Attribute Values) return a promise rejected with an
  // InvalidModificationError and abort.
  if (value.ByteLength() > 512) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Value can't exceed 512 bytes.");
    return EmptyPromise();
  }

  // Let valueVector be a copy of the bytes held by value.
  Vector<uint8_t> value_vector;
  value_vector.AppendSpan(value.ByteSpan());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = GetBluetooth()->Service();
  service->RemoteCharacteristicWriteValue(
      characteristic_->instance_id, value_vector, write_type,
      WTF::BindOnce(&BluetoothRemoteGATTCharacteristic::WriteValueCallback,
                    WrapPersistent(this), WrapPersistent(resolver),
                    value_vector));

  return promise;
}

ScriptPromise<IDLUndefined> BluetoothRemoteGATTCharacteristic::writeValue(
    ScriptState* script_state,
    const DOMArrayPiece& value,
    ExceptionState& exception_state) {
  return WriteCharacteristicValue(
      script_state, value,
      mojom::blink::WebBluetoothWriteType::kWriteDefaultDeprecated,
      exception_state);
}

ScriptPromise<IDLUndefined>
BluetoothRemoteGATTCharacteristic::writeValueWithResponse(
    ScriptState* script_state,
    const DOMArrayPiece& value,
    ExceptionState& exception_state) {
  return WriteCharacteristicValue(
      script_state, value,
      mojom::blink::WebBluetoothWriteType::kWriteWithResponse, exception_state);
}

ScriptPromise<IDLUndefined>
BluetoothRemoteGATTCharacteristic::writeValueWithoutResponse(
    ScriptState* script_state,
    const DOMArrayPiece& value,
    ExceptionState& exception_state) {
  return WriteCharacteristicValue(
      script_state, value,
      mojom::blink::WebBluetoothWriteType::kWriteWithoutResponse,
      exception_state);
}

void BluetoothRemoteGATTCharacteristic::NotificationsCallback(
    ScriptPromiseResolver<BluetoothRemoteGATTCharacteristic>* resolver,
    bool started,
    mojom::blink::WebBluetoothResult result) {
  if (started) {
    DCHECK_NE(num_in_flight_notification_registrations_, 0U);
    num_in_flight_notification_registrations_--;
  }
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  // If the device is disconnected, reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::kGATT));
    return;
  }

  // Store the agent as the `resolver`'s execution context may
  // start destruction with promise resolution.
  Agent* agent = resolver->GetExecutionContext()->GetAgent();
  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->Resolve(this);
  } else {
    resolver->Reject(BluetoothError::CreateDOMException(result));
  }

  if (started && !notification_registration_in_progress() &&
      !deferred_value_change_data_.empty()) {
    // Ensure promises are resolved before dispatching events allows them
    // to add listeners.
    agent->event_loop()->PerformMicrotaskCheckpoint();
    // Dispatch deferred characteristicvaluechanged events created during the
    // registration of notifications.
    auto deferred_value_change_data = std::move(deferred_value_change_data_);
    deferred_value_change_data_.clear();
    for (const auto& value_changed_data : deferred_value_change_data) {
      auto prior_value = value_;
      value_ = value_changed_data->dom_data_view;
      DispatchEvent(*value_changed_data->event);
      if (value_changed_data->resolver)
        value_changed_data->resolver->Resolve(value_);
      value_ = prior_value;
    }
  }
}

ScriptPromise<BluetoothRemoteGATTCharacteristic>
BluetoothRemoteGATTCharacteristic::startNotifications(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return EmptyPromise();
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        CreateInvalidCharacteristicErrorMessage());
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<BluetoothRemoteGATTCharacteristic>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = GetBluetooth()->Service();
  mojo::PendingAssociatedRemote<mojom::blink::WebBluetoothCharacteristicClient>
      client;
  // See https://bit.ly/2S0zRAS for task types.
  receivers_.Add(
      client.InitWithNewEndpointAndPassReceiver(),
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));

  num_in_flight_notification_registrations_++;
  service->RemoteCharacteristicStartNotifications(
      characteristic_->instance_id, std::move(client),
      WTF::BindOnce(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    WrapPersistent(this), WrapPersistent(resolver),
                    /*starting=*/true));

  return promise;
}

ScriptPromise<BluetoothRemoteGATTCharacteristic>
BluetoothRemoteGATTCharacteristic::stopNotifications(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kGATT));
    return EmptyPromise();
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        CreateInvalidCharacteristicErrorMessage());
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<BluetoothRemoteGATTCharacteristic>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = GetBluetooth()->Service();
  service->RemoteCharacteristicStopNotifications(
      characteristic_->instance_id,
      WTF::BindOnce(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    WrapPersistent(this), WrapPersistent(resolver),
                    /*starting=*/false,
                    mojom::blink::WebBluetoothResult::SUCCESS));
  return promise;
}

ScriptPromise<BluetoothRemoteGATTDescriptor>
BluetoothRemoteGATTCharacteristic::getDescriptor(
    ScriptState* script_state,
    const V8BluetoothDescriptorUUID* descriptor_uuid,
    ExceptionState& exception_state) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptor_uuid, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<BluetoothRemoteGATTDescriptor>>(
      script_state, exception_state.GetContext());
  GetDescriptorsImpl(resolver, exception_state,
                     mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                     descriptor);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTDescriptor>>
BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTDescriptor>>>(
      script_state, exception_state.GetContext());
  GetDescriptorsImpl(resolver, exception_state,
                     mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
  return resolver->Promise();
}

ScriptPromise<IDLSequence<BluetoothRemoteGATTDescriptor>>
BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* script_state,
    const V8BluetoothDescriptorUUID* descriptor_uuid,
    ExceptionState& exception_state) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptor_uuid, exception_state);
  if (exception_state.HadException())
    return ScriptPromise<IDLSequence<BluetoothRemoteGATTDescriptor>>();

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<BluetoothRemoteGATTDescriptor>>>(
      script_state, exception_state.GetContext());
  GetDescriptorsImpl(resolver, exception_state,
                     mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
                     descriptor);
  return resolver->Promise();
}

void BluetoothRemoteGATTCharacteristic::GetDescriptorsImpl(
    ScriptPromiseResolverBase* resolver,
    ExceptionState& exception_state,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& descriptors_uuid) {
  if (!GetGatt()->connected() || !GetBluetooth()->IsServiceBound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        BluetoothError::CreateNotConnectedExceptionMessage(
            BluetoothOperation::kDescriptorsRetrieval));
    return;
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          characteristic_->instance_id)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        CreateInvalidCharacteristicErrorMessage());
    return;
  }

  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = GetBluetooth()->Service();
  service->RemoteCharacteristicGetDescriptors(
      characteristic_->instance_id, quantity, descriptors_uuid,
      WTF::BindOnce(&BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback,
                    WrapPersistent(this), descriptors_uuid,
                    characteristic_->instance_id, quantity,
                    WrapPersistent(resolver)));
}

// Callback that allows us to resolve the promise with a single descriptor
// or with a vector owning the descriptors.
void BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback(
    const String& requested_descriptor_uuid,
    const String& characteristic_instance_id,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolverBase* resolver,
    mojom::blink::WebBluetoothResult result,
    std::optional<Vector<mojom::blink::WebBluetoothRemoteGATTDescriptorPtr>>
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
      resolver->DowncastTo<BluetoothRemoteGATTDescriptor>()->Resolve(
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
    resolver->DowncastTo<IDLSequence<BluetoothRemoteGATTDescriptor>>()->Resolve(
        gatt_descriptors);
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

String
BluetoothRemoteGATTCharacteristic::CreateInvalidCharacteristicErrorMessage() {
  return "Characteristic with UUID " + uuid() +
         " is no longer valid. Remember to retrieve the characteristic again "
         "after reconnecting.";
}

void BluetoothRemoteGATTCharacteristic::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(properties_);
  visitor->Trace(value_);
  visitor->Trace(device_);
  visitor->Trace(receivers_);
  visitor->Trace(deferred_value_change_data_);

  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void BluetoothRemoteGATTCharacteristic::DeferredValueChange::Trace(
    Visitor* visitor) const {
  visitor->Trace(event);
  visitor->Trace(dom_data_view);
  if (resolver)
    visitor->Trace(resolver);
}

}  // namespace blink
