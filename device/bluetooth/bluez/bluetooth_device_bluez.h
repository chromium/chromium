// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"

namespace device {
class BluetoothSocketThread;
class BluetoothUUID;
}  // namespace device

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothPairingBlueZ;

// The BluetoothDeviceBlueZ class implements BluetoothDevice for platforms using
// BlueZ.
//
// This class is not thread-safe, but is only called from the UI thread.
//
// A socket thread is used to create sockets but posts all callbacks on the UI
// thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceBlueZ
    : public device::BluetoothDevice,
      public bluez::BluetoothGattServiceClient::Observer {
 public:
  using GetServiceRecordsCallback =
      base::OnceCallback<void(const std::vector<BluetoothServiceRecordBlueZ>&)>;
  using GetServiceRecordsErrorCallback =
      base::OnceCallback<void(BluetoothServiceRecordBlueZ::ErrorCode)>;

  BluetoothDeviceBlueZ(const BluetoothDeviceBlueZ&) = delete;
  BluetoothDeviceBlueZ& operator=(const BluetoothDeviceBlueZ&) = delete;

  ~BluetoothDeviceBlueZ() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  device::BluetoothTransport GetType() const override;
  std::string GetIdentifier() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  std::optional<std::string> GetName() const override;
  bool IsPaired() const override;
#if BUILDFLAG(IS_CHROMEOS)
  bool IsBonded() const override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  std::optional<int8_t> GetInquiryRSSI() const override;
  std::optional<int8_t> GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS)
  void ConnectClassic(PairingDelegate* pairing_delegate,
                      ConnectCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const device::BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;
  std::unique_ptr<device::BluetoothGattConnection>
  CreateBluetoothGattConnectionObject() override;
  void SetGattServicesDiscoveryComplete(bool complete) override;
  bool IsGattServicesDiscoveryComplete() const override;
  void Pair(device::BluetoothDevice::PairingDelegate* pairing_delegate,
            ConnectCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS)
  void ExecuteWrite(base::OnceClosure callback,
                    ExecuteWriteErrorCallback error_callback) override;
  void AbortWrite(base::OnceClosure callback,
                  AbortWriteErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Invoked after a ConnectToService() or ConnectToServiceInsecurely() error,
  // to allow us to perform error handling before we invoke the
  // ConnectToServiceErrorCallback.
  void OnConnectToServiceError(ConnectToServiceErrorCallback error_callback,
                               const std::string& error_message);

  // Returns the complete list of service records discovered for on this
  // device via SDP. If called before discovery is complete, it may return
  // an incomplete list and/or stale cached records.
  void GetServiceRecords(GetServiceRecordsCallback callback,
                         GetServiceRecordsErrorCallback error_callback);

  // Called by BluetoothAdapterBlueZ to update BluetoothDevice->service_data_
  // when receive DevicePropertyChanged event for the service data property.
  // Note that
  // 1) BlueZ persists all service data meaning that BlueZ won't remove service
  //    data even when a device stops advertising service data.
  // 2) BlueZ sends DevicePropertyChanged event separately for each UUID that
  //    service data changed. Meaning that UpdateServiceData() might get called
  //    multiple times when there are multiple UUIDs that service data changed.
  // 3) When a device update service data for a UUID, BlueZ update data for that
  //    UUID if it is already exist. If not BlueZ adds that data for UUID.
  //    This means BlueZ won't remove service data even when a device stops
  //    advertising service data for a UUID.
  void UpdateServiceData();

  // Called by BluetoothAdapterBlueZ to update manufacturer_data_ defined in
  // BluetoothDevice when receive DevicePropertyChanged event for the
  // manufacturer data property. Note that same BlueZ implementation detail from
  // UpdateServiceData() also applies here.
  void UpdateManufacturerData();

  // Called by BluetoothAdapterBlueZ to update advertising_data_flags_ defined
  // in BluetoothDevice when receive DevicePropertyChanged event for the
  // advertising data flags property. Note that same BlueZ implementation detail
  // from UpdateServiceData() also applies here.
  void UpdateAdvertisingDataFlags();

  // Called by BluetoothAdapterBlueZ to update device_uuids_ defined in
  // BluetoothDevice when receiving DevicePropertyChanged event for the UUIDs
  // property. Note that BlueZ's implementation returns service UUIDs (SDP or
  // GATT) when they are available, otherwise it contains the EIR or
  // advertisement UUIDs. However, currently there is no way of knowing which
  // one we will get. Since the advertised UUIDs can be tracked while we receive
  // advertisement packets, here we assume it contains the service UUIDs. Both
  // are merged behind the scenes, so GetUUIDs() would return the expected
  // result.
  void UpdateServiceUUIDs();

  // Called by BluetoothAdapterBlueZ to update device_uuids_ defined in
  // BluetoothDevice when receiving advertisement data.
  void SetAdvertisedUUIDs(const BluetoothDevice::UUIDList& uuids);

  // Creates a pairing object with the given delegate |pairing_delegate| and
  // establishes it as the pairing context for this device. All pairing-related
  // method calls will be forwarded to this object until it is released.
  BluetoothPairingBlueZ* BeginPairing(
      BluetoothDevice::PairingDelegate* pairing_delegate);

  // Releases the current pairing object, any pairing-related method calls will
  // be ignored.
  void EndPairing();

  // Returns the current pairing object or NULL if no pairing is in progress.
  BluetoothPairingBlueZ* GetPairing() const;

  // Returns the object path of the device.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Returns the adapter which owns this device instance.
  BluetoothAdapterBlueZ* adapter() const;

 protected:
  // BluetoothDevice override
  void CreateGattConnectionImpl(
      std::optional<device::BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

 private:
  friend class BluetoothAdapterBlueZ;

  BluetoothDeviceBlueZ(
      BluetoothAdapterBlueZ* adapter,
      const dbus::ObjectPath& object_path,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);

  // bluez::BluetoothGattServiceClient::Observer overrides
  void GattServiceAdded(const dbus::ObjectPath& object_path) override;
  void GattServiceRemoved(const dbus::ObjectPath& object_path) override;

  // Called once all services have been discovered. Invokes
  // NotifyGattDiscoveryComplete() for services for which we haven't notified
  // before e.g. if a services is exposed during construction but services
  // haven't been resolved yet..
  void UpdateGattServices(const dbus::ObjectPath& object_path);

  // Called by dbus:: on completion of the D-Bus method call to get the
  // connection attributes of the current connection to the device.
  void OnGetConnInfo(ConnectionInfoCallback callback,
                     int16_t rssi,
                     int16_t transmit_power,
                     int16_t max_transmit_power);
  void OnGetConnInfoError(ConnectionInfoCallback callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to set the
  // connection parameters of the device.
  void OnSetLEConnectionParameters(base::OnceClosure callback);
  void OnSetLEConnectionParametersError(ErrorCallback callback,
                                        const std::string& error_name,
                                        const std::string& error_message);

  // Called by dbus:: in case of an error during the GetServiceRecords API call.
  void OnGetServiceRecordsError(GetServiceRecordsErrorCallback error_callback,
                                const std::string& error_name,
                                const std::string& error_message);

#if BUILDFLAG(IS_CHROMEOS)
  void OnExecuteWriteError(ExecuteWriteErrorCallback error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  void OnAbortWriteError(AbortWriteErrorCallback error_callback,
                         const std::string& error_name,
                         const std::string& error_message);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Internal methods to initiate a connection to this device, and methods
  // called by dbus:: on completion of the D-Bus method call.
  void ConnectInternal(ConnectCallback callback);
#if BUILDFLAG(IS_CHROMEOS)
  void ConnectClassicInternal(ConnectCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void OnConnect(ConnectCallback callback);
  void OnConnectError(ConnectCallback callback,
                      const std::string& error_name,
                      const std::string& error_message);

// Once DisconnectLE is supported on Linux, this buildflag will not be necessary
// (this bluez code is only run on Chrome OS and Linux).
#if BUILDFLAG(IS_CHROMEOS)
  void OnDisconnectLEError(const std::string& error_name,
                           const std::string& error_message);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Called by dbus:: on completion of the D-Bus method call to pair the device,
  // made inside |Connect()| and |ConnectClassic()|.
  void OnPairDuringConnect(ConnectCallback callback);
#if BUILDFLAG(IS_CHROMEOS)
  void OnPairDuringConnectClassic(ConnectCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)
  void OnPairDuringConnectError(ConnectCallback callback,
                                const std::string& error_name,
                                const std::string& error_message);

  // Called by dbus: on completion of the D-Bus method call to pair the device,
  // made inside |Pair()|.
  void OnPair(ConnectCallback callback);
  void OnPairError(ConnectCallback callback,
                   const std::string& error_name,
                   const std::string& error_message);

  // Called by dbus:: on failure of the D-Bus method call to cancel pairing,
  // there is no matching completion call since we don't do anything special
  // in that case.
  void OnCancelPairingError(const std::string& error_name,
                            const std::string& error_message);

  // Internal method to set the device as trusted. Trusted devices can connect
  // to us automatically, and we can connect to them after rebooting; it also
  // causes the device to be remembered by the stack even if not paired.
  // |success| to the callback indicates whether or not the request succeeded.
  void SetTrusted();
  void OnSetTrusted(bool success);

  // Called by dbus:: on completion of the D-Bus method call to disconnect the
  // device.
  void OnDisconnect(base::OnceClosure callback);
  void OnDisconnectError(ErrorCallback error_callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // The dbus object path of the device object.
  dbus::ObjectPath object_path_;

  // Number of ongoing calls to Connect().
  int num_connecting_calls_;

  // Keeps track of all services for which we've called
  // NotifyGattDiscoveryComplete().
  std::unordered_set<
      raw_ptr<device::BluetoothRemoteGattService, CtnExperimental>>
      discovery_complete_notified_;

  // UI thread task runner and socket thread object used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // During pairing this is set to an object that we don't own, but on which
  // we can make method calls to request, display or confirm PIN Codes and
  // Passkeys. Generally it is the object that owns this one.
  std::unique_ptr<BluetoothPairingBlueZ> pairing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceBlueZ> weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_DEVICE_BLUEZ_H_
