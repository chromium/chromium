// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_

#include "content/browser/devtools/protocol/bluetooth_emulation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"

namespace content::protocol {

// Class that implements the BluetoothEmulation domain, to allow
// configuring virtual Bluetooth devices to test the web-bluetooth API.
// See
// [device/bluetooth/test/README.md](https://chromium.googlesource.com/chromium/src/+/main/device/bluetooth/test/README.md)
class CONTENT_EXPORT BluetoothEmulationHandler
    : public DevToolsDomainHandler,
      public BluetoothEmulation::Backend {
 public:
  BluetoothEmulationHandler();
  ~BluetoothEmulationHandler() override;

  // DevToolsDomainHandler:
  void Wire(UberDispatcher* dispatcher) override;

 private:
  // BluetoothEmulation::Backend
  Response Enable(const String& in_state) override;
  Response Disable() override;
  void SimulatePreconnectedPeripheral(
      const String& in_address,
      const String& in_name,
      std::unique_ptr<
          protocol::Array<protocol::BluetoothEmulation::ManufacturerData>>
          in_manufacturer_data,
      std::unique_ptr<protocol::Array<String>> in_known_service_uuids,
      std::unique_ptr<SimulatePreconnectedPeripheralCallback> callback)
      override;

  // Simulates an advertisement packet described by |in_entry| being received
  // from a device. If central is currently scanning, the device will appear on
  // the list of discovered devices.
  void SimulateAdvertisement(
      std::unique_ptr<protocol::BluetoothEmulation::ScanEntry> in_entry,
      std::unique_ptr<SimulateAdvertisementCallback> callback) override;

  // Tracks emulation usage across all instances, since the backend can only
  // support one fake adapter at a time.
  inline static bool emulation_enabled_ = false;
  mojo::Remote<bluetooth::mojom::FakeCentral> fake_central_;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_
