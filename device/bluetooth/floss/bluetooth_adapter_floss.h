// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/floss/bluetooth_low_energy_scan_session_floss.h"
#include "device/bluetooth/floss/bluetooth_socket_floss.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_battery_manager_client.h"
#include "device/bluetooth/floss/floss_bluetooth_telephony_client.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"
#include "device/bluetooth/floss/floss_lescan_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/floss/floss_admin_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace floss {

class BluetoothDeviceFloss;
class BluetoothAdvertisementFloss;
class BluetoothLocalGattServiceFloss;
class BluetoothLocalGattCharacteristicFloss;

// The BluetoothAdapterFloss class implements BluetoothAdapter for platforms
// that use Floss, a dbus front-end for the Fluoride Bluetooth stack.
//
// Floss separates the "Powered" management of adapters in a separate manager
// interface. This class will first initialize the manager interface before
// initializing any clients that depend on a specific adapter being targeted.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFloss final
    : public device::BluetoothAdapter,
      public floss::FlossManagerClient::Observer,
      public floss::FlossAdapterClient::Observer,
      public floss::FlossBatteryManagerClient::
          FlossBatteryManagerClientObserver,
#if BUILDFLAG(IS_CHROMEOS)
      public FlossAdminClientObserver,
#endif  // BUILDFLAG(IS_CHROMEOS)
      public ScannerClientObserver {
 public:
  static scoped_refptr<BluetoothAdapterFloss> CreateAdapter();

  BluetoothAdapterFloss(const BluetoothAdapterFloss&) = delete;
  BluetoothAdapterFloss& operator=(const BluetoothAdapterFloss&) = delete;

  // BluetoothAdapter:
  void Initialize(base::OnceClosure callback) override;
  void Shutdown() override;

  UUIDList GetUUIDs() const override;

  std::string GetAddress() const override;
  std::string GetName() const override;
  std::string GetSystemName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;

  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  base::TimeDelta GetDiscoverableTimeout() const override;
  bool IsDiscovering() const override;

  std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const device::BluetoothDiscoveryFilter& discovery_filter) override;
  void CreateRfcommService(const device::BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override;
  void CreateL2capService(const device::BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override;

  // Intercept errors when creating an RFCOMM or L2CAP service. This keeps
  // a reference to the |socket| so that it does not go out of scope until after
  // the error is completed.
  void OnCreateServiceError(scoped_refptr<BluetoothSocketFloss> socket,
                            CreateServiceErrorCallback error_callback,
                            const std::string& error_message);

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;

#if BUILDFLAG(IS_CHROMEOS)
  bool IsExtendedAdvertisementsAvailable() const override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override;

  void ResetAdvertising(base::OnceClosure callback,
                        AdvertisementErrorCallback error_callback) override;

  void ConnectDevice(
      const std::string& address,
      const std::optional<device::BluetoothDevice::AddressType>& address_type,
      ConnectDeviceCallback callback,
      ConnectDeviceErrorCallback error_callback) override;

  device::BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  base::WeakPtr<device::BluetoothLocalGattService> CreateLocalGattService(
      const device::BluetoothUUID& uuid,
      bool is_primary,
      device::BluetoothLocalGattService::Delegate* delegate) override;

  // Register a GATT service. The service must belong to this adapter.
  void RegisterGattService(BluetoothLocalGattServiceFloss* service);

  // Gatt service added dbus hook.
  void OnGattServiceAdded(BluetoothLocalGattServiceFloss* service,
                          DBusResult<Void> ret);

  // Unregister a GATT service. The service must already be registered.
  void UnregisterGattService(BluetoothLocalGattServiceFloss* service);

  // Gatt service removed dbus hook.
  void OnGattServiceRemoved(BluetoothLocalGattServiceFloss* service,
                            DBusResult<Void> ret);

  void AddLocalGattService(
      std::unique_ptr<BluetoothLocalGattServiceFloss> service);

  void RemoveLocalGattService(BluetoothLocalGattServiceFloss* service);

  // Returns if a given service is currently registered.
  bool IsGattServiceRegistered(BluetoothLocalGattServiceFloss* service);

  // Send a notification for this characteristic that its value has been
  // updated. If the service that owns that characteristic is not registered,
  // this method will return false.
  bool SendValueChanged(BluetoothLocalGattCharacteristicFloss* characteristic,
                        const std::vector<uint8_t>& value);

#if BUILDFLAG(IS_CHROMEOS)
  void SetServiceAllowList(const UUIDList& uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;

  LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override;

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate)
      override;

  std::vector<BluetoothRole> GetSupportedRoles() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set the adapter name to one chosen from the system information. Only Ash
  // needs to do this.
  void SetStandardChromeOSAdapterName() override;
  // Enable telephony feature for floss. Only Ash needs to do this.
  void ConfigureBluetoothTelephony(bool enabled);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // ScannerClientObserver overrides
  void ScannerRegistered(device::BluetoothUUID uuid,
                         uint8_t scanner_id,
                         GattStatus status) override;
  void ScanResultReceived(ScanResult scan_result) override;
  void AdvertisementFound(uint8_t scanner_id, ScanResult scan_result) override;
  void AdvertisementLost(uint8_t scanner_id, ScanResult scan_result) override;

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  BluetoothAdapterFloss();
  ~BluetoothAdapterFloss() override;

  // Init will get asynchronouly called once we know if Object Manager is
  // supported.
  void Init();

  void NotifyDeviceFound(uint8_t scanner_id, const std::string& address);
  BluetoothDeviceFloss* CreateOrGetDeviceForUpdate(const std::string& address,
                                                   const std::string& name);

  // Helper function to create a Floss device
  std::unique_ptr<BluetoothDeviceFloss> CreateBluetoothDeviceFloss(
      FlossDeviceId device);

  // Helper function to update device properties if necessary
  void UpdateDeviceProperties(bool is_triggered_by_inquiry,
                              const FlossDeviceId& device_found);

  // Handle responses to most method calls
  void OnMethodResponse(base::OnceClosure callback,
                        ErrorCallback error_callback,
                        DBusResult<Void> ret);

  // Handle when discovery is automatically repeated based on active sessions.
  void OnRepeatedDiscoverySessionResult(
      bool start_discovery,
      bool is_error,
      device::UMABluetoothDiscoverySessionOutcome outcome);

  // Called on completion of start discovery and stop discovery
  void OnStartDiscovery(DiscoverySessionResultCallback callback,
                        DBusResult<Void> ret);
  void OnStopDiscovery(DiscoverySessionResultCallback callback,
                       DBusResult<Void> ret);
  // Called when all device properties have been initialized
  void OnInitializeDeviceProperties(BluetoothDeviceFloss* device_ptr);
  // Called once the battery data for a specific device has been fetched
  void OnGetBatteryInformation(
      DBusResult<std::optional<BatterySet>> battery_set);
  // Called when the UUIDs property changed and fetched.
  void OnDeviceUuidsChanged(BluetoothDeviceFloss* device_ptr);
  void OnGetConnectionState(const FlossDeviceId& device_id,
                            DBusResult<uint32_t> ret);
  void OnGetBondState(const FlossDeviceId& device_id, DBusResult<uint32_t> ret);

  // Announce to observers a change in the adapter state.
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);
  void NotifyAdapterPoweredChanged(bool powered);

  // Announce to observers that |device| has changed its connected state.
  void NotifyDeviceConnectedStateChanged(BluetoothDeviceFloss* device,
                                         bool is_now_connected);

  // Observers
  // floss::FlossManagerClient::Observer override.
  void AdapterPresent(int adapter, bool present) override;
  void AdapterEnabledChanged(int adapter, bool enabled) override;

  // Complete adapter present/enabled changes after adapter clients are ready.
  // Invoke PresentChanged to the observers only when |is_newly_present| is
  // true.
  void OnAdapterClientsReady(bool enabled, bool is_newly_present);

  // Initialize observers for adapter dependent clients. We need to add + remove
  // these observers whenever we get a powered notification.
  void AddAdapterObservers();
  void RemoveAdapterObservers();

  // Remove any active adapters.
  void RemoveAdapter();

  void PopulateInitialDevices();

  // floss::FlossAdapterClient::Observer override.
  void DiscoverableChanged(bool discoverable) override;
  void AdapterDiscoveringChanged(bool state) override;
  void AdapterFoundDevice(const FlossDeviceId& device_found) override;
  void AdapterClearedDevice(const FlossDeviceId& device_found) override;
  void AdapterDevicePropertyChanged(
      FlossAdapterClient::BtPropertyType prop_type,
      const FlossDeviceId& device) override;
  void AdapterSspRequest(const FlossDeviceId& remote_device,
                         uint32_t cod,
                         FlossAdapterClient::BluetoothSspVariant variant,
                         uint32_t passkey) override;
  void AdapterPinDisplay(const FlossDeviceId& remote_device,
                         std::string pincode) override;
  void AdapterPinRequest(const FlossDeviceId& remote_device,
                         uint32_t cod,
                         bool min_16_digit) override;
  void DeviceBondStateChanged(
      const FlossDeviceId& remote_device,
      uint32_t status,
      FlossAdapterClient::BondState bond_state) override;
  void AdapterDeviceConnected(const FlossDeviceId& device_id) override;
  void AdapterDeviceDisconnected(const FlossDeviceId& device_id) override;

  // floss::FlossBatteryManagerClient::FlossBatteryManagerClientObserver
  // override.
  void BatteryInfoUpdated(std::string remote_address,
                          BatterySet battery_set) override;
#if BUILDFLAG(IS_CHROMEOS)
  // floss::FlossAdminClientObserver override.
  void DevicePolicyEffectChanged(
      const FlossDeviceId& device_id,
      const std::optional<PolicyEffect>& effect) override;
  void ServiceAllowlistChanged(
      const std::vector<device::BluetoothUUID>& allowlist) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // BluetoothAdapter:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;

  void OnRegisterScanner(
      base::WeakPtr<BluetoothLowEnergyScanSessionFloss> scan_session,
      DBusResult<device::BluetoothUUID> ret);
  void OnStartScan(device::BluetoothUUID uuid,
                   uint8_t scanner_id,
                   DBusResult<FlossDBusClient::BtifStatus> ret);
  void OnLowEnergyScanSessionDestroyed(const std::string& uuid_str);
  void OnUnregisterScanner(uint8_t scanner_id, DBusResult<bool> ret);

  std::map<device::BluetoothUUID,
           base::WeakPtr<BluetoothLowEnergyScanSessionFloss>>
      scanners_;

  base::OnceClosure init_callback_;

  // Keeps track of whether the adapter is fully initialized.
  bool initialized_ = false;

  // Keeps track of whether Shutdown is called (and dbus clients are cleaned
  // up properly).
  bool dbus_is_shutdown_ = false;

  // Socket thread object used to create sockets. Public socket apis are run on
  // the ui thread but socket operations (including connect/disconnect) will be
  // run in this thread. See |BluetoothSocketNet| for more details.
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // List of advertisements registered with this adapter. This list is used
  // to ensure we unregister any advertisements that were registered with
  // this adapter on adapter shutdown. This is a sub-optimal solution since
  // we'll keep a list of all advertisements ever created by this adapter (the
  // unregistered ones will just be inactive). This will be fixed with
  // crbug.com/687396.
  std::vector<scoped_refptr<BluetoothAdvertisementFloss>> advertisements_;

  // While we are doing discovery, we will also maintain a separate LE scan
  // session to respond on the current discovery session.
  std::unique_ptr<device::BluetoothLowEnergyScanSession> le_discovery_session_ =
      nullptr;

  // Delegate for forwarding scan session notifications.
  std::unique_ptr<device::BluetoothLowEnergyScanSession::Delegate>
      le_discovery_session_delegate_;

  // Default BLE advertising interval.
  // 100 ms is one of the recommended values on Floss AdvertisingSetParameters.
  // b/253718595 will provide a 'no preference' option so that Floss can choose
  // a default value for the advertising interval.
  uint16_t interval_ms_ = 100;

  // List of GATT services that are owned by this adapter.
  base::flat_map<std::string, std::unique_ptr<BluetoothLocalGattServiceFloss>>
      owned_gatt_services_;

  base::WeakPtrFactory<BluetoothAdapterFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_
