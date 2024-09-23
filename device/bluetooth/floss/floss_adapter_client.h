// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_sdp_types.h"

namespace dbus {
class ErrorResponse;
class ObjectPath;
class Response;
}  // namespace dbus

namespace floss {

// The adapter client represents a specific adapter and exposes some common
// functionality on it (such as discovery and bonding). It is managed by
// FlossClientBundle and will be initialized only when the chosen adapter is
// powered on (presence and power management is done by |FlossManagerClient|).
class DEVICE_BLUETOOTH_EXPORT FlossAdapterClient : public FlossDBusClient {
 public:
  enum class BluetoothDeviceType {
    kUnknown = 0,
    kBredr = 1,
    kBle = 2,
    kDual = 3,
  };

  enum class BluetoothSspVariant {
    kPasskeyConfirmation = 0,
    kPasskeyEntry = 1,
    kConsent = 2,
    kPasskeyNotification = 3,
  };

  enum class BondState {
    kNotBonded = 0,
    kBondingInProgress = 1,
    kBonded = 2,
  };

  enum class ConnectionState {
    kDisconnected = 0,
    kConnectedOnly = 1,
    kPairedBREDROnly = 3,
    kPairedLEOnly = 5,
    kPairedBoth = 7,
  };

  enum class BtPropertyType {
    kBdName = 0x1,
    kBdAddr,
    kUuids,
    kClassOfDevice,
    kTypeOfDevice,
    kServiceRecord,
    kAdapterScanMode,
    kAdapterBondedDevices,
    kAdapterDiscoverableTimeout,
    kRemoteFriendlyName,
    kRemoteRssi,
    kRemoteVersionInfo,
    kLocalLeFeatures,
    kLocalIoCaps,
    kLocalIoCapsBle,
    kDynamicAudioBuffer,
    kRemoteIsCoordinatedSetMember,
    kAppearance,
    kVendorProductInfo,
    // Unimplemented:
    //  BT_PROPERTY_WL_MEDIA_PLAYERS_LIST,
    //  BT_PROPERTY_REMOTE_ASHA_CAPABILITY,
    //  BT_PROPERTY_REMOTE_ASHA_TRUNCATED_HISYNCID,
    //  BT_PROPERTY_REMOTE_MODEL_NUM,
    kRemoteAddrType = 0x18,

    kUnknown = 0xFE,
    kRemoteDeviceTimestamp = 0xFF,
  };

  enum class BtDiscoverableMode : uint32_t {
    kNonDiscoverable = 0,
    kLimitedDiscoverable = 1,
    kGeneralDiscoverable = 2,
  };

  enum class BtAddressType {
    kPublic = 0,
    kRandom,
    kPublicId,
    kRandomId,
    kUnknown = 0xfe,
    kAnonymous = 0xff,
  };

  enum class BtAdapterRole {
    kCentral = 0,
    kPeripheral,
    kCentralPeripheral,
  };

  struct VendorProductInfo {
    uint8_t vendorIdSrc;
    uint16_t vendorId;
    uint16_t productId;
    uint16_t version;
    VendorProductInfo()
        : vendorIdSrc(
              device::BluetoothDevice::VendorIDSource::VENDOR_ID_UNKNOWN),
          vendorId(),
          productId(),
          version() {}
  };

  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    Observer() = default;
    ~Observer() override = default;

    // Notification sent when the adapter address has changed.
    virtual void AdapterAddressChanged(const std::string& address) {}

    // Notification sent when the adapter address has changed.
    virtual void DiscoverableChanged(bool discoverable) {}

    // Notification sent when the discovering state has changed.
    virtual void AdapterDiscoveringChanged(bool state) {}

    // Notification sent when discovery has found a device. This notification
    // is not guaranteed to be unique per Chrome discovery session (i.e. you can
    // get the same device twice).
    virtual void AdapterFoundDevice(const FlossDeviceId& device_found) {}

    // Notification sent when a found device is cleared. It will be sent when
    // a device found during discovery is determined to be stale (was last seen
    // some amount of time ago).
    virtual void AdapterClearedDevice(const FlossDeviceId& device_cleared) {}

