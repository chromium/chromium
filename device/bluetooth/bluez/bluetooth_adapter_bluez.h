// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_battery_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <optional>

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace base {
class TimeDelta;
}  // namespace base

namespace device {
class BluetoothDevice;
class BluetoothSocketThread;
class BluetoothTestBlueZ;
}  // namespace device

namespace bluez {

class BluetoothBlueZTest;
class BluetoothAdapterProfileBlueZ;
class BluetoothAdvertisementBlueZ;
#if BUILDFLAG(IS_CHROMEOS)
class BluetoothAdvertisementMonitorApplicationServiceProvider;
class BluetoothAdvertisementMonitorServiceProvider;
#endif  // BUILDFLAG(IS_CHROMEOS)
class BluetoothDeviceBlueZ;
class BluetoothLocalGattCharacteristicBlueZ;
class BluetoothLocalGattServiceBlueZ;
class BluetoothGattApplicationServiceProvider;
class BluetoothPairingBlueZ;

// The BluetoothAdapterBlueZ class implements BluetoothAdapter for platforms
// that use BlueZ.
//
// All methods are called from the dbus origin / UI thread and are generally
// not assumed to be thread-safe.
//
// This class interacts with sockets using the BluetoothSocketThread to ensure
// single-threaded calls, and posts tasks to the UI thread.
//
// Methods tolerate a shutdown scenario where BluetoothAdapterBlueZ::Shutdown
// causes IsPresent to return false just before the dbus system is shutdown but
// while references to the BluetoothAdapterBlueZ object still exists.
//
// When adding methods to this class verify shutdown behavior in
// BluetoothBlueZTest, Shutdown.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterBlueZ final
    : public device::BluetoothAdapter,
      public bluez::BluetoothAdapterClient::Observer,
      public bluez::BluetoothAdminPolicyClient::Observer,
      public bluez::BluetoothBatteryClient::Observer,
      public bluez::BluetoothDeviceClient::Observer,
      public bluez::BluetoothInputClient::Observer,
      public bluez::BluetoothAgentManagerClient::Observer,
      public bluez::BluetoothLEAdvertisingManagerClient::Observer,
#if BUILDFLAG(IS_CHROMEOS)
      public bluez::BluetoothAdvertisementMonitorManagerClient::Observer,
#endif  // BUILDFLAG(IS_CHROMEOS)
      public bluez::BluetoothAgentServiceProvider::Delegate {
 public:
  using ErrorCompletionCallback =
      base::OnceCallback<void(const std::string& error_message)>;
  using ProfileRegisteredCallback =
      base::OnceCallback<void(BluetoothAdapterProfileBlueZ* profile)>;
  using ServiceRecordCallback = base::OnceCallback<void(uint32_t)>;
  using ServiceRecordErrorCallback =
      base::OnceCallback<void(BluetoothServiceRecordBlueZ::ErrorCode)>;

#if BUILDFLAG(IS_CHROMEOS)
  using ScanRecordPtr = data_decoder::mojom::ScanRecordPtr;
  using ScanRecordCallback = base::OnceCallback<void(ScanRecordPtr)>;
#endif  // BUILDFLAG(IS_CHROMEOS)

  static scoped_refptr<BluetoothAdapterBlueZ> CreateAdapter();

  BluetoothAdapterBlueZ(const BluetoothAdapterBlueZ&) = delete;
  BluetoothAdapterBlueZ& operator=(const BluetoothAdapterBlueZ&) = delete;

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
  bool IsDiscoveringForTesting() const;
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // These functions are specifically for use with ARC. They have no need to
  // exist for other platforms, hence we're putting them directly in the BlueZ
  // specific code.

  // Creates a service record with the SDP server running on this adapter. This
  // only creates the record, it does not create a listening socket for the
  // service.
  void CreateServiceRecord(const BluetoothServiceRecordBlueZ& record,
                           ServiceRecordCallback callback,
                           ServiceRecordErrorCallback error_callback);

  // Removes a service record from the SDP server. This would result in the
  // service not being discoverable in any further scans of the adapter. Any
  // sockets listening on this service will need to be closed separately.
  void RemoveServiceRecord(uint32_t handle,
                           base::OnceClosure callback,
                           ServiceRecordErrorCallback error_callback);

  // Locates the device object by object path (the devices map and
  // BluetoothDevice methods are by address).
  BluetoothDeviceBlueZ* GetDeviceWithPath(const dbus::ObjectPath& object_path);

  // Announce to observers a device address change.
  void NotifyDeviceAddressChanged(BluetoothDeviceBlueZ* device,
                                  const std::string& old_address);

  // Announce to observers MTU change in ATT communication to |device|.
  void NotifyDeviceMTUChanged(BluetoothDeviceBlueZ* device, uint16_t mtu);

  // Announce to observers advertisement received from |device|.
  void NotifyDeviceAdvertisementReceived(BluetoothDeviceBlueZ* device,
                                         int16_t rssi,
                                         const std::vector<uint8_t>& eir);

#if BUILDFLAG(IS_CHROMEOS)
  // Announce to observers advertisement received from |device|.
  void OnAdvertisementReceived(std::string device_address,
                               std::string device_name,
                               uint8_t rssi,
                               uint16_t device_appearance,
                               const dbus::ObjectPath& device_path,
                               ScanRecordPtr scan_record);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Announce to observers that |device| has changed its connected state.
  void NotifyDeviceConnectedStateChanged(BluetoothDeviceBlueZ* device,
                                         bool is_now_connected);

  // Returns the object path of the adapter.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Request a profile on the adapter for a custom service with a
  // specific UUID for the device at |device_path| to be sent to |delegate|.
  // If |device_path| is the empty string, incoming connections will be
  // assigned to |delegate|.  When the profile is
  // successfully registered, |success_callback| will be called with a pointer
  // to the profile which is managed by BluetoothAdapterBlueZ.  On failure,
  // |error_callback| will be called.
  void UseProfile(const device::BluetoothUUID& uuid,
                  const dbus::ObjectPath& device_path,
                  const bluez::BluetoothProfileManagerClient::Options& options,
                  bluez::BluetoothProfileServiceProvider::Delegate* delegate,
                  ProfileRegisteredCallback success_callback,
                  ErrorCompletionCallback error_callback);

  // Release use of a profile by a device.
  void ReleaseProfile(const dbus::ObjectPath& device_path,
                      BluetoothAdapterProfileBlueZ* profile);

  // Add a local GATT service to the list of services owned by this adapter.
  void AddLocalGattService(
      std::unique_ptr<BluetoothLocalGattServiceBlueZ> service);

  // Removes a local GATT service from the list of services owned by this
  // adapter and deletes it. If the service was registered, it is unregistered.
  void RemoveLocalGattService(BluetoothLocalGattServiceBlueZ* service);

  // Register a GATT service. The service must belong to this adapter.
  void RegisterGattService(
      BluetoothLocalGattServiceBlueZ* service,
      base::OnceClosure callback,
      device::BluetoothGattService::ErrorCallback error_callback);

  // Unregister a GATT service. The service must already be registered.
  void UnregisterGattService(
      BluetoothLocalGattServiceBlueZ* service,
      base::OnceClosure callback,
      device::BluetoothGattService::ErrorCallback error_callback);

  // Returns if a given service is currently registered.
  bool IsGattServiceRegistered(BluetoothLocalGattServiceBlueZ* service);

  // Send a notification for this characteristic that its value has been
  // updated. If the service that owns that characteristic is not registered,
  // this method will return false.
  bool SendValueChanged(BluetoothLocalGattCharacteristicBlueZ* characteristic,
                        const std::vector<uint8_t>& value);

  // Returns the object path of the adapter.
  dbus::ObjectPath GetApplicationObjectPath() const;

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest, Shutdown);
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest, Shutdown_OnStartDiscovery);
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest, Shutdown_OnStartDiscoveryError);
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest, Shutdown_OnStopDiscovery);
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest, Shutdown_OnStopDiscoveryError);
  FRIEND_TEST_ALL_PREFIXES(BluetoothBlueZTest,
                           StartDiscovery_DiscoveringStopped_StartAgain);
  friend class BluetoothBlueZTest;
  friend class device::BluetoothTestBlueZ;

  // typedef for callback parameters that are passed to AddDiscoverySession
  // and RemoveDiscoverySession. This is used to queue incoming requests while
  // a call to BlueZ is pending.
  using DiscoveryCallbackQueue = base::queue<DiscoverySessionResultCallback>;

  // Callback pair for the profile registration queue.
  using RegisterProfileCompletionPair =
      std::pair<base::OnceClosure, ErrorCompletionCallback>;

  explicit BluetoothAdapterBlueZ();
  ~BluetoothAdapterBlueZ() override;

  // Init will get asynchronouly called once we know if Object Manager is
  // supported.
  void Init();

  // bluez::BluetoothAdapterClient::Observer override.
  void AdapterAdded(const dbus::ObjectPath& object_path) override;
  void AdapterRemoved(const dbus::ObjectPath& object_path) override;
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override;

  // bluez::BluetoothAdminPolicyClient::Observer override.
  void AdminPolicyAdded(const dbus::ObjectPath& object_path) override;
  void AdminPolicyRemoved(const dbus::ObjectPath& object_path) override;
  void AdminPolicyPropertyChanged(const dbus::ObjectPath& object_path,
                                  const std::string& property_name) override;

  // bluez::BluetoothBatteryClient::Observer override.
  void BatteryAdded(const dbus::ObjectPath& object_path) override;
  void BatteryRemoved(const dbus::ObjectPath& object_path) override;
  void BatteryPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override;

  // bluez::BluetoothDeviceClient::Observer override.
  void DeviceAdded(const dbus::ObjectPath& object_path) override;
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // bluez::BluetoothInputClient::Observer override.
  void InputPropertyChanged(const dbus::ObjectPath& object_path,
                            const std::string& property_name) override;

  // bluez::BluetoothAgentManagerClient::Observer override.
  void AgentManagerAdded(const dbus::ObjectPath& object_path) override;
  void AgentManagerRemoved(const dbus::ObjectPath& object_path) override;

