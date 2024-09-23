// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace device {

class BluetoothAdvertisement;
class BluetoothDiscoveryFilter;
class BluetoothDiscoverySession;
#if BUILDFLAG(IS_CHROMEOS)
class BluetoothLowEnergyScanFilter;
#endif  // BUILDFLAG(IS_CHROMEOS)
class BluetoothRemoteGattCharacteristic;
class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;
class BluetoothSocket;
class BluetoothUUID;
enum class UMABluetoothDiscoverySessionOutcome;

// BluetoothAdapter represents a local Bluetooth adapter which may be used to
// interact with remote Bluetooth devices. As well as providing support for
// determining whether an adapter is present and whether the radio is powered,
// this class also provides support for obtaining the list of remote devices
// known to the adapter, discovering new devices, and providing notification of
// updates to device information.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapter
    : public base::RefCounted<BluetoothAdapter> {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  enum class LowEnergyScanSessionHardwareOffloadingStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported
  };
  enum class BluetoothRole { kCentral = 0, kPeripheral, kCentralPeripheral };
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Interface for observing changes from bluetooth adapters.
  class DEVICE_BLUETOOTH_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when the presence of the adapter |adapter| changes. When |present|
    // is true the adapter is now present, false means the adapter has been
    // removed from the system.
    virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                       bool present) {}

    // Called when the radio power state of the adapter |adapter| changes. When
    // |powered| is true the adapter radio is powered, false means the adapter
    // radio is off.
    virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                       bool powered) {}

    // Called when the discoverability state of the  adapter |adapter| changes.
    // When |discoverable| is true the adapter is discoverable by other devices,
    // false means the adapter is not discoverable.
    virtual void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                            bool discoverable) {}

    // Called when the discovering state of the adapter |adapter| changes. When
    // |discovering| is true the adapter is seeking new devices, false means it
    // is not.
    virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                           bool discovering) {}

    // Called when a new device |device| is added to the adapter |adapter|,
    // either because it has been discovered or a connection made. |device|
    // should not be cached. Instead, copy its Bluetooth address.
    virtual void DeviceAdded(BluetoothAdapter* adapter,
                             BluetoothDevice* device) {}

    // Called when the adapter |DiscoveryChangeComplete| is finished
    virtual void DiscoveryChangeCompletedForTesting() {}

    // Called when the result of one of the following methods of the device
    // |device| changes:
    //  * GetAddress()
    //  * GetAppearance()
    //  * GetName() (Chrome OS and Windows only)
    //  * GetBluetoothClass()
    //  * GetInquiryRSSI()
    //  * GetInquiryTxPower()
    //  * GetUUIDs()
    //  * GetServiceData()
    //  * GetServiceDataUUIDs()
    //  * GetServiceDataForUUID()
    //  * GetManufacturerData()
    //  * GetManufacturerDataIDs()
    //  * GetManufacturerDataForID()
    //  * GetAdvertisingDataFlags()
    //  * IsConnectable()
    //  * IsConnected()
    //  * IsConnecting()
    //  * IsGattConnected()
    //  * IsPaired()
    //
    // On Android and MacOS this method is called for each advertisement packet
    // received. On Chrome OS and Linux, we can't guarantee that this method
    // will be called for each Adv. Packet received but, because the RSSI is
    // always changing, it's very likely this method will be called for each
    // Adv. Packet.
    // |device| should not be cached. Instead, copy its Bluetooth address.
    virtual void DeviceChanged(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}

    // Called when address property of the device |device| known to the adapter
    // |adapter| change due to pairing.
    virtual void DeviceAddressChanged(BluetoothAdapter* adapter,
                                      BluetoothDevice* device,
                                      const std::string& old_address) {}

    // Called when advertisement is received.
    //
    // Override this function to observe LE advertisements. This function
    // returns the raw values that have been parsed from EIR.
    virtual void DeviceAdvertisementReceived(
        const std::string& device_address,
        const std::optional<std::string>& device_name,
        const std::optional<std::string>& advertisement_name,
        std::optional<int8_t> rssi,
        std::optional<int8_t> tx_power,
        std::optional<uint16_t> appearance,
        const BluetoothDevice::UUIDList& advertised_uuids,
        const BluetoothDevice::ServiceDataMap& service_data_map,
        const BluetoothDevice::ManufacturerDataMap& manufacturer_data_map) {}

