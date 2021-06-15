// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_advertising_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_manufacturer_data_map.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_service_data_map.h"

namespace blink {

BluetoothAdvertisingEvent::BluetoothAdvertisingEvent(
    const AtomicString& event_type,
    const BluetoothAdvertisingEventInit* initializer)
    : Event(event_type, initializer),
      device_(initializer->device()),
      name_(initializer->name()),
      appearance_(initializer->hasAppearance() ? initializer->appearance() : 0),
      txPower_(initializer->hasTxPower() ? initializer->txPower() : 0),
      rssi_(initializer->hasRssi() ? initializer->rssi() : 0),
      manufacturer_data_map_(initializer->manufacturerData()),
      service_data_map_(initializer->serviceData()) {
  if (initializer->hasUuids()) {
    uuids_ = initializer->uuids();
  }
}

BluetoothAdvertisingEvent::BluetoothAdvertisingEvent(
    const AtomicString& event_type,
    BluetoothDevice* device,
    mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event)
    : Event(event_type, Bubbles::kYes, Cancelable::kYes),
      device_(std::move(device)),
      name_(advertising_event->name),
      appearance_(advertising_event->appearance),
      txPower_(advertising_event->tx_power),
      rssi_(advertising_event->rssi),
      manufacturer_data_map_(MakeGarbageCollected<BluetoothManufacturerDataMap>(
          advertising_event->manufacturer_data)),
      service_data_map_(MakeGarbageCollected<BluetoothServiceDataMap>(
          advertising_event->service_data)) {
  for (const String& uuid : advertising_event->uuids) {
    uuids_.push_back(MakeGarbageCollected<V8UnionUUIDOrUnsignedLong>(uuid));
  }
}  // namespace blink

BluetoothAdvertisingEvent::~BluetoothAdvertisingEvent() {}

void BluetoothAdvertisingEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(uuids_);
  visitor->Trace(manufacturer_data_map_);
  visitor->Trace(service_data_map_);
  Event::Trace(visitor);
}

const AtomicString& BluetoothAdvertisingEvent::InterfaceName() const {
  return event_type_names::kAdvertisementreceived;
}

BluetoothDevice* BluetoothAdvertisingEvent::device() const {
  return device_;
}

const String& BluetoothAdvertisingEvent::name() const {
  return name_;
}

const HeapVector<Member<V8UnionUUIDOrUnsignedLong>>&
BluetoothAdvertisingEvent::uuids() const {
  return uuids_;
}

BluetoothManufacturerDataMap* BluetoothAdvertisingEvent::manufacturerData()
    const {
  return manufacturer_data_map_;
}

BluetoothServiceDataMap* BluetoothAdvertisingEvent::serviceData() const {
  return service_data_map_;
}

}  // namespace blink
