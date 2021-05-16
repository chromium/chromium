// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_ADVERTISING_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_ADVERTISING_EVENT_H_

#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class BluetoothDevice;
class BluetoothAdvertisingEventInit;
class BluetoothManufacturerDataMap;
class BluetoothServiceDataMap;
class StringOrUnsignedLong;

class BluetoothAdvertisingEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothAdvertisingEvent(const AtomicString& event_type,
                            const BluetoothAdvertisingEventInit* initializer);

  BluetoothAdvertisingEvent(
      const AtomicString& event_type,
      BluetoothDevice* device,
      mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event);

  ~BluetoothAdvertisingEvent() override;

  void Trace(Visitor*) const override;

  const AtomicString& InterfaceName() const override;

  BluetoothDevice* device() const;
  const String& name() const;
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  const HeapVector<Member<V8UnionUUIDOrUnsignedLong>>& uuids() const;
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  const HeapVector<StringOrUnsignedLong>& uuids() const;
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  absl::optional<uint16_t> appearance() const { return appearance_; }
  absl::optional<int8_t> txPower() const { return txPower_; }
  absl::optional<int8_t> rssi() const { return rssi_; }
  BluetoothManufacturerDataMap* manufacturerData() const;
  BluetoothServiceDataMap* serviceData() const;

 private:
  Member<BluetoothDevice> device_;
  String name_;
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<Member<V8UnionUUIDOrUnsignedLong>> uuids_;
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  HeapVector<StringOrUnsignedLong> uuids_;
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  absl::optional<uint16_t> appearance_;
  absl::optional<int8_t> txPower_;
  absl::optional<int8_t> rssi_;
  const Member<BluetoothManufacturerDataMap> manufacturer_data_map_;
  const Member<BluetoothServiceDataMap> service_data_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_ADVERTISING_EVENT_H_