    // Notification sent when a device property has changed.
    virtual void AdapterDevicePropertyChanged(BtPropertyType prop_type,
                                              const FlossDeviceId& device) {}

    // Notification sent for Simple Secure Pairing.
    virtual void AdapterSspRequest(const FlossDeviceId& remote_device,
                                   uint32_t cod,
                                   BluetoothSspVariant variant,
                                   uint32_t passkey) {}

    // Notification sent for legacy pairing to display auto-gen pin code.
    virtual void AdapterPinDisplay(const FlossDeviceId& remote_device,
                                   std::string pincode) {}

    // Notification sent for legacy pairing to ask user input pin code.
    virtual void AdapterPinRequest(const FlossDeviceId& remote_device,
                                   uint32_t cod,
                                   bool min_16_digit) {}

    // Notification sent when a bonding state changes for a remote device.
    // TODO(b:202334519): Change status type to enum once Floss has the enum.
    virtual void DeviceBondStateChanged(const FlossDeviceId& remote_device,
                                        uint32_t status,
                                        BondState bond_state) {}

    // Notification sent when a remote device becomes connected.
    virtual void AdapterDeviceConnected(const FlossDeviceId& device) {}

    // Notification sent when a remote device becomes disconnected.
    virtual void AdapterDeviceDisconnected(const FlossDeviceId& device) {}

    // Notification sent when requested SDP search has completed.
    virtual void SdpSearchComplete(const FlossDeviceId device,
                                   const device::BluetoothUUID uuid,
                                   const std::vector<BtSdpRecord> records) {}

    // Notification sent when an SDP record has finished being created and
    // assigned a handle.
    virtual void SdpRecordCreated(const BtSdpRecord record,
                                  const int32_t handle) {}
  };

  // Error: No such adapter.
  static const char kErrorUnknownAdapter[];

  // Creates the instance.
  static std::unique_ptr<FlossAdapterClient> Create();

  // Checks if a connection state indicates that it is paired.
  static bool IsConnectionPaired(uint32_t connection_state);

  FlossAdapterClient(const FlossAdapterClient&) = delete;
  FlossAdapterClient& operator=(const FlossAdapterClient&) = delete;

  FlossAdapterClient();
  ~FlossAdapterClient() override;

  // Manage observers.
  void AddObserver(Observer* observer);
  bool HasObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the address of this adapter.
  const std::string& GetAddress() const { return property_address_.Get(); }

  // Get the name of this adapter.
  const std::string& GetName() const { return property_name_.Get(); }

  // Set the name of this adapter.
  virtual void SetName(ResponseCallback<Void> callback,
                       const std::string& name);

  // Get whether adapter is discoverable.
  bool GetDiscoverable() const { return property_discoverable_.Get(); }

  // Set whether adapter is discoverable.
  virtual void SetDiscoverable(ResponseCallback<Void> callback,
                               bool discoverable);

  // Set adapter discoverability mode.
  virtual void SetDiscoverable(ResponseCallback<Void> callback,
                               BtDiscoverableMode mode,
                               uint32_t duration);

  // Get the discoverable timeout for the adapter. Updates whenever the
  // discoverable state changes.
  uint32_t GetDiscoverableTimeout() const { return discoverable_timeout_; }

  // Indicates if LE extended advertising is supported.
  bool IsExtAdvSupported() const { return property_ext_adv_supported_.Get(); }

  // Start a discovery session.
  virtual void StartDiscovery(ResponseCallback<Void> callback);

  // Cancel the active discovery session.
  virtual void CancelDiscovery(ResponseCallback<Void> callback);

  // Create a bond with the given device and transport.
  virtual void CreateBond(ResponseCallback<bool> callback,
                          FlossDeviceId device,
                          BluetoothTransport transport);

  // Create a bond with the given device and transport. API version >= 0.4.0,
  // add callback status.
  virtual void CreateBond(
      ResponseCallback<FlossDBusClient::BtifStatus> callback,
      FlossDeviceId device,
      BluetoothTransport transport);

  // Cancel a bond process.
  virtual void CancelBondProcess(ResponseCallback<bool> callback,
                                 FlossDeviceId device);

