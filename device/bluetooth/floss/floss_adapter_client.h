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
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

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

    kUnknown = 0xFE,
    kRemoteDeviceTimestamp = 0xFF,
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

    // Notification sent for Simple Secure Pairing.
    virtual void AdapterSspRequest(const FlossDeviceId& remote_device,
                                   uint32_t cod,
                                   BluetoothSspVariant variant,
                                   uint32_t passkey) {}

    // Notification sent when a bonding state changes for a remote device.
    // TODO(b:202334519): Change status type to enum once Floss has the enum.
    virtual void DeviceBondStateChanged(const FlossDeviceId& remote_device,
                                        uint32_t status,
                                        BondState bond_state) {}

    // Notification sent when a remote device becomes connected.
    virtual void AdapterDeviceConnected(const FlossDeviceId& device) {}

    // Notification sent when a remote device becomes disconnected.
    virtual void AdapterDeviceDisconnected(const FlossDeviceId& device) {}
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

  // Start a discovery session.
  virtual void StartDiscovery(ResponseCallback<Void> callback);

  // Cancel the active discovery session.
  virtual void CancelDiscovery(ResponseCallback<Void> callback);

  // Create a bond with the given device and transport.
  virtual void CreateBond(ResponseCallback<bool> callback,
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

  // Get bonding state of a device.
  virtual void GetBondState(ResponseCallback<uint32_t> callback,
                            const FlossDeviceId& device);

  // Connect to all enabled profiles.
  virtual void ConnectAllEnabledProfiles(ResponseCallback<Void> callback,
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

  // Get the object path for this adapter.
  const dbus::ObjectPath* GetObjectPath() const { return &adapter_path_; }

  // Initialize the adapter client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;

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

  // Handle callback |OnSspRequest| on exported object path.
  void OnSspRequest(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnBondStateChanged| on exported object path.
  void OnBondStateChanged(dbus::MethodCall* method_call,
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

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Adapter managed by this client.
  dbus::ObjectPath adapter_path_;

  // Service which implements the adapter interface.
  std::string service_name_;

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

  // Object path for exported callbacks registered against adapter interface.
  static const char kExportedCallbacksPath[];

  base::WeakPtrFactory<FlossAdapterClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_
