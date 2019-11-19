// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_
#define DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {
namespace test {

// Fake FIDO discovery simulating the behavior of the production
// implementations, and can be used to feed U2fRequests with fake/mock FIDO
// devices.
//
// Most often this class is used together with FakeFidoDiscoveryFactory:
//
//   FakeFidoDiscoveryFactory factory;
//   auto* fake_hid_discovery = factory.ForgeNextHidDiscovery();
//   auto* fake_ble_discovery = factory.ForgeNextBleDiscovery();
//
//   // Pass the factory to the client or replace it globally:
//   content::AuthenticatorEnvironment::GetInstance()
//       ->ReplaceDefaultDiscoveryFactoryForTesting(
//           std::move(factory))
//
//   // Run the production code that will eventually call:
//   // fido_device_discovery_->Create(
//   //     FidoTransportProtocol::kUsbHumanInterfaceDevice)
//   // hid_instance->Start();
//   // fido_device_discovery_->Create(
//   //     FidoTransportProtocol::kBluetoothLowEnergy)
//   // ble_instance->Start();
//
//   // Wait, i.e. spin the message loop until the fake discoveries are started.
//   fake_hid_discovery->WaitForCallToStart();
//   fake_ble_discovery->WaitForCallToStart();
//
//   // Add devices to be discovered immediately.
//   fake_hid_discovery->AddDevice(std::make_unique<MockFidoDevice>(...));
//
//   // Start discoveries (HID succeeds, BLE fails).
//   fake_hid_discovery->SimulateStart(true /* success */);
//   fake_ble_discovery->SimulateStart(false /* success */);
//
//   // Add devices discovered after doing some heavy lifting.
//   fake_hid_discovery->AddDevice(std::make_unique<MockFidoDevice>(...));
//
//   // Destroy the production instance to eventually stop the discovery.
//   // hid_instance.reset();
//
class FakeFidoDiscovery : public FidoDeviceDiscovery,
                          public base::SupportsWeakPtr<FakeFidoDiscovery> {
 public:
  enum class StartMode {
    // SimulateStarted() needs to be called manually to finish starting the
    // discovery after the production code calls Start().
    kManual,
    // The discovery is automatically (successfully) started after Start().
    kAutomatic
  };

  explicit FakeFidoDiscovery(FidoTransportProtocol transport,
                             StartMode mode = StartMode::kManual);
  ~FakeFidoDiscovery() override;

  // Blocks until start is requested.
  void WaitForCallToStart();

  // Simulates the discovery actually starting.
  void SimulateStarted(bool success);

  // Combines WaitForCallToStart + SimulateStarted(true).
  void WaitForCallToStartAndSimulateSuccess();

  // Tests are to directly call Add/RemoveDevice to simulate adding/removing
  // devices. Observers are automatically notified.
  using FidoDeviceDiscovery::AddDevice;
  using FidoDeviceDiscovery::RemoveDevice;

 private:
  // FidoDeviceDiscovery:
  void StartInternal() override;

  const StartMode mode_;
  base::RunLoop wait_for_start_loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoDiscovery);
};

// Overrides FidoDeviceDiscovery::Create* to construct FakeFidoDiscoveries.
class FakeFidoDiscoveryFactory : public device::FidoDiscoveryFactory {
 public:
  using StartMode = FakeFidoDiscovery::StartMode;

  FakeFidoDiscoveryFactory();
  ~FakeFidoDiscoveryFactory() override;

  // Constructs a fake discovery to be returned from the next call to
  // FidoDeviceDiscovery::Create. Returns a raw pointer to the fake so that
  // tests can set it up according to taste.
  //
  // It is an error not to call the relevant method prior to a call to
  // FidoDeviceDiscovery::Create with the respective transport.
  FakeFidoDiscovery* ForgeNextHidDiscovery(StartMode mode = StartMode::kManual);
  FakeFidoDiscovery* ForgeNextNfcDiscovery(StartMode mode = StartMode::kManual);
  FakeFidoDiscovery* ForgeNextBleDiscovery(StartMode mode = StartMode::kManual);
  FakeFidoDiscovery* ForgeNextCableDiscovery(
      StartMode mode = StartMode::kManual);
  FakeFidoDiscovery* ForgeNextPlatformDiscovery(
      StartMode mode = StartMode::kManual);

  // device::FidoDiscoveryFactory:
  std::unique_ptr<FidoDiscoveryBase> Create(
      FidoTransportProtocol transport,
      ::service_manager::Connector* connector) override;

 private:
  std::unique_ptr<FakeFidoDiscovery> next_hid_discovery_;
  std::unique_ptr<FakeFidoDiscovery> next_nfc_discovery_;
  std::unique_ptr<FakeFidoDiscovery> next_ble_discovery_;
  std::unique_ptr<FakeFidoDiscovery> next_cable_discovery_;
  std::unique_ptr<FakeFidoDiscovery> next_platform_discovery_;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoDiscoveryFactory);
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_FAKE_FIDO_DISCOVERY_H_