#if BUILDFLAG(IS_CHROMEOS)
  // bluez::BluetoothAdvertisementMonitorManagerClient::Observer override.
  void SupportedAdvertisementMonitorFeaturesChanged() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // bluez::BluetoothAgentServiceProvider::Delegate override.
  void Released() override;
  void RequestPinCode(const dbus::ObjectPath& device_path,
                      PinCodeCallback callback) override;
  void DisplayPinCode(const dbus::ObjectPath& device_path,
                      const std::string& pincode) override;
  void RequestPasskey(const dbus::ObjectPath& device_path,
                      PasskeyCallback callback) override;
  void DisplayPasskey(const dbus::ObjectPath& device_path,
                      uint32_t passkey,
                      uint16_t entered) override;
  void RequestConfirmation(const dbus::ObjectPath& device_path,
                           uint32_t passkey,
                           ConfirmationCallback callback) override;
  void RequestAuthorization(const dbus::ObjectPath& device_path,
                            ConfirmationCallback callback) override;
  void AuthorizeService(const dbus::ObjectPath& device_path,
                        const std::string& uuid,
                        ConfirmationCallback callback) override;
  void Cancel() override;

#if BUILDFLAG(IS_CHROMEOS)
  // Called by dbus:: on completion of the D-Bus method call to update
  // bluetooth devcoredump state.
  void OnSetDevCoredumpSuccess();
  void OnSetDevCoredumpError(const std::string& error_name,
                             const std::string& error_message);
