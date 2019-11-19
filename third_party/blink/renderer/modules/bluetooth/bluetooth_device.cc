// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_attribute_instance_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_server.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

BluetoothDevice::BluetoothDevice(ExecutionContext* context,
                                 mojom::blink::WebBluetoothDevicePtr device,
                                 Bluetooth* bluetooth)
    : ContextLifecycleObserver(context),
      attribute_instance_map_(
          MakeGarbageCollected<BluetoothAttributeInstanceMap>(this)),
      device_(std::move(device)),
      gatt_(MakeGarbageCollected<BluetoothRemoteGATTServer>(context, this)),
      bluetooth_(bluetooth) {}

BluetoothRemoteGATTService* BluetoothDevice::GetOrCreateRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr service,
    bool is_primary,
    const String& device_instance_id) {
  return attribute_instance_map_->GetOrCreateRemoteGATTService(
      std::move(service), is_primary, device_instance_id);
}

bool BluetoothDevice::IsValidService(const String& service_instance_id) {
  return attribute_instance_map_->ContainsService(service_instance_id);
}

BluetoothRemoteGATTCharacteristic*
BluetoothDevice::GetOrCreateRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service) {
  return attribute_instance_map_->GetOrCreateRemoteGATTCharacteristic(
      context, std::move(characteristic), service);
}

bool BluetoothDevice::IsValidCharacteristic(
    const String& characteristic_instance_id) {
  return attribute_instance_map_->ContainsCharacteristic(
      characteristic_instance_id);
}

BluetoothRemoteGATTDescriptor*
BluetoothDevice::GetOrCreateBluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic) {
  return attribute_instance_map_->GetOrCreateBluetoothRemoteGATTDescriptor(
      std::move(descriptor), characteristic);
}

bool BluetoothDevice::IsValidDescriptor(const String& descriptor_instance_id) {
  return attribute_instance_map_->ContainsDescriptor(descriptor_instance_id);
}

void BluetoothDevice::ClearAttributeInstanceMapAndFireEvent() {
  attribute_instance_map_->Clear();
  DispatchEvent(
      *Event::CreateBubble(event_type_names::kGattserverdisconnected));
}

const WTF::AtomicString& BluetoothDevice::InterfaceName() const {
  return event_target_names::kBluetoothDevice;
}

ExecutionContext* BluetoothDevice::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void BluetoothDevice::Trace(blink::Visitor* visitor) {
  visitor->Trace(attribute_instance_map_);
  visitor->Trace(gatt_);
  visitor->Trace(bluetooth_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void BluetoothDevice::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kGattserverdisconnected) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kGATTServerDisconnectedEvent);
  }
}

}  // namespace blink
