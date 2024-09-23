// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/gatt_service.h"
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

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;

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
                             bool connectable,
                             RegisterAdvertisementCallback callback) override;
  void SetDiscoverable(bool discoverable,
                       SetDiscoverableCallback callback) override;
  void SetName(const std::string& name, SetNameCallback callback) override;
  void StartDiscoverySession(const std::string& client_name,
                             StartDiscoverySessionCallback callback) override;
  void ConnectToServiceInsecurely(
      const std::string& address,
      const device::BluetoothUUID& service_uuid,
      bool should_unbond_on_error,
      ConnectToServiceInsecurelyCallback callback) override;
  void CreateRfcommServiceInsecurely(
      const std::string& service_name,
      const device::BluetoothUUID& service_uuid,
      CreateRfcommServiceInsecurelyCallback callback) override;
  void CreateLocalGattService(
      const device::BluetoothUUID& service_id,
      mojo::PendingRemote<mojom::GattServiceObserver> observer,
      CreateLocalGattServiceCallback callback) override;
  void IsLeScatternetDualRoleSupported(
      IsLeScatternetDualRoleSupportedCallback callback) override;

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
  struct ConnectToServiceRequestDetails {
    ConnectToServiceRequestDetails(const std::string& address,
                                   const device::BluetoothUUID& service_uuid,
                                   const base::Time& time_requested,
                                   bool should_unbond_on_error,
                                   ConnectToServiceInsecurelyCallback callback);
    ~ConnectToServiceRequestDetails();

    std::string address;
    device::BluetoothUUID service_uuid;
    base::Time time_requested;
    bool should_unbond_on_error;
    ConnectToServiceInsecurelyCallback callback;
  };

  void OnGattServiceInvalidated(device::BluetoothUUID service_id);

  void OnDeviceFetchedForInsecureServiceConnection(
      int request_id,
      device::BluetoothDevice* device);
  void ProcessDeviceForInsecureServiceConnection(
      int request_id,
      device::BluetoothDevice* device,
      bool disconnected);
  void ProcessPendingInsecureServiceConnectionRequest(
      device::BluetoothDevice* device,
      bool disconnected);

  void OnGattConnect(
      ConnectToDeviceCallback callback,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

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

  void OnConnectToService(int request_id,
                          scoped_refptr<device::BluetoothSocket> socket);
  void OnConnectToServiceError(int request_id, const std::string& message);
  void OnConnectToServiceInsecurelyError(int request_id,
                                         const std::string& error_message);

  void OnCreateRfcommServiceInsecurely(
      CreateRfcommServiceInsecurelyCallback callback,
      scoped_refptr<device::BluetoothSocket> socket);
  void OnCreateRfcommServiceInsecurelyError(
      CreateRfcommServiceInsecurelyCallback callback,
      const std::string& message);

  void ExecuteConnectToServiceCallback(int request_id,
                                       mojom::ConnectToServiceResultPtr result);

  // The current Bluetooth adapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The adapter observers that listen to this service.
  mojo::RemoteSet<mojom::AdapterObserver> observers_;

  // Keeps track of details about pending ConnectToService requests while async
  // operations are in progress.  This includes details about the caller and
  // service as well as the callback.  Requests will wait here in three cases:
  // * device::BluetoothAdapter::ConnectDevice()
  // * device::BluetoothDevice::ConnectToServiceInsecurely()
  // * device's services have not completed discovery
  base::flat_map<int, std::unique_ptr<ConnectToServiceRequestDetails>>
      connect_to_service_request_map_;

  // Ids of ConnectToServiceRequestDetails that are awaiting the completion of
  // service discovery for the given device.
  std::vector<int> connect_to_service_requests_pending_discovery_;

  base::flat_map<device::BluetoothUUID, std::unique_ptr<GattService>>
      uuid_to_local_gatt_service_map_;

  // Allowed UUIDs for untrusted clients to initiate outgoing connections, or
  // listen on incoming connections.
  std::set<device::BluetoothUUID> allowed_uuids_;

  int next_request_id_ = 0;

  base::WeakPtrFactory<Adapter> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADAPTER_H_
