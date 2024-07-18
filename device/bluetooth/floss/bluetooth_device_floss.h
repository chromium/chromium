// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_pairing_floss.h"
#include "device/bluetooth/floss/bluetooth_socket_floss.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {

// BluetoothDeviceFloss implements device::BluetoothDevice for platforms using
// Floss (Linux front-end for Fluoride). Objects of this type should be managed
// by BluetoothAdapterFloss, which also manages device lifetimes.
//
// This class is not thread-safe but is only called from the UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceFloss
    : public device::BluetoothDevice,
      public FlossGattClientObserver {
 public:
  enum class ConnectingState {
    kIdle = 0,
    kACLConnecting,
    kProfilesConnecting,
    kProfilesConnected,
  };
  enum class GattConnectingState {
    kGattDisconnected = 0,
    kGattConnecting,
    kGattConnected,
    // Initial state, no prior connection built. Use direct connection.
    kGattConnectionInit,
  };
  enum PropertiesState : uint32_t {
    kNotRead = 0,
    kTriggeredByScan = 1 << 1,
    kTriggeredByInquiry = 1 << 2,
    kTriggeredbyBoth = (kTriggeredByScan | kTriggeredByInquiry)
  };

  BluetoothDeviceFloss(const BluetoothDeviceFloss&) = delete;
  BluetoothDeviceFloss& operator=(const BluetoothDeviceFloss&) = delete;

  BluetoothDeviceFloss(
      BluetoothAdapterFloss* adapter,
      const FlossDeviceId& device,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);
  ~BluetoothDeviceFloss() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  device::BluetoothTransport GetType() const override;
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
  void ConnectClassic(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
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
  BluetoothPairingFloss* BeginPairing(
      BluetoothDevice::PairingDelegate* pairing_delegate);
#if BUILDFLAG(IS_CHROMEOS)
  bool UsingReliableWrite() const { return using_reliable_write_; }
  void BeginReliableWrite();
  void ExecuteWrite(base::OnceClosure callback,
                    ExecuteWriteErrorCallback error_callback) override;
  void AbortWrite(base::OnceClosure callback,
                  AbortWriteErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  FlossDeviceId AsFlossDeviceId() const;
  // Floss always distinguishes between IsBonded and IsPaired so provide
  // a publicly accessible implementation.
  bool IsBondedImpl() const;
  void SetName(const std::string& name);
  FlossAdapterClient::BondState GetBondState() { return bond_state_; }
  void SetBondState(
      FlossAdapterClient::BondState bond_state,
      std::optional<BluetoothDevice::ConnectErrorCode> error_code);
  void SetIsConnected(bool is_connected);
  void SetConnectionState(uint32_t state);
  void ResetPairing();

  BluetoothPairingFloss* pairing() const { return pairing_.get(); }

  void InitializeDeviceProperties(PropertiesState state,
                                  base::OnceClosure callback);
  bool IsReadingProperties() const {
    return property_reads_triggered_ != PropertiesState::kNotRead;
  }
  bool HasReadProperties() const {
    return property_reads_completed_ != PropertiesState::kNotRead;
  }
  PropertiesState GetPropertiesState() const {
    return property_reads_completed_;
  }

  // FlossGattClientObserver overrides
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override;
  void GattSearchComplete(std::string address,
                          const std::vector<GattService>& services,
                          GattStatus status) override;
  void GattConnectionUpdated(std::string address,
                             int32_t interval,
                             int32_t latency,
                             int32_t timeout,
                             GattStatus status) override;
  void GattConfigureMtu(std::string address,
                        int32_t mtu,
                        GattStatus status) override;
#if BUILDFLAG(IS_CHROMEOS)
  void GattServiceChanged(std::string address) override;
  void GattExecuteWrite(std::string address, GattStatus status) override;
#endif

  // Returns the adapter which owns this device instance.
  BluetoothAdapterFloss* adapter() const {
    return static_cast<BluetoothAdapterFloss*>(adapter_);
  }

  // Methods for fetching device properties.
  void FetchRemoteType(base::OnceClosure callback);
  void FetchRemoteClass(base::OnceClosure callback);
  void FetchRemoteAppearance(base::OnceClosure callback);
  void FetchRemoteUuids(base::OnceClosure callback);
  void FetchRemoteVendorProductInfo(base::OnceClosure callback);
  void FetchRemoteAddressType(base::OnceClosure callback);

 protected:
  // BluetoothDevice override
  void CreateGattConnectionImpl(
      std::optional<device::BluetoothUUID> service_uuid) override;
  void UpgradeToFullDiscovery() override;
  void DisconnectGatt() override;

 private:
  // Invoked when no connection established during connecting.
  void ConnectionIncomplete();
  // Connects with specified transport
  void ConnectWithTransport(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      ConnectCallback callback,
      FlossAdapterClient::BluetoothTransport transport);
  // Method to connect profiles.
  void ConnectAllEnabledProfiles();
  // Updates the state of connecting and calls callbacks accordingly.
  void UpdateConnectingState(
      ConnectingState state,
      std::optional<BluetoothDevice::ConnectErrorCode> error);
  // Updates the state of gatt connecting.
  void UpdateGattConnectingState(GattConnectingState state);
  // Triggers the pending callback of Connect() method.
  void TriggerConnectCallback(
      std::optional<BluetoothDevice::ConnectErrorCode> error_code);

  void OnGetRemoteType(base::OnceClosure callback,
                       DBusResult<FlossAdapterClient::BluetoothDeviceType> ret);
  void OnGetRemoteClass(base::OnceClosure callback, DBusResult<uint32_t> ret);
  void OnGetRemoteAppearance(base::OnceClosure callback,
                             DBusResult<uint16_t> ret);
  void OnGetRemoteVendorProductInfo(
      base::OnceClosure callback,
      DBusResult<FlossAdapterClient::VendorProductInfo> ret);
  void OnGetRemoteUuids(base::OnceClosure callback, DBusResult<UUIDList> ret);
  void OnGetRemoteAddressType(
      base::OnceClosure callback,
      DBusResult<FlossAdapterClient::BtAddressType> ret);
  void OnConnectAllEnabledProfiles(DBusResult<Void> ret);
  void OnConnectAllEnabledProfiles(DBusResult<FlossDBusClient::BtifStatus> ret);
  void OnDisconnectAllEnabledProfiles(base::OnceClosure callback,
                                      ErrorCallback error_callback,
                                      DBusResult<Void> ret);

  // Intercept errors when connecting to an L2CAP or RFCOMM socket. This keeps
  // a reference to the |socket| so that it does not go out of scope until after
  // the error is completed.
  void OnConnectToServiceError(scoped_refptr<BluetoothSocketFloss> socket,
                               ConnectToServiceErrorCallback error_callback,
                               const std::string& error_message);

  void TriggerInitDevicePropertiesCallback();
  void OnConnectGatt(DBusResult<Void> ret);
  void OnSetConnectionLatency(base::OnceClosure callback,
                              ErrorCallback error_callback,
                              DBusResult<Void> ret);
  void OnCreateBond(DBusResult<bool> ret);
  void OnCreateBond(DBusResult<FlossDBusClient::BtifStatus> ret);

#if BUILDFLAG(IS_CHROMEOS)
  void OnExecuteWrite(base::OnceClosure callback,
                      ExecuteWriteErrorCallback error_callback,
                      DBusResult<Void> ret);
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::optional<ConnectCallback> pending_callback_on_connect_profiles_ =
      std::nullopt;

  // Timer to stop waiting for a successful connect complete.
  base::OneShotTimer connection_incomplete_timer_;

  std::optional<base::OnceClosure> pending_callback_on_init_props_ =
      std::nullopt;

  // Callbacks for a pending |SetConnectionLatency|.
  std::optional<std::pair<base::OnceClosure, ErrorCallback>>
      pending_set_connection_latency_ = std::nullopt;

#if BUILDFLAG(IS_CHROMEOS)
  // Callbacks for a pending |ExecuteWrite| or |AbortWrite|.
  std::optional<std::pair<base::OnceClosure, ExecuteWriteErrorCallback>>
      pending_execute_write_ = std::nullopt;

  // Writes are using reliable writes.
  bool using_reliable_write_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Number of pending device properties to initialize
  int num_pending_properties_ = 0;

  // Address of this device. Changing this should necessitate creating a new
  // BluetoothDeviceFloss object.
  const std::string address_;

  // Name of this device. Can be queried later and isn't mandatory for creation.
  std::string name_;

  // Transport type of device.
  device::BluetoothTransport transport_ =
      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC;

  // Class of device.
  uint32_t cod_ = 0;

  // Appearance of device.
  uint16_t appearance_ = 0;

  // Vendor and product info of device.
  FlossAdapterClient::VendorProductInfo vpi_;

  // Address type of device.
  AddressType address_type_ = AddressType::ADDR_TYPE_UNKNOWN;

  // Whether the device is bonded/paired.
  FlossAdapterClient::BondState bond_state_ =
      FlossAdapterClient::BondState::kNotBonded;

  // Whether the device is connected at link layer level (not profile level).
  // Updated via |SetIsConnected| only.
  bool is_acl_connected_ = false;

  // Are all services resolved? Only true if full discovery is completed. See
  // |IsGattServicesDiscoveryComplete| for more info.
  bool svc_resolved_ = false;

  // Have we triggered initial property reads?
  PropertiesState property_reads_triggered_ = PropertiesState::kNotRead;

  // Have we completed reading properties?
  PropertiesState property_reads_completed_ = PropertiesState::kNotRead;

  // Specific uuid to search for after gatt connection is established. If this
  // is not set, then we do full discovery.
  std::optional<device::BluetoothUUID> search_uuid;

  // Similar to is_acl_connected_ but contains the full connection state
  // (including encryption). This is updated when |SetConnectionState| is called
  // or when |SetIsConnected| is called.
  // (https://android.googlesource.com/platform/system/bt/+/refs/heads/android10-c2f2-release/btif/src/btif_dm.cc#737),
  // This is used for determining if the device is paired.
  uint32_t connection_state_ = 0;

  // The status of profile connecting.
  ConnectingState connecting_state_ = ConnectingState::kIdle;

  // The status of GATT connecting.
  GattConnectingState gatt_connecting_state_ =
      GattConnectingState::kGattConnectionInit;

  // UI thread task runner and socket thread used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // Represents currently ongoing pairing with this remote device.
  std::unique_ptr<BluetoothPairingFloss> pairing_;

  base::WeakPtrFactory<BluetoothDeviceFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_
