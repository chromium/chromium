// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_

#include <string>
#include <unordered_set>

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossAdapterClient
    : public FlossAdapterClient {
 public:
  FakeFlossAdapterClient();
  ~FakeFlossAdapterClient() override;

  // Addresses used by unit tests.
  static const char kBondedAddress1[];
  static const char kBondedAddress2[];
  static const char kPairedAddressBrEdr[];
  static const char kPairedAddressLE[];
  static const uint32_t kDefaultClassOfDevice;

  // Addresses used by unit tests and emulator.
  static const char kClassicName[];
  static const char kClassicAddress[];
  static const uint32_t kClassicClassOfDevice;
  static const char kPinCodeDisplayName[];
  static const char kPinCodeDisplayAddress[];
  static const uint32_t kPinCodeDisplayClassOfDevice;
  static const char kPasskeyDisplayName[];
  static const char kPasskeyDisplayAddress[];
  static const uint32_t kPasskeyDisplayClassOfDevice;
  static const char kPinCodeRequestName[];
  static const char kPinCodeRequestAddress[];
  static const uint32_t kPinCodeRequestClassOfDevice;
  static const char kPhoneName[];
  static const char kPhoneAddress[];
  static const uint32_t kPhoneClassOfDevice;
  static const char kPasskeyRequestName[];
  static const char kPasskeyRequestAddress[];
  static const uint32_t kPasskeyRequestClassOfDevice;
  static const char kJustWorksName[];
  static const char kJustWorksAddress[];
  static const uint32_t kJustWorksClassOfDevice;

  static const uint32_t kPasskey;
  static const char kPinCode[];

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;
  void SetName(ResponseCallback<Void> callback,
               const std::string& name) override;
  void StartDiscovery(ResponseCallback<Void> callback) override;
  void CancelDiscovery(ResponseCallback<Void> callback) override;
  void CreateBond(ResponseCallback<bool> callback,
                  FlossDeviceId device,
                  BluetoothTransport transport) override;
  void CreateBond(ResponseCallback<FlossDBusClient::BtifStatus> callback,
                  FlossDeviceId device,
                  BluetoothTransport transport) override;
  void CancelBondProcess(ResponseCallback<bool> callback,
                         FlossDeviceId device) override;
  void RemoveBond(ResponseCallback<bool> callback,
                  FlossDeviceId device) override;
  void GetRemoteType(ResponseCallback<BluetoothDeviceType> callback,
                     FlossDeviceId device) override;
  void GetRemoteClass(ResponseCallback<uint32_t> callback,
                      FlossDeviceId device) override;
  void GetRemoteAppearance(ResponseCallback<uint16_t> callback,
                           FlossDeviceId device) override;
  void GetConnectionState(ResponseCallback<uint32_t> callback,
                          const FlossDeviceId& device) override;
  void GetRemoteUuids(
      ResponseCallback<device::BluetoothDevice::UUIDList> callback,
      FlossDeviceId device) override;
  void GetRemoteVendorProductInfo(
      ResponseCallback<FlossAdapterClient::VendorProductInfo> callback,
      FlossDeviceId device) override;
  void GetRemoteAddressType(
      ResponseCallback<FlossAdapterClient::BtAddressType> callback,
      FlossDeviceId device) override;
  void GetBondState(ResponseCallback<uint32_t> callback,
                    const FlossDeviceId& device) override;
  void ConnectAllEnabledProfiles(ResponseCallback<Void> callback,
                                 const FlossDeviceId& device) override;
  void ConnectAllEnabledProfiles(
      ResponseCallback<FlossDBusClient::BtifStatus> callback,
      const FlossDeviceId& device) override;
  void DisconnectAllEnabledProfiles(ResponseCallback<Void> callback,
                                    const FlossDeviceId& device) override;
  void SetPairingConfirmation(ResponseCallback<Void> callback,
                              const FlossDeviceId& device,
                              bool accept) override;
  void SetPin(ResponseCallback<Void> callback,
              const FlossDeviceId& device,
              bool accept,
              const std::vector<uint8_t>& pin) override;
  void GetBondedDevices() override;
  void GetConnectedDevices() override;

  // Helper for posting a delayed task.
  void PostDelayedTask(base::OnceClosure callback);

  // Helper for setting the connection state.
  void SetConnected(const std::string& address, bool connected);

  // Test utility to do fake notification to observers.
  void NotifyObservers(
      const base::RepeatingCallback<void(Observer*)>& notify) const;

  // Fake discovery failure on next call.
  void FailNextDiscovery();

  // Fake bonding failure on next CreateBond call.
  void FailNextBonding();

 private:
  std::unordered_set<std::string> bonded_addresses_;
  std::unordered_set<std::string> connected_addresses_;
  std::optional<bool> fail_discovery_;
  std::optional<bool> fail_bonding_;
  base::WeakPtrFactory<FakeFlossAdapterClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADAPTER_CLIENT_H_