#if BUILDFLAG(IS_CHROMEOS)
    // Called when the bonded property of the device |device| known to the
    // adapter |adapter| changed.
    virtual void DeviceBondedChanged(BluetoothAdapter* adapter,
                                     BluetoothDevice* device,
                                     bool new_bonded_status) {}
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    // Called when the paired property of the device |device| known to the
    // adapter |adapter| changed.
    virtual void DevicePairedChanged(BluetoothAdapter* adapter,
                                     BluetoothDevice* device,
                                     bool new_paired_status) {}

    // Called when the MTU |mtu| (Bluetooth Spec Vol 3, Part F, 3.4.2) used in
    // ATT communication with device |device| known to the adapter |adapter|
    // changed.
    virtual void DeviceMTUChanged(BluetoothAdapter* adapter,
                                  BluetoothDevice* device,
                                  uint16_t mtu) {}

    // Called when advertisement is received from |device|. |eir| is the
    // extended inquiry response specified in Bluetooth Core Spec, Vol 3,
    // Part C, Section 11.
    //
    // Override this function to observe LE advertisements. Whenever |rssi| of
    // |device| changes, this function is called with the latest |eir| from
    // |device|. This function is never called on classic |device|.
    virtual void DeviceAdvertisementReceived(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             int16_t rssi,
                                             const std::vector<uint8_t>& eir) {}

    // Called when |device|'s state has changed from connected to not connected
    // or vice versa.
    virtual void DeviceConnectedStateChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             bool is_now_connected) {}

    // Called when blocked by policy property of the |device| known to the
    // |adapter| changes.
    virtual void DeviceBlockedByPolicyChanged(BluetoothAdapter* adapter,
                                              BluetoothDevice* device,
                                              bool new_blocked_status) {}
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    // Called when the device battery info with |type| has been updated.
    virtual void DeviceBatteryChanged(BluetoothAdapter* adapter,
                                      BluetoothDevice* device,
                                      BluetoothDevice::BatteryType type) {}
#endif

    // Called when the device |device| is removed from the adapter |adapter|,
    // either as a result of a discovered device being lost between discovering
    // phases or pairing information deleted. |device| should not be
    // cached. Instead, copy its Bluetooth address.
    virtual void DeviceRemoved(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}

    // Deprecated GATT Added/Removed Events NOTE:
    //
    // The series of Observer methods for Service, Characteristic, & Descriptor
    // Added/Removed events should be removed.  They are rarely used and add
    // API & implementation complexity.  They are not reliable for cross
    // platform use, and devices that modify their attribute table have not been
    // tested or supported.
    //
    // New code should use Observer::GattServicesDiscovered and then call
    //   GetGattService(s)
    //   GetCharacteristic(s)
    //   GetDescriptor(s)
    //
    // TODO(crbug.com/41312390): Remove Service, Characteristic, & Descriptor
    // Added/Removed.

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when a new GATT service |service| is added to the device |device|,
    // as the service is received from the device. Don't cache |service|. Store
    // its identifier instead (i.e. BluetoothRemoteGattService::GetIdentifier).
    virtual void GattServiceAdded(BluetoothAdapter* adapter,
                                  BluetoothDevice* device,
                                  BluetoothRemoteGattService* service) {}

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when the GATT service |service| is removed from the device
    // |device|. This can happen if the attribute database of the remote device
    // changes or when |device| gets removed.
    virtual void GattServiceRemoved(BluetoothAdapter* adapter,
                                    BluetoothDevice* device,
                                    BluetoothRemoteGattService* service) {}

    // Called when the GATT discovery process has completed for all services,
    // characteristics, and descriptors in |device|.
    virtual void GattServicesDiscovered(BluetoothAdapter* adapter,
                                        BluetoothDevice* device) {}

    // TODO(crbug.com/41354033): Deprecated & not functional on all platforms.
    // Use GattServicesDiscovered.
    //
    // Called when all characteristic and descriptor discovery procedures are
    // known to be completed for the GATT service |service|. This method will be
    // called after the initial discovery of a GATT service and will usually be
    // preceded by calls to GattCharacteristicAdded and GattDescriptorAdded.
    virtual void GattDiscoveryCompleteForService(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattService* service) {}

#if BUILDFLAG(IS_CHROMEOS)
    // Called when the GATT service on the peer side indicates that something is
    // changed on their side, so we need to start re-discovery everything.
    virtual void GattNeedsDiscovery(BluetoothDevice* device) {}
