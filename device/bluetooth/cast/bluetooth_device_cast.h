// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_DEVICE_CAST_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_DEVICE_CAST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/le_scan_manager.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothDeviceCast : public BluetoothDevice {
 public:
  BluetoothDeviceCast(
      BluetoothAdapter* adapter,
      scoped_refptr<chromecast::bluetooth::RemoteDevice> device);

  BluetoothDeviceCast(const BluetoothDeviceCast&) = delete;
  BluetoothDeviceCast& operator=(const BluetoothDeviceCast&) = delete;

  ~BluetoothDeviceCast() override;

  // BluetoothDevice implementation:
  uint32_t GetBluetoothClass() const override;
  BluetoothTransport GetType() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
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
  std::optional<int8_t> GetInquiryRSSI() const override;
  std::optional<int8_t> GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
  void Pair(PairingDelegate* pairing_delegate,
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

  // Called by BluetoothAdapterCast to update the device to reflect the
  // information obtained from a scan. Returns true if the device changed as a
  // result.
  bool UpdateWithScanResult(const chromecast::bluetooth::LeScanResult& result);

  // Called by BluetoothAdapterCast when the connection state changes, and
  // by OnConnect() and OnDisconnect() when asynchronous requests return. Calls
  // DisConnectGatt() or DidDisconnectGatt() as needed. Returns true if the
  // connection state changed as a result.
  bool SetConnected(bool connected);

  // Called by BluetoothAdapterCast when the value of a characteristic in one of
  // this device's services has changed, resulting in a notification to the
  // device. Locate the characteristc and update the underluing value. If the
  // value is updated, run |callback| synchronously. Return true if that value
  // changed.
  using OnValueUpdatedCallback =
      base::OnceCallback<void(BluetoothRemoteGattCharacteristic*,
                              const std::vector<uint8_t>&)>;
  bool UpdateCharacteristicValue(
      scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic,
      std::vector<uint8_t> value,
      OnValueUpdatedCallback callback);

 private:
  // Implements platform specific operations to initiate a GATT connection.
  // Subclasses must also call DidConnectGatt, DidFailToConnectGatt, or
  // DidDisconnectGatt immediately or asynchronously as the connection state
  // changes.
  void CreateGattConnectionImpl(
      std::optional<BluetoothUUID> service_uuid) override;

  // Disconnects GATT connection on platforms that maintain a specific GATT
  // connection.
  void DisconnectGatt() override;

  // Called back from connect requests generated from CreateGattConnectionImpl.
  void OnConnect(chromecast::bluetooth::RemoteDevice::ConnectStatus status);

  // Called in response to GetServices
  void OnGetServices(
      std::vector<scoped_refptr<chromecast::bluetooth::RemoteService>>
          services);

  bool connected_;
  bool pending_connect_ = false;

  const scoped_refptr<chromecast::bluetooth::RemoteDevice> remote_device_;
  const std::string address_;
  std::optional<std::string> name_;

  base::WeakPtrFactory<BluetoothDeviceCast> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_DEVICE_CAST_H_