  // Removes bonding.
  virtual void RemoveBond(ResponseCallback<bool> callback,
                          FlossDeviceId device);

  // Gets the transport type of the device.
  virtual void GetRemoteType(ResponseCallback<BluetoothDeviceType> callback,
                             FlossDeviceId device);

  // Gets class of a device.
  virtual void GetRemoteClass(ResponseCallback<uint32_t> callback,
                              FlossDeviceId device);

  // Gets appearance of a device.
  virtual void GetRemoteAppearance(ResponseCallback<uint16_t> callback,
                                   FlossDeviceId device);

  // Get connection state of a device.
  // TODO(b/202334519): Change return type to enum instead of u32
  virtual void GetConnectionState(ResponseCallback<uint32_t> callback,
                                  const FlossDeviceId& device);

  // Gets UUIDs of a device.
  virtual void GetRemoteUuids(
      ResponseCallback<device::BluetoothDevice::UUIDList> callback,
      FlossDeviceId device);

  // Triggers SDP to fetch UUIDs of a device.
  virtual void FetchRemoteUuids(ResponseCallback<bool> callback,
                                FlossDeviceId device);

  // Gets the Vendor and Product Id of a device
  virtual void GetRemoteVendorProductInfo(
      ResponseCallback<VendorProductInfo> callback,
      FlossDeviceId device);

  // Gets the address type of a device
  virtual void GetRemoteAddressType(ResponseCallback<BtAddressType> callback,
                                    FlossDeviceId device);

  // Get bonding state of a device.
  virtual void GetBondState(ResponseCallback<uint32_t> callback,
                            const FlossDeviceId& device);

  // Connect to all enabled profiles.
  virtual void ConnectAllEnabledProfiles(ResponseCallback<Void> callback,
                                         const FlossDeviceId& device);

  // Connect to all enabled profiles. API version >= 0.4.0, add callback status.
  virtual void ConnectAllEnabledProfiles(
      ResponseCallback<FlossDBusClient::BtifStatus> callback,
      const FlossDeviceId& device);

  // Disconnect all enabled profiles.
  virtual void DisconnectAllEnabledProfiles(ResponseCallback<Void> callback,
                                            const FlossDeviceId& device);

  // Indicates whether the user approves the pairing, if accepted then a pairing
  // should be completed on the remote device.
  virtual void SetPairingConfirmation(ResponseCallback<Void> callback,
                                      const FlossDeviceId& device,
                                      bool accept);

  // Indicates whether the user approves the pairing with the given pin.
  virtual void SetPin(ResponseCallback<Void> callback,
                      const FlossDeviceId& device,
                      bool accept,
                      const std::vector<uint8_t>& pin);

  // Indicates whether the user approves the pairing with the given passkey.
  virtual void SetPasskey(ResponseCallback<Void> callback,
                          const FlossDeviceId& device,
                          bool accept,
                          const std::vector<uint8_t>& passkey);

  // Returns bonded devices.
  virtual void GetBondedDevices();

  // Returns connected devices.
  virtual void GetConnectedDevices();

  // Initiate an SDP search on device for uuid. Search results provided through
  // |OnSdpSearchComplete|.
  virtual void SdpSearch(ResponseCallback<bool> callback,
                         const FlossDeviceId& device,
                         device::BluetoothUUID uuid);

  // Create a new SDP record in this device's SDP server. Record handle is
  // returned via |SdpRecordCreated|.
  virtual void CreateSdpRecord(ResponseCallback<bool> callback,
                               const BtSdpRecord& record);

  // Remove an SDP record by its record handle.
  virtual void RemoveSdpRecord(ResponseCallback<bool> callback,
                               const int32_t& handle);

  std::vector<BtAdapterRole> GetSupportedRoles() {
    return property_roles_.Get();
  }

  // Get the object path for this adapter.
  const dbus::ObjectPath* GetObjectPath() const { return &adapter_path_; }

  // Initialize the adapter client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  friend class FlossAdapterClientTest;