#endif

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when properties of the remote GATT service |service| have changed.
    // This will get called for properties such as UUID, as well as for changes
    // to the list of known characteristics and included services. Observers
    // should read all GATT characteristic and descriptors objects and do any
    // necessary set up required for a changed service.
    virtual void GattServiceChanged(BluetoothAdapter* adapter,
                                    BluetoothRemoteGattService* service) {}

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when the remote GATT characteristic |characteristic| has been
    // discovered. Use this to issue any initial read/write requests to the
    // characteristic but don't cache the pointer as it may become invalid.
    // Instead, use the specially assigned identifier to obtain a characteristic
    // and cache that identifier as necessary, as it can be used to retrieve the
    // characteristic from its GATT service. The number of characteristics with
    // the same UUID belonging to a service depends on the particular profile
    // the remote device implements, hence the client of a GATT based profile
    // will usually operate on the whole set of characteristics and not just
    // one.
    virtual void GattCharacteristicAdded(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattCharacteristic* characteristic) {}

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when a GATT characteristic |characteristic| has been removed from
    // the system.
    virtual void GattCharacteristicRemoved(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattCharacteristic* characteristic) {}

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when the remote GATT characteristic descriptor |descriptor| has
    // been discovered. Don't cache the arguments as the pointers may become
    // invalid. Instead, use the specially assigned identifier to obtain a
    // descriptor and cache that identifier as necessary.
    virtual void GattDescriptorAdded(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattDescriptor* descriptor) {}

    // See "Deprecated GATT Added/Removed Events NOTE" above.
    //
    // Called when a GATT characteristic descriptor |descriptor| has been
    // removed from the system.
    virtual void GattDescriptorRemoved(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattDescriptor* descriptor) {}

    // Called when the value of a characteristic has changed. This might be a
    // result of a read/write request to, or a notification/indication from, a
    // remote GATT characteristic.
    virtual void GattCharacteristicValueChanged(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattCharacteristic* characteristic,
        const std::vector<uint8_t>& value) {}

    // Called when the value of a characteristic descriptor has been updated.
    virtual void GattDescriptorValueChanged(
        BluetoothAdapter* adapter,
        BluetoothRemoteGattDescriptor* descriptor,
        const std::vector<uint8_t>& value) {}

#if BUILDFLAG(IS_CHROMEOS)
    // Called when the low energy scanning hardware offloading support state
    // changes.
    virtual void LowEnergyScanSessionHardwareOffloadingStatusChanged(
        LowEnergyScanSessionHardwareOffloadingStatus status) {}
#endif  // BUILDFLAG(IS_CHROMEOS)
  };

  // Used to configure a listening service.
  struct DEVICE_BLUETOOTH_EXPORT ServiceOptions {
    ServiceOptions();
    ~ServiceOptions();

    std::optional<int> channel;
    std::optional<int> psm;
    std::optional<std::string> name;

    // Clients can configure this option to choose if they want to enforce
    // bonding with remote devices that connect to this device. Options:
    //   * Unset: bonding is not enforced by the local device, and the remote
    //     device can choose if they want to enforce bonding.
    //   * Set to false: bonding is prevented by the local device. Clients which
    //     use this are responsible for securing their communication at the
    //     application level.
    //   * Set to true: bonding is enforced by the local device.
    std::optional<bool> require_authentication;
  };

  enum class DiscoveryState {
    kStarting = 0,
    kStopping,
    kDiscovering,
    kIdle,
  };

  enum class PermissionStatus { kUndetermined = 0, kDenied, kAllowed };

  // The ErrorCallback is used for methods that can fail in which case it is
  // called, in the success case the callback is simply not called.
  using ErrorCallback = base::OnceClosure;

  using DiscoverySessionCallback =
      base::OnceCallback<void(std::unique_ptr<BluetoothDiscoverySession>)>;
  using DeviceList = std::vector<BluetoothDevice*>;
  using ConstDeviceList =
      std::vector<raw_ptr<const BluetoothDevice, VectorExperimental>>;
  using UUIDList = std::vector<BluetoothUUID>;
  using CreateServiceCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothSocket>)>;
  using CreateServiceErrorCallback =
      base::OnceCallback<void(const std::string& message)>;
  using CreateAdvertisementCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothAdvertisement>)>;
  using AdvertisementErrorCallback = BluetoothAdvertisement::ErrorCallback;
  using ConnectDeviceCallback = base::OnceCallback<void(BluetoothDevice*)>;
  using ConnectDeviceErrorCallback =
      base::OnceCallback<void(const std::string& error_message)>;
  using DiscoverySessionErrorCallback =
      base::OnceCallback<void(UMABluetoothDiscoverySessionOutcome)>;
  // The is_error bool is a flag to indicate if the result is an error(true)
  // or a success(false)
  // The Session Outcome is the result which could be success or some sort of
  // error.  However, this variable is ignored when the bool is false
  using DiscoverySessionResultCallback =
      base::OnceCallback<void(/*is_error*/ bool,
                              UMABluetoothDiscoverySessionOutcome)>;
  using RequestSystemPermissionCallback =
      base::OnceCallback<void(BluetoothAdapter::PermissionStatus)>;

  // Creates a new adapter. Initialize() must be called before the adapter can
  // be used.
  static scoped_refptr<BluetoothAdapter> CreateAdapter();

  virtual void Initialize(base::OnceClosure callback) = 0;

  // Returns a weak pointer to an existing adapter for testing purposes only.
  base::WeakPtr<BluetoothAdapter> GetWeakPtrForTesting();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Shutdown the adapter: tear down and clean up all objects owned by
  // BluetoothAdapter. After this call, the BluetoothAdapter will behave as if
  // no Bluetooth controller exists in the local system. |IsPresent| will return
  // false.
  virtual void Shutdown();
