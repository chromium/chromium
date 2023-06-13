// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_DEVICE_MAC_H_

#import <IOBluetooth/IOBluetooth.h>
#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class IOBluetoothDevice;

namespace device {

class BluetoothAdapterMac;
class BluetoothUUID;

class BluetoothClassicDeviceMac : public BluetoothDeviceMac {
 public:
  explicit BluetoothClassicDeviceMac(BluetoothAdapterMac* adapter,
                                     IOBluetoothDevice* device);

  BluetoothClassicDeviceMac(const BluetoothClassicDeviceMac&) = delete;
  BluetoothClassicDeviceMac& operator=(const BluetoothClassicDeviceMac&) =
      delete;

  ~BluetoothClassicDeviceMac() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  absl::optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  absl::optional<int8_t> GetInquiryRSSI() const override;
  absl::optional<int8_t> GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;

  base::Time GetLastUpdateTime() const override;

  // Returns the Bluetooth address for the |device|. The returned address has a
  // normalized format (see below).
  static std::string GetDeviceAddress(IOBluetoothDevice* device);
  bool IsLowEnergyDevice() override;

 protected:
  // BluetoothDevice override
  void CreateGattConnectionImpl(
      absl::optional<BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

 private:
  friend class BluetoothAdapterMac;

  // Implementation to read the host's transmit power level of type
  // |power_level_type|.
  int GetHostTransmitPower(
      BluetoothHCITransmitPowerLevelType power_level_type) const;

  IOBluetoothDevice* __strong device_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_DEVICE_MAC_H_
