// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_

#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/bluetooth_emulation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content::protocol {

// Class that implements the BluetoothEmulation domain, to allow
// configuring virtual Bluetooth devices to test the web-bluetooth API.
// See
// [device/bluetooth/test/README.md](https://chromium.googlesource.com/chromium/src/+/main/device/bluetooth/test/README.md)
class CONTENT_EXPORT BluetoothEmulationHandler
    : public DevToolsDomainHandler,
      public BluetoothEmulation::Backend,
      public bluetooth::mojom::FakeCentralClient {
 public:
  BluetoothEmulationHandler();
  ~BluetoothEmulationHandler() override;

  static std::vector<BluetoothEmulationHandler*> ForAgentHost(
      DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler:
  void Wire(UberDispatcher* dispatcher) override;

 private:
  // BluetoothEmulation::Backend
  Response Enable(const std::string& in_state, bool in_le_supported) override;
  Response Disable() override;
  void SetSimulatedCentralState(
      const std::string& in_state,
      std::unique_ptr<SetSimulatedCentralStateCallback> callback) override;
  void SimulatePreconnectedPeripheral(
      const std::string& in_address,
      const std::string& in_name,
      std::unique_ptr<
          protocol::Array<protocol::BluetoothEmulation::ManufacturerData>>
          in_manufacturer_data,
      std::unique_ptr<protocol::Array<std::string>> in_known_service_uuids,
      std::unique_ptr<SimulatePreconnectedPeripheralCallback> callback)
      override;

  // Simulates an advertisement packet described by |in_entry| being received
  // from a device. If central is currently scanning, the device will appear on
  // the list of discovered devices.
  void SimulateAdvertisement(
      std::unique_ptr<protocol::BluetoothEmulation::ScanEntry> in_entry,
      std::unique_ptr<SimulateAdvertisementCallback> callback) override;

  void SimulateGATTOperationResponse(
      const std::string& in_address,
      const std::string& in_type,
      int in_code,
      std::unique_ptr<SimulateGATTOperationResponseCallback> callback) override;

  void SimulateCharacteristicOperationResponse(
      const std::string& characteristic_id,
      const std::string& in_type,
      int in_code,
      std::optional<Binary> in_data,
      std::unique_ptr<SimulateCharacteristicOperationResponseCallback> callback)
      override;

  void SimulateDescriptorOperationResponse(
      const std::string& descriptor_id,
      const std::string& in_type,
      int in_code,
      std::optional<Binary> in_data,
      std::unique_ptr<SimulateDescriptorOperationResponseCallback> callback)
      override;

  void AddService(const std::string& in_address,
                  const std::string& service_uuid,
                  std::unique_ptr<AddServiceCallback> callback) override;
  void RemoveService(const std::string& service_id,
                     std::unique_ptr<RemoveServiceCallback> callback) override;

  void AddCharacteristic(
      const std::string& service_id,
      const std::string& in_characteristicUuid,
      std::unique_ptr<protocol::BluetoothEmulation::CharacteristicProperties>
          in_properties,
      std::unique_ptr<AddCharacteristicCallback> callback) override;
  void RemoveCharacteristic(
      const std::string& characteristic_id,
      std::unique_ptr<RemoveCharacteristicCallback> callback) override;
  void AddDescriptor(const std::string& characteristic_id,
                     const std::string& descriptor_uuid,
                     std::unique_ptr<AddDescriptorCallback> callback) override;
  void RemoveDescriptor(
      const std::string& in_descriptorId,
      std::unique_ptr<RemoveDescriptorCallback> callback) override;
  void SimulateGATTDisconnection(
      const std::string& address,
      std::unique_ptr<SimulateGATTDisconnectionCallback> callback) override;

  // bluetooth::mojom::FakeCentralClient
  void DispatchGATTOperationEvent(
      bluetooth::mojom::GATTOperationType type,
      const std::string& peripheral_address) override;
  void DispatchCharacteristicOperationEvent(
      bluetooth::mojom::CharacteristicOperationType type,
      const std::optional<std::vector<uint8_t>>& data,
      const std::optional<bluetooth::mojom::WriteType> write_type,
      const std::string& characteristic_id) override;
  void DispatchDescriptorOperationEvent(
      bluetooth::mojom::DescriptorOperationType type,
      const std::optional<std::vector<uint8_t>>& data,
      const std::string& descriptor_id) override;

  bool is_enabled() { return fake_central_.is_bound(); }

  std::optional<BluetoothEmulation::Frontend> frontend_;
  // Tracks emulation usage across all instances, since the backend can only
  // support one fake adapter at a time.
  inline static bool emulation_enabled_ = false;
  mojo::Remote<bluetooth::mojom::FakeCentral> fake_central_;
  mojo::AssociatedReceiver<bluetooth::mojom::FakeCentralClient>
      client_receiver_{this};
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      global_factory_values_;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BLUETOOTH_EMULATION_HANDLER_H_
