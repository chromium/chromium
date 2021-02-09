// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "device/bluetooth/public/mojom/device.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace bluetooth {

// Implementation of Mojo Adapter located in
// device/bluetooth/public/mojom/adapter.mojom.
// It handles requests for Bluetooth adapter capabilities
// and devices and uses the platform abstraction of device/bluetooth.
class Adapter : public mojom::Adapter,
                public device::BluetoothAdapter::Observer {
 public:
  explicit Adapter(scoped_refptr<device::BluetoothAdapter> adapter);
  ~Adapter() override;

  // mojom::Adapter overrides:
  void ConnectToDevice(const std::string& address,
                       ConnectToDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void GetInfo(GetInfoCallback callback) override;
  void AddObserver(mojo::PendingRemote<mojom::AdapterObserver> observer,
                   AddObserverCallback callback) override;
  void RegisterAdvertisement(const device::BluetoothUUID& service_uuid,
                             const std::vector<uint8_t>& service_data,
                             bool use_scan_response,
                             RegisterAdvertisementCallback callback) override;
  void SetDiscoverable(bool discoverable,
                       SetDiscoverableCallback callback) override;
  void SetName(const std::string& name, SetNameCallback callback) override;
  void StartDiscoverySession(StartDiscoverySessionCallback callback) override;
  void ConnectToServiceInsecurely(
      const std::string& address,
      const device::BluetoothUUID& service_uuid,
      ConnectToServiceInsecurelyCallback callback) override;
  void CreateRfcommServiceInsecurely(
      const std::string& service_name,
      const device::BluetoothUUID& service_uuid,
      CreateRfcommServiceInsecurelyCallback callback) override;

  // device::BluetoothAdapter::Observer overrides:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoverableChanged(device::BluetoothAdapter* adapter,
                                  bool discoverable) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  // Permit untrusted clients to initiate outgoing connections, or listen on
  // incoming connections, with |service_uuid|.
  void AllowConnectionsForUuid(const device::BluetoothUUID& service_uuid);

 private:
  void OnDeviceFetchedForInsecureServiceConnection(
      const device::BluetoothUUID& service_uuid,
      ConnectToServiceInsecurelyCallback callback,
      device::BluetoothDevice* device);
  void ProcessPendingInsecureServiceConnectionRequest(
      const std::string& address,
      device::BluetoothDevice* device);

  void OnGattConnected(
      ConnectToDeviceCallback callback,
      std::unique_ptr<device::BluetoothGattConnection> connection);
  void OnConnectError(ConnectToDeviceCallback callback,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  void OnRegisterAdvertisement(
      RegisterAdvertisementCallback callback,
      scoped_refptr<device::BluetoothAdvertisement> advertisement);
  void OnRegisterAdvertisementError(
      RegisterAdvertisementCallback callback,
      device::BluetoothAdvertisement::ErrorCode error_code);

  void OnSetDiscoverable(SetDiscoverableCallback callback);
  void OnSetDiscoverableError(SetDiscoverableCallback callback);

  void OnSetName(SetNameCallback callback);
  void OnSetNameError(SetNameCallback callback);

  void OnStartDiscoverySession(
      StartDiscoverySessionCallback callback,
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoverySessionError(StartDiscoverySessionCallback callback);

  void OnConnectToService(ConnectToServiceInsecurelyCallback callback,
                          scoped_refptr<device::BluetoothSocket> socket);
  void OnConnectToServiceError(ConnectToServiceInsecurelyCallback callback,
                               const std::string& message);

  void OnCreateRfcommServiceInsecurely(
      CreateRfcommServiceInsecurelyCallback callback,
      scoped_refptr<device::BluetoothSocket> socket);
  void OnCreateRfcommServiceInsecurelyError(
      CreateRfcommServiceInsecurelyCallback callback,
      const std::string& message);

  // The current Bluetooth adapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The adapter observers that listen to this service.
  mojo::RemoteSet<mojom::AdapterObserver> observers_;

  // Arguments provided to ConnectToServiceInsecurely(), cached until the
  // device is ready to be connected to.
  std::vector<std::tuple<std::string,
                         device::BluetoothUUID,
                         ConnectToServiceInsecurelyCallback>>
      pending_connect_to_service_args_;

  // Allowed UUIDs for untrusted clients to initiate outgoing connections, or
  // listen on incoming connections.
  std::set<device::BluetoothUUID> allowed_uuids_;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Adapter);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADAPTER_H_
