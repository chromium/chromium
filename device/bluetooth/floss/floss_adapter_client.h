// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace dbus {
class ErrorResponse;
class MessageReader;
class MessageWriter;
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
  enum class BluetoothTransport {
    kAuto = 0,
    kBrEdr = 1,
    kLe = 2,
  };

  enum class BluetoothSspVariant {
    kPasskeyConfirmation = 0,
    kPasskeyEntry = 1,
    kConsent = 2,
    kPasskeyNotification = 3,
  };

  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    Observer() = default;
    ~Observer() override = default;

    // Notification sent when the adapter address has changed.
    virtual void AdapterAddressChanged(const std::string& address) {}

    // Notification sent when the discovering state has changed.
    virtual void AdapterDiscoveringChanged(bool state) {}

    // Notification sent when discovery has found a device. This notification
    // is not guaranteed to be unique per Chrome discovery session (i.e. you can
    // get the same device twice).
    virtual void AdapterFoundDevice(const FlossDeviceId& device_found) {}

    // Notification sent for Simple Secure Pairing.
    virtual void AdapterSspRequest(const FlossDeviceId& remote_device,
                                   uint32_t cod,
                                   BluetoothSspVariant variant,
                                   uint32_t passkey) {}
  };

  // Error: No such adapter.
  static const char kErrorUnknownAdapter[];

  // Creates the instance.
  static std::unique_ptr<FlossAdapterClient> Create();

  // Parses |FlossDeviceId| from a property map in DBus. The data is provided as
  // an array of dict entries (with a signature of a{sv}).
  static bool ParseFlossDeviceId(dbus::MessageReader* reader,
                                 FlossDeviceId* device);

  // Serializes |FlossDeviceId| as a property map in DBus (written as an array
  // of dict entries with a signature of a{sv}).
  static void SerializeFlossDeviceId(dbus::MessageWriter* writer,
                                     const FlossDeviceId& device);

  FlossAdapterClient(const FlossAdapterClient&) = delete;
  FlossAdapterClient& operator=(const FlossAdapterClient&) = delete;

  FlossAdapterClient();
  ~FlossAdapterClient() override;

  // Manage observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the address of this adapter.
  const std::string& GetAddress() const { return adapter_address_; }

  // Start a discovery session.
  virtual void StartDiscovery(ResponseCallback callback);

  // Cancel the active discovery session.
  virtual void CancelDiscovery(ResponseCallback callback);

  // Create a bond with the given device and transport.
  virtual void CreateBond(ResponseCallback callback,
                          FlossDeviceId device,
                          BluetoothTransport transport);

  // Get the object path for this adapter.
  const dbus::ObjectPath* GetObjectPath() const { return &adapter_path_; }

  // Initialize the adapter client.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const std::string& adapter_path) override;

 protected:
  friend class FlossAdapterClientTest;

  // Handle response to |GetAddress| DBus method call.
  void HandleGetAddress(dbus::Response* response,
                        dbus::ErrorResponse* error_response);

  // Handle callback |OnAddressChanged| on exported object path.
  void OnAddressChanged(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDiscoveringChanged| on exported object path.
  void OnDiscoveringChanged(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnDeviceFound| on exported object path.
  void OnDeviceFound(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OnSspRequest| on exported object path.
  void OnSspRequest(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  dbus::Bus* bus_ = nullptr;

  // Adapter managed by this client.
  dbus::ObjectPath adapter_path_;

  // Service which implements the adapter interface.
  std::string service_name_;

  // Address of adapter.
  std::string adapter_address_;

 private:
  // Object path for exported callbacks registered against adapter interface.
  static const char kExportedCallbacksPath[];

  base::WeakPtrFactory<FlossAdapterClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_ADAPTER_CLIENT_H_