#endif

  // Adds and removes observers for events on this bluetooth adapter. If
  // monitoring multiple adapters, check the |adapter| parameter of observer
  // methods to determine which adapter is issuing the event.
  virtual void AddObserver(BluetoothAdapter::Observer* observer);
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer);
  virtual bool HasObserver(BluetoothAdapter::Observer* observer);

  // The address of this adapter. The address format is "XX:XX:XX:XX:XX:XX",
  // where each XX is a hexadecimal number.
  virtual std::string GetAddress() const = 0;

  // The name of the adapter.
  virtual std::string GetName() const = 0;

  // The Bluetooth system name. Implementations may return an informational name
  // "BlueZ 5.54" on Chrome OS.
  virtual std::string GetSystemName() const;

  // Set the human-readable name of the adapter to |name|. On success,
  // |callback| will be called. On failure, |error_callback| will be called.
  // TODO(crbug.com/40145221): Implement a mechanism to request this resource
  // before being able to use it.
  virtual void SetName(const std::string& name,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) = 0;

  // Indicates whether the adapter is initialized and ready to use.
  virtual bool IsInitialized() const = 0;

  // Indicates whether the adapter is actually present on the system. For the
  // default adapter, this indicates whether any adapter is present. An adapter
  // is only considered present if the address has been obtained.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio can be powered. Defaults to
  // IsPresent(). Currently only overridden on Windows, where the adapter can be
  // present, but we might fail to get access to the underlying radio.
  virtual bool CanPower() const;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Returns the status of the browser's Bluetooth permission status.
  virtual PermissionStatus GetOsPermissionStatus() const;

  // Request Bluetooth system permission. For platforms that require Bluetooth
  // system permission for accessing Bluetooth devices, it triggers system
  // permission prompt. `callback` will be invoked when the system permission is
  // determined or `this` is destructed.
  virtual void RequestSystemPermission(
      RequestSystemPermissionCallback callback);

  // Requests a change to the adapter radio power. Setting |powered| to true
  // will turn on the radio and false will turn it off. On success, |callback|
  // will be called. On failure, |error_callback| will be called.
  //
  // The default implementation is meant for platforms that don't have a
  // callback based API. It will store pending callbacks in
  // |set_powered_callbacks_| and invoke SetPoweredImpl(bool) which these
  // platforms need to implement. Pending callbacks are only run when
  // RunPendingPowerCallbacks() is invoked.
  //
  // Platforms that natively support a callback based API (e.g. BlueZ and Win)
  // should override this method and provide their own implementation instead.
  //
  // Due to an issue with non-native APIs on Windows 10, both IsPowered() and
  // SetPowered() don't work correctly when run from a x86 Chrome on a x64 CPU.
  // See https://github.com/Microsoft/cppwinrt/issues/47 for more details.
  virtual void SetPowered(bool powered,
                          base::OnceClosure callback,
                          ErrorCallback error_callback);

  // Indicates whether the adapter support the LowEnergy peripheral role.
  virtual bool IsPeripheralRoleSupported() const;

  // Indicates whether the adapter radio is discoverable.
  virtual bool IsDiscoverable() const = 0;

  // Requests that the adapter change its discoverability state. If
  // |discoverable| is true, then it will be discoverable by other Bluetooth
  // devices. On successfully changing the adapter's discoverability, |callback|
  // will be called. On failure, |error_callback| will be called.
  virtual void SetDiscoverable(bool discoverable,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) = 0;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Gets the current discoverable time for the adapter radio.
  virtual base::TimeDelta GetDiscoverableTimeout() const = 0;