  // Handle callback |OnAdapterPropertyChanged| on exported object path.
  void OnAdapterPropertyChanged(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // When address property is updated.
  void OnAddressChanged(const std::string& address);

  // When name property is updated.
  void OnNameChanged(const std::string& name);

  // When discoverable property is updated.
  void OnDiscoverableChanged(const bool& discoverable);

  // Handle callback |OnDiscoveringChanged| on exported object path.
  void OnDiscoveringChanged(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDeviceFound| on exported object path.
  void OnDeviceFound(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDeviceCleared| on exported object path.
  void OnDeviceCleared(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDevicePropertiesChanged| on exported object path.
  void OnDevicePropertiesChanged(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnSspRequest| on exported object path.
  void OnSspRequest(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnPinDisplay| on exported object path.
  void OnPinDisplay(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnPinRequest| on exported object path.
  void OnPinRequest(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnBondStateChanged| on exported object path.
  void OnBondStateChanged(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnSdpSearchComplete| on exported object path.
  void OnSdpSearchComplete(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnSdpRecordCreated| on exported object path.
  void OnSdpRecordCreated(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDeviceConnected| on exported object path.
  void OnDeviceConnected(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDeviceDisconnected| on exported object path.
  void OnDeviceDisconnected(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle GetBondedDevices.
  void OnGetBondedDevices(DBusResult<std::vector<FlossDeviceId>> ret);

  // Handle GetConnectedDevices.
  void OnGetConnectedDevices(DBusResult<std::vector<FlossDeviceId>> ret);

  // Calls GetDiscoverableTimeout.
  void UpdateDiscoverableTimeout();

  // Handle GetDiscoverableTimeout and cache the returned value.
  void OnDiscoverableTimeout(DBusResult<uint32_t> ret);

  // Handle |RegisterCallback| result and store callback ID.
  void OnRegisterCallback(DBusResult<uint32_t> ret);

  // Handle |RegisterConnectionCallback| result and store callback ID.
  void OnRegisterConnectionCallback(DBusResult<uint32_t> ret);

  // Handle both |UnregisterCallback| and |UnregisterConnectionCallback|.
  void OnUnregisterCallbacks(DBusResult<bool> ret);

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Adapter managed by this client.
  dbus::ObjectPath adapter_path_;

  // Service which implements the adapter interface.
  std::string service_name_;

  // Object path for exported callbacks registered to this client.
  std::string exported_callback_path_;

  // Cached discoverable timeout value (updates on init and on discoverable
  // state changes).
  uint32_t discoverable_timeout_ = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(FlossAdapterClientTest, CallAdapterMethods);

  template <typename R, typename... Args>
  void CallAdapterMethod(ResponseCallback<R> callback,
                         const char* member,
                         Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kAdapterInterface,
               adapter_path_, member, args...);
  }

  FlossProperty<std::string> property_address_{
      kAdapterInterface, adapter::kCallbackInterface, adapter::kGetAddress,
      adapter::kOnAddressChanged};

  FlossProperty<std::string> property_name_{
      kAdapterInterface, adapter::kCallbackInterface, adapter::kGetName,
      adapter::kOnNameChanged};

  FlossProperty<bool> property_discoverable_{
      kAdapterInterface, adapter::kCallbackInterface, adapter::kGetDiscoverable,
      adapter::kOnDiscoverableChanged};

  FlossProperty<bool> property_ext_adv_supported_{
      kAdapterInterface, adapter::kCallbackInterface,
      adapter::kIsLeExtendedAdvertisingSupported, nullptr};

  FlossProperty<std::vector<BtAdapterRole>> property_roles_{
      kAdapterInterface, adapter::kCallbackInterface,
      adapter::kGetSupportedRoles, nullptr};

  // Object path for exported callbacks registered against adapter interface.
  static const char kExportedCallbacksPath[];

  // Signal when client is ready to be used.
  base::OnceClosure on_ready_;

  // Number of callbacks pending registration before client is ready to use.
  int pending_register_calls_ = 0;

  // Callback ID used for callbacks registered to this client.
  std::optional<uint32_t> callback_id_;

  // Callback ID used for connection callbacks registered to this client.
  std::optional<uint32_t> connection_callback_id_;

  base::WeakPtrFactory<FlossAdapterClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_