#endif // BUILDFLAG(IS_CHROMEOS)

  // Called by dbus:: on completion of the D-Bus method call to enable LL
  // privacy.
  void OnSetLLPrivacySuccess();
  void OnSetLLPrivacyError(const std::string& error_name,
                           const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent();
  void OnRegisterAgentError(const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to request that
  // the pairing agent be made the default.
  void OnRequestDefaultAgent();
  void OnRequestDefaultAgentError(const std::string& error_name,
                                  const std::string& error_message);

  // Internal method to obtain a BluetoothPairingBlueZ object for the device
  // with path |object_path|. Returns the existing pairing object if the device
  // already has one (usually an outgoing connection in progress) or a new
  // pairing object with the default pairing delegate if not. If no default
  // pairing object exists, NULL will be returned.
  BluetoothPairingBlueZ* GetPairing(const dbus::ObjectPath& object_path);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void DiscoverableChanged(bool discoverable);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(base::OnceClosure callback,
                         ErrorCallback error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(base::OnceClosure callback,
                                 ErrorCallback error_callback,
                                 bool success);

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
  void SetDiscoveryFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      base::OnceClosure callback,
      DiscoverySessionErrorCallback error_callback);

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(DiscoverySessionResultCallback callback);
  void OnStartDiscoveryError(DiscoverySessionResultCallback callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(base::OnceClosure callback);
  void OnStopDiscoveryError(DiscoverySessionErrorCallback error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  void OnPreSetDiscoveryFilter(DiscoverySessionResultCallback callback);
  void OnPreSetDiscoveryFilterError(
      DiscoverySessionErrorCallback error_callback,
      device::UMABluetoothDiscoverySessionOutcome outcome);
  void OnSetDiscoveryFilter(base::OnceClosure callback,
                            DiscoverySessionErrorCallback error_callback);
  void OnSetDiscoveryFilterError(DiscoverySessionErrorCallback error_callback,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method to register a profile.
  void OnRegisterProfile(const device::BluetoothUUID& uuid,
                         std::unique_ptr<BluetoothAdapterProfileBlueZ> profile);

  void SetProfileDelegate(
      const device::BluetoothUUID& uuid,
      const dbus::ObjectPath& device_path,
      bluez::BluetoothProfileServiceProvider::Delegate* delegate,
      ProfileRegisteredCallback success_callback,
      ErrorCompletionCallback error_callback);
  void OnRegisterProfileError(const device::BluetoothUUID& uuid,
                              const std::string& error_name,
                              const std::string& error_message);

  // Called by BluetoothAdapterProfileBlueZ when no users of a profile
  // remain.
  void RemoveProfile(const device::BluetoothUUID& uuid);

  // Make the call to GattManager1 to unregister then re-register the GATT
  // application. If the ignore_unregister_failure flag is set, we attempt to
  // register even if the initial unregister call fails.
  void UpdateRegisteredApplication(
      bool ignore_unregister_failure,
      base::OnceClosure callback,
      device::BluetoothGattService::ErrorCallback error_callback);

  // Make the call to GattManager1 to register the services currently
  // registered.
  void RegisterApplication(
      base::OnceClosure callback,
      device::BluetoothGattService::ErrorCallback error_callback);

  // Register application, ignoring the given errors. Used to register a GATT
  // application even if a previous unregister application call fails.
  void RegisterApplicationOnError(
      base::OnceClosure callback,
      device::BluetoothGattService::ErrorCallback error_callback,
      const std::string& error_name,
      const std::string& error_message);

  // Called by dbus:: on an error while trying to create or remove a service
  // record. Translates the error name/message into a
  // BluetoothServiceRecordBlueZ::ErrorCode value.
  void ServiceRecordErrorConnector(ServiceRecordErrorCallback error_callback,
                                   const std::string& error_name,
                                   const std::string& error_message);

  void OnConnectDevice(ConnectDeviceCallback callback,
                       const dbus::ObjectPath& object_path);
  void OnConnectDeviceError(ConnectDeviceErrorCallback error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Updates |is_blocked_by_policy| field of a device based on its corresponding
  // value in Admin Policy interface. Should be called when receiving events
  // about admin policy addition, change, or removal.
  void UpdateDeviceAdminPolicyFromAdminPolicyClient(
      const dbus::ObjectPath& object_path);

  // Updates |battery_percentage| field of a device based on its corresponding
  // value in Battery interface. Should be called when receiving events about
  // battery object addition, change, or removal.
  void UpdateDeviceBatteryLevelFromBatteryClient(
      const dbus::ObjectPath& object_path);

#if BUILDFLAG(IS_CHROMEOS)
  void RegisterAdvertisementMonitorApplicationServiceProvider();
  void OnRegisterAdvertisementMonitorApplicationServiceProvider();

  // Unregister the underlying advertisement monitor through
  // |advertisement_monitor_application_provider_|.
  void OnLowEnergyScanSessionDestroyed(const std::string& session_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::OnceClosure init_callback_;

  bool initialized_;

  // Set in |Shutdown()|, makes IsPresent()| return false.
  bool dbus_is_shutdown_;

  // List of queued requests to add new discovery sessions. While there is a
  // pending request to BlueZ to start or stop discovery, many requests from
  // within Chrome to start or stop discovery sessions may occur. We only
  // queue requests to add new sessions to be processed later. All requests to
  // remove a session while a call is pending immediately return failure. Note
  // that since BlueZ keeps its own reference count of applications that have
  // requested discovery, dropping our count to 0 won't necessarily result in
  // the controller actually stopping discovery if, for example, an application
  // other than Chrome, such as bt_console, was also used to start discovery.
  DiscoveryCallbackQueue discovery_request_queue_;

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // Instance of the D-Bus agent object used for pairing, initialized with
  // our own class as its delegate.
  std::unique_ptr<bluez::BluetoothAgentServiceProvider> agent_;

  // Socket thread object used to create sockets.
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // The profiles we have registered with the bluetooth daemon.
  std::map<device::BluetoothUUID,
           raw_ptr<BluetoothAdapterProfileBlueZ, CtnExperimental>>
      profiles_;

  // Profiles that have been released and are pending removal.
  std::map<device::BluetoothUUID,
           raw_ptr<BluetoothAdapterProfileBlueZ, CtnExperimental>>
      released_profiles_;

  // Queue of delegates waiting for a profile to register.
  std::map<device::BluetoothUUID,
           raw_ptr<std::vector<RegisterProfileCompletionPair>, CtnExperimental>>
      profile_queues_;

  // List of GATT services that are owned by this adapter.
  std::map<dbus::ObjectPath, std::unique_ptr<BluetoothLocalGattServiceBlueZ>>
      owned_gatt_services_;

  // GATT services that are currently available on the GATT server.
  std::map<dbus::ObjectPath,
           raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>
      registered_gatt_services_;

  // DBus Object Manager that acts as a service provider for all the services
  // that are registered with this adapter.
  std::unique_ptr<BluetoothGattApplicationServiceProvider>
      gatt_application_provider_;

  // List of advertisements registered with this adapter. This list is used
  // to ensure we unregister any advertisements that were registered with
  // this adapter on adapter shutdown. This is a sub-optimal solution since
  // we'll keep a list of all advertisements ever created by this adapter (the
  // unregistered ones will just be inactive). This will be fixed with
  // crbug.com/687396.
  std::vector<scoped_refptr<BluetoothAdvertisementBlueZ>> advertisements_;

#if BUILDFLAG(IS_CHROMEOS)
  // Timer used to schedule a second update to BlueZ's long term keys. This
  // second update is necessary in a first-time install situation, where field
  // trials might not yet have been available. By scheduling a second update
  // sometime later, the field trials will be guaranteed to be present.
  base::OneShotTimer set_long_term_keys_after_first_time_install_timer_;

  // Pointer for parsing BLE advertising packets out of process.
  mojo::Remote<data_decoder::mojom::BleScanParser> ble_scan_parser_;

  std::unique_ptr<BluetoothAdvertisementMonitorApplicationServiceProvider>
      advertisement_monitor_application_provider_;

  bool is_advertisement_monitor_application_provider_registered_ = false;

  // Used to queue up low energy scan sessions that need to be started as soon
  // as the advertisement monitor application has been registered. The
  // application can only be registered once the adapter has been set, so it is
  // possible for clients to start scan sessions before the monitor application
  // is registered.
  base::queue<std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>>
      pending_advertisement_monitors_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterBlueZ> weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_BLUEZ_H_
