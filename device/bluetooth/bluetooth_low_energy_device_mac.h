// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_

#import <CoreBluetooth/CoreBluetooth.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string_view>

#include "build/build_config.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_device_mac.h"

#if !BUILDFLAG(IS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif

@class BluetoothLowEnergyPeripheralDelegate;

namespace device {

class BluetoothAdapter;
class BluetoothLowEnergyAdapterApple;
class BluetoothRemoteGattServiceMac;
class BluetoothRemoteGattCharacteristicMac;
class BluetoothRemoteGattDescriptorMac;
class BluetoothUUID;

class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyDeviceMac
    : public BluetoothDeviceMac {
 public:
  BluetoothLowEnergyDeviceMac(BluetoothAdapter* adapter,
                              CBPeripheral* peripheral);

  BluetoothLowEnergyDeviceMac(const BluetoothLowEnergyDeviceMac&) = delete;
  BluetoothLowEnergyDeviceMac& operator=(const BluetoothLowEnergyDeviceMac&) =
      delete;

  ~BluetoothLowEnergyDeviceMac() override;

  // BluetoothDevice overrides.
  std::string GetIdentifier() const override;
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  BluetoothDevice::VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  std::optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
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
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;
  bool IsLowEnergyDevice() override;

 protected:
  // BluetoothDevice override.
  void CreateGattConnectionImpl(
      std::optional<BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

  // Methods used by BluetoothLowEnergyPeripheralBridge.
  void DidDiscoverPrimaryServices(NSError* error);
  void DidModifyServices(NSArray* invalidatedServices);
  void DidDiscoverCharacteristics(CBService* cb_service, NSError* error);
  void DidUpdateValue(CBCharacteristic* characteristic, NSError* error);
  void DidWriteValue(CBCharacteristic* characteristic, NSError* error);
  void DidUpdateNotificationState(CBCharacteristic* characteristic,
                                  NSError* error);
  void DidDiscoverDescriptors(CBCharacteristic* characteristic, NSError* error);
  void DidUpdateValueForDescriptor(CBDescriptor* cb_descriptor, NSError* error);
  void DidWriteValueForDescriptor(CBDescriptor* descriptor, NSError* error);

  static std::string GetPeripheralIdentifier(CBPeripheral* peripheral);

  // Hashes and truncates the peripheral identifier to deterministically
  // construct an address. The use of fake addresses is a temporary fix before
  // we switch to using bluetooth identifiers throughout Chrome.
  // http://crbug.com/507824
  static std::string GetPeripheralHashAddress(CBPeripheral* peripheral);
  static std::string GetPeripheralHashAddress(
      std::string_view device_identifier);

 private:
  friend class BluetoothLowEnergyAdapterApple;
  friend class BluetoothLowEnergyAdapterAppleTest;
  friend class BluetoothLowEnergyPeripheralBridge;
  friend class BluetoothRemoteGattServiceMac;
  friend class BluetoothTestMac;
  friend class BluetoothRemoteGattServiceMac;

  // Called by the adapter when the device is connected.
  void DidConnectPeripheral();

  // Calls macOS to discover primary services.
  void DiscoverPrimaryServices();

  // Sends notification if this device is ready with all services discovered.
  void SendNotificationIfDiscoveryComplete();

  // Returns the Bluetooth adapter.
  BluetoothLowEnergyAdapterApple* GetLowEnergyAdapter();
  BluetoothLowEnergyAdapterApple* GetLowEnergyAdapter() const;

  // Returns the CoreBluetooth Peripheral.
  CBPeripheral* GetPeripheral();

  // Returns BluetoothRemoteGattServiceMac based on the CBService.
  BluetoothRemoteGattServiceMac* GetBluetoothRemoteGattServiceMac(
      CBService* service) const;

  // Returns BluetoothRemoteGattCharacteristicMac based on the CBCharacteristic.
  BluetoothRemoteGattCharacteristicMac* GetBluetoothRemoteGattCharacteristicMac(
      CBCharacteristic* cb_characteristic) const;

  // Returns BluetoothRemoteGattDescriptorMac based on the CBDescriptor.
  BluetoothRemoteGattDescriptorMac* GetBluetoothRemoteGattDescriptorMac(
      CBDescriptor* cb_descriptor) const;

  // Callback used when the CoreBluetooth Peripheral is disconnected.
  void DidDisconnectPeripheral(NSError* error);

  // CoreBluetooth data structure.
  CBPeripheral* __strong peripheral_;

  // Objective-C delegate for the CBPeripheral.
  BluetoothLowEnergyPeripheralDelegate* __strong peripheral_delegate_;

  // Whether the device is connected.
  bool connected_;

  // The peripheral's identifier, as returned by [CBPeripheral identifier].
  std::string identifier_;

  // A local address for the device created by hashing the peripheral
  // identifier.
  std::string hash_address_;

  // Increases each time -[CBPeripheral discoverServices:] is called, and
  // decreases each time DidDiscoverPrimaryServices() is called. Once the
  // value is set to 0, characteristics and properties are discovered.
  int discovery_pending_count_;
};

// Stream operator for logging.
DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothLowEnergyDeviceMac& device);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_