#endif

  // Indicates whether the adapter is currently discovering new devices.
  virtual bool IsDiscovering() const = 0;

  // Inserts all the devices that are connected by the operating system, and not
  // being connected by Chromium, into |devices_|. This method is useful since
  // a discovery session cannot find devices that are already connected to the
  // computer.
  // TODO(crbug.com/40487754): Needs to be implemented for Android and Windows.
  virtual std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const BluetoothDiscoveryFilter& discovery_filter);

  // Requests the adapter to start a new discovery session. On success, a new
  // instance of BluetoothDiscoverySession will be returned to the caller via
  // |callback| and the adapter will be discovering nearby Bluetooth devices.
  // The returned BluetoothDiscoverySession is owned by the caller and it's the
  // owner's responsibility to properly clean it up and stop the session when
  // device discovery is no longer needed.
  //
  // If clients desire device discovery to run, they should always call this
  // method and never make it conditional on the value of IsDiscovering(), as
  // another client might cause discovery to stop unexpectedly. Hence, clients
  // should always obtain a BluetoothDiscoverySession and call
  // BluetoothDiscoverySession::Stop when done. When this method gets called,
  // device discovery may actually be in progress. Clients can call GetDevices()
  // and check for those with IsPaired() as false to obtain the list of devices
  // that have been discovered so far. Otherwise, clients can be notified of all
  // new and lost devices by implementing the Observer methods "DeviceAdded" and
  // "DeviceRemoved".
  //
  // |client_name|: The name of the application using this scan session. This
  // field is for logging purposes and does not affect any scanning logic, so
  // the value can be freely defined by the caller.
  void StartDiscoverySession(const std::string& client_name,
                             DiscoverySessionCallback callback,
                             ErrorCallback error_callback);
  void StartDiscoverySessionWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const std::string& client_name,
      DiscoverySessionCallback callback,
      ErrorCallback error_callback);

  // Return all discovery filters assigned to this adapter merged together.
  std::unique_ptr<BluetoothDiscoveryFilter> GetMergedDiscoveryFilter() const;

  // Requests the list of devices from the adapter. All devices are returned,
  // including those currently connected, those paired and all devices returned
  // by RetrieveGattConnectedDevicesWithDiscoveryFilter() (from the previous
  // call). Use the returned device pointers to determine which they are.
  virtual DeviceList GetDevices();
  virtual ConstDeviceList GetDevices() const;

  // Returns a pointer to the device with the given address |address| or NULL if
  // no such device is known.
  virtual BluetoothDevice* GetDevice(const std::string& address);
  virtual const BluetoothDevice* GetDevice(const std::string& address) const;

  // Returns a list of UUIDs for services registered on this adapter.
  // This may include UUIDs from standard profiles (e.g. A2DP) as well
  // as those created by CreateRfcommService and CreateL2capService.
  virtual UUIDList GetUUIDs() const = 0;

  // Possible priorities for AddPairingDelegate(), low is intended for
  // permanent UI and high is intended for interactive UI or applications.
  enum PairingDelegatePriority {
    PAIRING_DELEGATE_PRIORITY_LOW,
    PAIRING_DELEGATE_PRIORITY_HIGH
  };

  // Adds a default pairing delegate with priority |priority|. Method calls
  // will be made on |pairing_delegate| for incoming pairing requests if the
  // priority is higher than any other registered; or for those of the same
  // priority, the first registered.
  //
  // |pairing_delegate| must not be freed without first calling
  // RemovePairingDelegate().
  virtual void AddPairingDelegate(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      PairingDelegatePriority priority);

  // Removes a previously added pairing delegate.
  virtual void RemovePairingDelegate(
      BluetoothDevice::PairingDelegate* pairing_delegate);

  // Returns the first registered pairing delegate with the highest priority,
  // or NULL if no delegate is registered. Used to select the delegate for
  // incoming pairing requests.
  virtual BluetoothDevice::PairingDelegate* DefaultPairingDelegate();

  // Creates an RFCOMM service on this adapter advertised with UUID |uuid|,
  // listening on channel |options.channel|, which may be left null to
  // automatically allocate one. The service will be advertised with
  // |options.name| as the English name of the service. |callback| will be
  // called on success with a BluetoothSocket instance that is to be owned by
  // the received.  |error_callback| will be called on failure with a message
  // indicating the cause.
  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      CreateServiceCallback callback,
      CreateServiceErrorCallback error_callback) = 0;

  // Creates an L2CAP service on this adapter advertised with UUID |uuid|,
  // listening on PSM |options.psm|, which may be left null to automatically
  // allocate one. The service will be advertised with |options.name| as the
  // English name of the service. |callback| will be called on success with a
  // BluetoothSocket instance that is to be owned by the received.
  // |error_callback| will be called on failure with a message indicating the
  // cause.
  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      CreateServiceCallback callback,
      CreateServiceErrorCallback error_callback) = 0;

  // Creates and registers an advertisement for broadcast over the LE channel.
  // The created advertisement will be returned via the success callback. An
  // advertisement can unregister itself at any time by calling its unregister
  // function.
  virtual void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) = 0;

  // Indicates whether LE extended advertising is supported.
  virtual bool IsExtendedAdvertisementsAvailable() const;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Sets the interval between two consecutive advertisements. Valid ranges
  // for the interval are from 20ms to 10.24 seconds, with min <= max.
  // Note: This is a best effort. The actual interval may vary non-trivially
  // from the requested intervals. On some hardware, there is a minimum
  // interval of 100ms. The minimum and maximum values are specified by the
  // Core 4.2 Spec, Vol 2, Part E, Section 7.8.5.
  virtual void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) = 0;

  // Resets advertising on this adapter. This will unregister all existing
  // advertisements and will stop advertising them.
  virtual void ResetAdvertising(base::OnceClosure callback,
                                AdvertisementErrorCallback error_callback) = 0;

  // Connect to a device with |address| that is either undiscovered or not
  // previously paired or connected. Callers are responsible for ensuring that
  // the device with |address| is available and nearby via their own out-of-band
  // mechanism, and should not call this method if GetDevice(address) returns
  // a valid reference (in which case this method will fail).
  virtual void ConnectDevice(
      const std::string& address,
      const std::optional<BluetoothDevice::AddressType>& address_type,
      ConnectDeviceCallback callback,
      ConnectDeviceErrorCallback error_callback) = 0;
