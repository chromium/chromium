// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"

#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

BluetoothAdvertisingEvent::BluetoothAdvertisingEvent(
    const AtomicString& event_type,
    BluetoothDevice* device,
    mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event,
    const DOMWrapperWorld* world)
    : Event(event_type, Bubbles::kYes, Cancelable::kYes),
      device_(std::move(device)),
      name_(advertising_event->name),
      appearance_(advertising_event->appearance),
      txPower_(advertising_event->tx_power),
      rssi_(advertising_event->rssi),
      manufacturer_data_map_(MakeGarbageCollected<BluetoothManufacturerDataMap>(
          advertising_event->manufacturer_data)),
      service_data_map_(MakeGarbageCollected<BluetoothServiceDataMap>(
          advertising_event->service_data)),
      world_(world) {
  for (const String& uuid : advertising_event->uuids) {
    uuids_.push_back(uuid);
  }
}

BluetoothAdvertisingEvent::~BluetoothAdvertisingEvent() {}

bool BluetoothAdvertisingEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  return !world_ || &world == world_.Get();
}

void BluetoothAdvertisingEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(manufacturer_data_map_);
  visitor->Trace(service_data_map_);
  visitor->Trace(world_);
  Event::Trace(visitor);
}

const AtomicString& BluetoothAdvertisingEvent::InterfaceName() const {
  return event_type_names::kAdvertisementreceived;
}

BluetoothDevice* BluetoothAdvertisingEvent::device() const {
  return device_.Get();
}

const String& BluetoothAdvertisingEvent::name() const {
  return name_;
}

const Vector<String>& BluetoothAdvertisingEvent::uuids() const {
  return uuids_;
}

BluetoothManufacturerDataMap* BluetoothAdvertisingEvent::manufacturerData()
    const {
  return manufacturer_data_map_.Get();
}

BluetoothServiceDataMap* BluetoothAdvertisingEvent::serviceData() const {
  return service_data_map_.Get();
}

}  // namespace blink