#endif

  // Returns the list of pending advertisements that are not registered yet.
  virtual std::vector<BluetoothAdvertisement*>
  GetPendingAdvertisementsForTesting() const;

  // Returns the local GATT services associated with this adapter with the
  // given identifier. Returns NULL if the service doesn't exist.
  virtual BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const = 0;

  // Creates a GATT services associated with this adapter with the
  // given identifier. Currently only derived and implemented on BlueZ and
  // Floss. All other platforms use default behavior of returning nullptr.
  virtual base::WeakPtr<BluetoothLocalGattService> CreateLocalGattService(
      const BluetoothUUID& uuid,
      bool is_primary,
      BluetoothLocalGattService::Delegate* delegate);

  // The following methods are used to send various events to observers.
  void NotifyAdapterPresentChanged(bool present);
  void NotifyAdapterPoweredChanged(bool powered);
  void NotifyDeviceChanged(BluetoothDevice* device);
  void NotifyAdapterDiscoveryChangeCompletedForTesting();

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void NotifyDevicePairedChanged(BluetoothDevice* device,
                                 bool new_paired_status);
  void NotifyDeviceConnectedStateChanged(BluetoothDevice* device,
                                         bool is_connected);
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void NotifyDeviceBatteryChanged(BluetoothDevice* device,
                                  BluetoothDevice::BatteryType type);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  void NotifyDeviceBondedChanged(BluetoothDevice* device,
                                 bool new_bonded_status);
  void NotifyDeviceIsBlockedByPolicyChanged(BluetoothDevice* device,
                                            bool new_blocked_status);
  void NotifyGattNeedsDiscovery(BluetoothDevice* device);
#endif

  void NotifyGattServiceAdded(BluetoothRemoteGattService* service);
  void NotifyGattServiceRemoved(BluetoothRemoteGattService* service);
  void NotifyGattServiceChanged(BluetoothRemoteGattService* service);
  void NotifyGattServicesDiscovered(BluetoothDevice* device);
  void NotifyGattDiscoveryComplete(BluetoothRemoteGattService* service);
  void NotifyGattCharacteristicAdded(
      BluetoothRemoteGattCharacteristic* characteristic);
  void NotifyGattCharacteristicRemoved(
      BluetoothRemoteGattCharacteristic* characteristic);
  void NotifyGattDescriptorAdded(BluetoothRemoteGattDescriptor* descriptor);
  void NotifyGattDescriptorRemoved(BluetoothRemoteGattDescriptor* descriptor);
  void NotifyGattCharacteristicValueChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value);
  void NotifyGattDescriptorValueChanged(
      BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value);

#if BUILDFLAG(IS_CHROMEOS)
  void NotifyLowEnergyScanSessionHardwareOffloadingStatusChanged(
      LowEnergyScanSessionHardwareOffloadingStatus status);

  // Set a service allowlist by specifying services UUIDs. When this is called,
  // existing connections will be disconnected and services not in the allowlist
  // will be blocked. Device property |IsBlockedByPolicy| will be True if some
  // of the auto-connect services are blocked, False otherwise.
  virtual void SetServiceAllowList(const UUIDList& uuids,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback) = 0;

  // Returns |kSupported| if the device supports the offloading of filtering and
  // other scanning logic to the Bluetooth hardware. This brings the benefit of
  // reduced power consumption for BluetoothLowEnergyScanSession. Returns
  // |kNotSupported| if hardware offloading is not available, in which case
  // BluetoothLowEnergyScanSession will operate with higher power
  // consumption. |kUndetermined| indicates the status can not currently be
  // determined (such as when the adapter is not present), and the client should
  // retry.
  //
  // Consumers should check this value before
  // creating a BluetoothLowEnergyScanSession and consider ways to mitigate
  // power usage, especially if the scan session is intended to be long-running.
  virtual LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() = 0;

  // Starts a low energy scanning session that will notify the client on session
  // started, session invalidated, device found and device lost events via the
  // |delegate|.
  //
  // The client controls the lifetime of the session (except on unexpected
  // invalidation, see below). The client ends a scan session by destroying the
  // returned instance.
  //
  // A session cannot recover once the
  // BluetoothLowEnergyScanSession::Delegate::OnSessionInvalidated() callback
  // has been invoked. Invalidation can happen if the platform unexpectedly
  // cleans up the scan session due to a firmware crash, etc.. If a client wants
  // an identical scanning session, it should discard its newly invalidated
  // BluetoothLowEnergyScanSession and create a new one by calling
  // StartLowEnergyScanSession() again.
  virtual std::unique_ptr<BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<BluetoothLowEnergyScanSession::Delegate> delegate) = 0;

  // Returns a list of all the roles that are supported by the adapter.
  virtual std::vector<BluetoothRole> GetSupportedRoles() = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set the adapter name to one chosen from the system information. Only Ash
  // needs to do this.
  virtual void SetStandardChromeOSAdapterName() = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // The timeout in seconds used by RemoveTimedOutDevices.
  static const base::TimeDelta timeoutSec;

  // This struct is meant to hold any possible callback from a discovery
  // request. The purpose of this is to consolidate all discovery request
  // callbacks into one array that can be handled all at once when the state
  // desired from all requests is achieved or an error is thrown.
  struct StartOrStopDiscoveryCallback {
    StartOrStopDiscoveryCallback(base::OnceClosure start_callback,
                                 ErrorCallback start_error_callback);
    StartOrStopDiscoveryCallback(
        base::OnceClosure stop_callback,
        DiscoverySessionErrorCallback stop_error_callback);
    ~StartOrStopDiscoveryCallback();

    // The success callback for a start discovery request.
    base::OnceClosure start_callback;
    // The success callback for a stop discovery request.
    base::OnceClosure stop_callback;
    // The error callback for a start discovery request.
    ErrorCallback start_error_callback;
    // The error callback for a stop discovery request.
    DiscoverySessionErrorCallback stop_error_callback;
  };

 protected:
  friend class base::RefCounted<BluetoothAdapter>;
  friend class BluetoothAdapterFactory;
  friend class BluetoothDiscoverySession;
  friend class BluetoothTestBase;

  using DevicesMap =
      std::unordered_map<std::string, std::unique_ptr<BluetoothDevice>>;
  using PairingDelegatePair =
      std::pair<BluetoothDevice::PairingDelegate*, PairingDelegatePriority>;

  using CallbackQueue =
      base::queue<std::unique_ptr<StartOrStopDiscoveryCallback>>;

  // Implementations on Android and macOS need to store pending SetPowered()
  // callbacks until an appropriate event is received, due to a lack of blocking
  // or callback supporting platform APIs. Declaring the struct here allows
  // Android and macOS to share the implementation.
  struct SetPoweredCallbacks {
    SetPoweredCallbacks();
    ~SetPoweredCallbacks();

    bool powered = false;
    base::OnceClosure callback;
    ErrorCallback error_callback;
  };

  BluetoothAdapter();
  virtual ~BluetoothAdapter();

  virtual base::WeakPtr<BluetoothAdapter> GetWeakPtr() = 0;

  // This method calls into platform specific logic on macOS and Android where
  // pending SetPowered() callbacks need to be stored explicitly.
  virtual bool SetPoweredImpl(bool powered) = 0;

  // Called by macOS, Android and WinRT once the specific powered state events
  // are received or an error occurred. Clears out pending callbacks.
  void RunPendingPowerCallbacks();

  // Internal methods for initiating and terminating device discovery sessions.
  // An implementation of BluetoothAdapter keeps an internal reference count to
  // make sure that the underlying controller is constantly searching for nearby
  // devices and retrieving information from them as long as there are clients
  // who have requested discovery. These methods behave in the following way:
  //
  // On a call to StartScanWithFilter:
  //    - This should only be called when we get the first request to start the
  //      scan with only the initial filter in that request.
  //    - This function should do the OS specific things to get the filter
  //      started.
  //    - When finished it should callback with success or the appropriate
  //    Output for an error.
  // On a call to UpdateFilter:
  //    - The scan should already be started or at least starting.
  //    - This function takes in a filter and should do all the OS specifics
  //      needed to update the scan with the new filter.
  //    - When finished it should call the callback with success or the
  //      appropriate output for an error
  //
  // On a call to StopScan:
  //    - Make a request to the physical adapter that we no longer needs to
  //      be scanning
  //    - When finished it should callback with success.  If an error is thrown
  //      we still return success to the user and update our internal state to
  //      say that we are not discovering.

  virtual void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) = 0;
  virtual void UpdateFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) = 0;
  virtual void StopScan(DiscoverySessionResultCallback callback) = 0;

  // Removes the |discovery_session| from |discovery_sessions_| and updates
  // accordingly
  void RemoveDiscoverySession(BluetoothDiscoverySession* discovery_session,
                              base::OnceClosure callback,
                              DiscoverySessionErrorCallback error_callback);
  // Helper function that short circuits a successful callback if the filter is
  // the same as the current filter.
  void MaybeUpdateFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback);

  // Called by RemovePairingDelegate() in order to perform any class-specific
  // internal functionality necessary to remove the pairing delegate, such as
  // cleaning up ongoing pairings using it.
  virtual void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) = 0;

  // Marks all known DiscoverySession instances as inactive. Called by
  // BluetoothAdapter in the event that the adapter unexpectedly stops
  // discovering. This should be called by all platform implementations.
  void MarkDiscoverySessionsAsInactive();

  void DeleteDeviceForTesting(const std::string& address);

  // Removes from |devices_| any previously paired, connected or seen
  // devices which are no longer present. Notifies observers. Note:
  // this is only used by platforms where there is no notification of
  // lost devices.
  void RemoveTimedOutDevices();

  int NumDiscoverySessions() const;

  // Number of DiscoverySessions with the status of SCANNING.
  int NumScanningDiscoverySessions() const;

  // Clear `devices_` and send device removed event for each one of them.
  void ClearAllDevices();

  // UI thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Observers of BluetoothAdapter, notified from implementation subclasses.
  base::ObserverList<device::BluetoothAdapter::Observer>::
      UncheckedAndDanglingUntriaged observers_;

  // Devices paired with, connected to, discovered by, or visible to the
  // adapter. The key is the Bluetooth address of the device and the value is
  // the BluetoothDevice object whose lifetime is managed by the adapter
  // instance.
  DevicesMap devices_;

  // Default pairing delegates registered with the adapter.
  std::list<PairingDelegatePair> pairing_delegates_;

  // SetPowered() callbacks, only relevant for macOS and Android.
  std::unique_ptr<SetPoweredCallbacks> set_powered_callbacks_;

  // List of active DiscoverySession objects. This is used to notify sessions to
  // become inactive in case of an unexpected change to the adapter discovery
  // state. We keep raw pointers, with the invariant that a DiscoverySession
  // will remove itself from this list when it gets destroyed or becomes
  // inactive by calling DiscoverySessionBecameInactive(), hence no pointers to
  // deallocated sessions are kept.
  std::set<raw_ptr<BluetoothDiscoverySession, SetExperimental>>
      discovery_sessions_;

 private:
  // This is the callback for all OS level calls to StartScanWithFilter,
  // UpdateFilter, and StopScan.  It updates the state accordingly, calls all
  // appropriate callbacks, and calls ProcessDiscoveryQueue().
  void OnDiscoveryChangeComplete(bool is_error,
                                 UMABluetoothDiscoverySessionOutcome outcome);

  // This method processes all queued requests that have been waiting for a
  // process to finish.
  void ProcessDiscoveryQueue();

  // Utility method used to call all callbacks in the case of an error in a
  // process
  void NotifyDiscoveryError(CallbackQueue queue);

  // Utility function to update our internal state after a process has
  // completed(example: kStarting -> kDiscovering)
  void UpdateDiscoveryState(bool is_error);

  // List of callbacks for requests that have been queued up and are awaiting a
  // process to finish before they can begin the request
  CallbackQueue discovery_callback_queue_;
  // List of callbacks whose requests are currently being processed by the OS
  // level adapter
  CallbackQueue callbacks_awaiting_response_;

  // Discovery filter currently being used by the adapter
  device::BluetoothDiscoveryFilter current_discovery_filter_;
  // Discovery filter that is about to be set in the OS level adapter.  After
  // the process that is implementing this feature is finished this will become
  // the |current_discovery_filter_|.
  device::BluetoothDiscoveryFilter filter_being_set_;

  // True, if there is a pending request to start or stop discovery.
  bool discovery_request_pending_ = false;

  // enum used to track our internal discovery state.
  DiscoveryState internal_discovery_state_ = DiscoveryState::kIdle;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
