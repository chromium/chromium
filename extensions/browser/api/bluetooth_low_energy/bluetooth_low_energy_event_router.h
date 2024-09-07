// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/bluetooth_low_energy.h"
#include "extensions/common/extension_id.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace device {

class BluetoothLocalGattCharacteristic;
class BluetoothLocalGattDescriptor;
class BluetoothRemoteGattCharacteristic;
class BluetoothRemoteGattDescriptor;

}  // namespace device

namespace extensions {

class BluetoothLowEnergyConnection;
class BluetoothLowEnergyNotifySession;
class Extension;

// The BluetoothLowEnergyEventRouter is used by the bluetoothLowEnergy API to
// interface with the internal Bluetooth API in device/bluetooth.
class BluetoothLowEnergyEventRouter
    : public device::BluetoothAdapter::Observer,
      public device::BluetoothLocalGattService::Delegate,
      public extensions::ExtensionRegistryObserver {
 public:
  struct AttributeValueRequest {
   public:
    enum RequestType { ATTRIBUTE_READ_REQUEST, ATTRIBUTE_WRITE_REQUEST };

    AttributeValueRequest(Delegate::ValueCallback value_callback,
                          Delegate::ErrorCallback error_callback);
    AttributeValueRequest(base::OnceClosure success_callback,
                          Delegate::ErrorCallback error_callback);

    AttributeValueRequest(const AttributeValueRequest&) = delete;
    AttributeValueRequest& operator=(const AttributeValueRequest&) = delete;

    ~AttributeValueRequest();

    RequestType type;
    Delegate::ValueCallback value_callback;
    base::OnceClosure success_callback;
    Delegate::ErrorCallback error_callback;
  };

  explicit BluetoothLowEnergyEventRouter(content::BrowserContext* context);

  BluetoothLowEnergyEventRouter(const BluetoothLowEnergyEventRouter&) = delete;
  BluetoothLowEnergyEventRouter& operator=(
      const BluetoothLowEnergyEventRouter&) = delete;

  ~BluetoothLowEnergyEventRouter() override;

  // Possible ways that an API method can fail or succeed.
  enum Status {
    kStatusSuccess = 0,
    kStatusErrorAlreadyConnected,
    kStatusErrorAlreadyNotifying,
    kStatusErrorAuthenticationFailed,
    kStatusErrorCanceled,
    kStatusErrorFailed,
    kStatusErrorGattNotSupported,
    kStatusErrorHigherSecurity,
    kStatusErrorInProgress,
    kStatusErrorInsufficientAuthorization,
    kStatusErrorInvalidArguments,
    kStatusErrorInvalidLength,
    kStatusErrorNotConnected,
    kStatusErrorNotFound,
    kStatusErrorNotNotifying,
    kStatusErrorPermissionDenied,
    kStatusErrorTimeout,
    kStatusErrorUnsupportedDevice,
    kStatusErrorInvalidServiceId,
    kStatusErrorNotReady,
    kStatusErrorAlreadyExists,
    kStatusErrorDoesNotExist,
    kStatusErrorNoMemory,
    kStatusErrorJniEnvironment,
    kStatusErrorJniThreadAttach,
    kStatusErrorWakelock,
    kStatusErrorUnexpectedState,
    kStatusErrorSocket,
  };

  // Error callback is used by asynchronous methods to report failures.
  using ErrorCallback = base::OnceCallback<void(Status)>;

  // Returns true if Bluetooth is supported on the current platform or if the
  // internal |adapter_| instance has been initialized for testing.
  bool IsBluetoothSupported() const;

  // Obtains a handle on the BluetoothAdapter and invokes |callback|. Returns
  // false, if Bluetooth is not supported. Otherwise, asynchronously initializes
  // it and invokes |callback|. Until the first successful call to this method,
  // none of the methods in this class will succeed and no device::Bluetooth*
  // API events will be observed.
  bool InitializeAdapterAndInvokeCallback(base::OnceClosure callback);

  // Returns true, if the BluetoothAdapter was initialized.
  bool HasAdapter() const;

  // Creates a GATT connection to the device with address |device_address| for
  // extension |extension|. The connection is kept alive until the extension is
  // unloaded, the device is removed, or is disconnect by the host subsystem.
  // |callback| is called with the status of the connect operation. If
  // |persistent| is true, then the allocated connection resource is persistent
  // across unloads.
  void Connect(bool persistent,
               const Extension* extension,
               const std::string& device_address,
               ErrorCallback callback);

  // Disconnects the currently open GATT connection of extension |extension| to
  // device with address |device_address|. |error_callback| is called with an
  // error status in case of failure, e.g. if the device is not found or the
  // given
  // extension does not have an open connection to the device.
  void Disconnect(const Extension* extension,
                  const std::string& device_address,
                  base::OnceClosure callback,
                  ErrorCallback error_callback);

  // Returns the list of api::bluetooth_low_energy::Service objects
  // associated with the Bluetooth device with address |device_address| if
  // successful. Otherwise returns std::nullopt, if no device with the given
  // address is known. If the device is found but it has no GATT services, then
  // returns an empty list.
  typedef std::vector<api::bluetooth_low_energy::Service> ServiceList;
  std::optional<ServiceList> GetServices(
      const std::string& device_address) const;

  // Returns a service based on GATT service with instance ID |instance_id| if
  // successful. Otherwise returns an error status.
  base::expected<api::bluetooth_low_energy::Service, Status> GetService(
      const std::string& instance_id) const;

  // Returns a service list of GATT services that are included
  // by the GATT service with instance ID |instance_id|. Returns an error status
  // if no GATT service with the given ID is known. If the given service has no
  // included services, then an empty list is returned.
  base::expected<ServiceList, Status> GetIncludedServices(
      const std::string& instance_id) const;

  // Returns the list of api::bluetooth_low_energy::Characteristic objects
  // associated with the GATT service with instance ID |instance_id|. Returns an
  // error status, if no service with the given instance ID is known. If the
  // service is found but it has no characteristics, then returns empty list.
  // |extension| is the extension that made the call.
  typedef std::vector<api::bluetooth_low_energy::Characteristic>
      CharacteristicList;
  base::expected<CharacteristicList, Status> GetCharacteristics(
      const Extension* extension,
      const std::string& instance_id) const;

  // Returns a Characteristic based on GATT characteristic with instance
  // ID |instance_id|. |extension| is the
  // extension that made the call.
  base::expected<api::bluetooth_low_energy::Characteristic, Status>
  GetCharacteristic(const Extension* extension,
                    const std::string& instance_id) const;

  // Returns the list of api::bluetooth_low_energy::Descriptor objects
  // associated with the GATT characteristic with instance ID |instance_id|. If
  // the characteristic is found but it has no descriptors, then returns an
  // empty list. In case of failure, returns a error status. |extension| is the
  // extension that made the call.
  typedef std::vector<api::bluetooth_low_energy::Descriptor> DescriptorList;
  base::expected<DescriptorList, Status> GetDescriptors(
      const Extension* extension,
      const std::string& instance_id) const;

  // Returns a descriptor based on GATT characteristic descriptor with
  // instance ID |instance_id|. |extension| is the extension that made the call.
  base::expected<api::bluetooth_low_energy::Descriptor, Status> GetDescriptor(
      const Extension* extension,
      const std::string& instance_id) const;

  // Sends a request to read the value of the characteristic with intance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void ReadCharacteristicValue(const Extension* extension,
                               const std::string& instance_id,
                               base::OnceClosure callback,
                               ErrorCallback error_callback);

  // Sends a request to write the value of the characteristic with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void WriteCharacteristicValue(const Extension* extension,
                                const std::string& instance_id,
                                const std::vector<uint8_t>& value,
                                base::OnceClosure callback,
                                ErrorCallback error_callback);

  // Sends a request to start characteristic notifications from characteristic
  // with instance ID |instance_id|, for extension |extension|. Invokes
  // |callback| on success and |error_callback| on failure. If |persistent| is
  // true, then the allocated connection resource is persistent across unloads.
  void StartCharacteristicNotifications(bool persistent,
                                        const Extension* extension,
                                        const std::string& instance_id,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback);

  // Sends a request to stop characteristic notifications from characteristic
  // with instance ID |instance_id|, for extension |extension|. Invokes
  // |callback| on success and |error_callback| on failure.
  void StopCharacteristicNotifications(const Extension* extension,
                                       const std::string& instance_id,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback);

  // Sends a request to read the value of the descriptor with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void ReadDescriptorValue(const Extension* extension,
                           const std::string& instance_id,
                           base::OnceClosure callback,
                           ErrorCallback error_callback);

  // Sends a request to write the value of the descriptor with instance ID
  // |instance_id|. Invokes |callback| on success and |error_callback| on
  // failure. |extension| is the extension that made the call.
  void WriteDescriptorValue(const Extension* extension,
                            const std::string& instance_id,
                            const std::vector<uint8_t>& value,
                            base::OnceClosure callback,
                            ErrorCallback error_callback);

  // Initializes the adapter for testing. Used by unit tests only.
  void SetAdapterForTesting(device::BluetoothAdapter* adapter);

  // device::BluetoothAdapter::Observer overrides.
  void GattServiceAdded(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        device::BluetoothRemoteGattService* service) override;
  void GattServiceRemoved(device::BluetoothAdapter* adapter,
                          device::BluetoothDevice* device,
                          device::BluetoothRemoteGattService* service) override;
  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;
  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;
  void GattServiceChanged(device::BluetoothAdapter* adapter,
                          device::BluetoothRemoteGattService* service) override;
  void GattCharacteristicAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;
  void GattCharacteristicRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic) override;
  void GattDescriptorAdded(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;
  void GattDescriptorRemoved(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void GattDescriptorValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value) override;

  // device::BluetoothLocalGattService::Delegate overrides.
  void OnCharacteristicReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      int offset,
      Delegate::ValueCallback value_callback) override;
  void OnCharacteristicWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      Delegate::ErrorCallback error_callback) override;
  void OnCharacteristicPrepareWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_request,
      base::OnceClosure callback,
      Delegate::ErrorCallback error_callback) override;
  void OnDescriptorReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      int offset,
      Delegate::ValueCallback value_callback) override;
  void OnDescriptorWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      Delegate::ErrorCallback error_callback) override;
  void OnNotificationsStart(
      const device::BluetoothDevice* device,
      device::BluetoothGattCharacteristic::NotificationType notification_type,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;
  void OnNotificationsStop(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // Adds a mapping for a local characteristic ID to its service ID
  void AddLocalCharacteristic(const std::string& id,
                              const std::string& service_id);
  // Returns a BluetoothGattCharacteristic by its ID |id|.
  // Returns NULL, if the characteristic cannot be found.
  device::BluetoothLocalGattCharacteristic* GetLocalCharacteristic(
      const std::string& id) const;

  // Adds a mapping for a service_id to the id of the app that owns it.
  void AddServiceToApp(const std::string& app_id,
                       const std::string& service_id);

  // Removes a mapping for a service_id to the id of the app that owns it.
  void RemoveServiceFromApp(const std::string& app_id,
                            const std::string& service_id);

  // Register a local GATT service.
  void RegisterGattService(const Extension* extension,
                           const std::string& service_id,
                           base::OnceClosure callback,
                           ErrorCallback error_callback);

  // Unregister a local GATT service.
  void UnregisterGattService(const Extension* extension,
                             const std::string& service_id,
                             base::OnceClosure callback,
                             ErrorCallback error_callback);

  // Handle a response from the app for the given request id.
  void HandleRequestResponse(const Extension* extension,
                             size_t request_id,
                             bool is_error,
                             const std::vector<uint8_t>& value);

  device::BluetoothAdapter* adapter() { return adapter_.get(); }

 private:
  // Called by BluetoothAdapterFactory.
  void OnGetAdapter(base::OnceClosure callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  // Initializes the identifier for all existing GATT objects and devices.
  // Called by OnGetAdapter and SetAdapterForTesting.
  void InitializeIdentifierMappings();

  // Sends the event named |event_name| to all listeners of that event that
  // have the Bluetooth UUID manifest permission for UUID |uuid| and the
  // "low_energy" manifest permission, with |args| as the argument to that
  // event. If the event involves a characteristic, then |characteristic_id|
  // should be the instance ID of the involved characteristic. Otherwise, an
  // empty string should be passed.
  void DispatchEventToExtensionsWithPermission(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      const device::BluetoothUUID& uuid,
      const std::string& characteristic_id,
      base::Value::List args);

  void DispatchEventToExtension(const ExtensionId& extension_id,
                                events::HistogramValue histogram_value,
                                const std::string& event_name,
                                base::Value::List args);

  // Returns a BluetoothRemoteGattService by its instance ID |instance_id|.
  // Returns
  // NULL, if the service cannot be found.
  device::BluetoothRemoteGattService* FindServiceById(
      const std::string& instance_id) const;

  // Returns a BluetoothRemoteGattCharacteristic by its instance ID
  // |instance_id|.
  // Returns NULL, if the characteristic cannot be found.
  device::BluetoothRemoteGattCharacteristic* FindCharacteristicById(
      const std::string& instance_id) const;

  // Returns a BluetoothRemoteGattDescriptor by its instance ID |instance_id|.
  // Returns NULL, if the descriptor cannot be found.
  device::BluetoothRemoteGattDescriptor* FindDescriptorById(
      const std::string& instance_id) const;

  // Dispatches a BLUETOOTH_LOW_ENERGY_ON_CHARACTERISTIC_VALUE_CHANGED and runs
  // |callback|.
  void OnReadRemoteCharacteristic(
      const std::string& characteristic_instance_id,
      base::OnceClosure callback,
      ErrorCallback error_callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Runs |callback|.
  void OnReadRemoteDescriptor(
      base::OnceClosure callback,
      ErrorCallback error_callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Called by BluetoothDevice in response to a call to CreateGattConnection.
  void OnCreateGattConnection(
      bool persistent,
      const ExtensionId& extension_id,
      const std::string& device_address,
      ErrorCallback callback,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Called by BluetoothGattService in response to Register().
  void OnRegisterGattServiceSuccess(const std::string& service_id,
                                    const ExtensionId& extension_id,
                                    base::OnceClosure callback);

  // Called by BluetoothGattService in response to Unregister().
  void OnUnregisterGattServiceSuccess(const std::string& service_id,
                                      const ExtensionId& extension_id,
                                      base::OnceClosure callback);

  // Called by BluetoothRemoteGattCharacteristic and
  // BluetoothRemoteGattDescriptor in
  // case of an error during the write operations.
  void OnError(ErrorCallback error_callback,
               device::BluetoothGattService::GattErrorCode error_code);

  // Called by BluetoothRemoteGattCharacteristic in response to a call to
  // StartNotifySession.
  void OnStartNotifySession(
      bool persistent,
      const ExtensionId& extension_id,
      const std::string& characteristic_id,
      base::OnceClosure callback,
      std::unique_ptr<device::BluetoothGattNotifySession> session);

  // Called by BluetoothRemoteGattCharacteristic in response to a call to
  // StartNotifySession.
  void OnStartNotifySessionError(
      const ExtensionId& extension_id,
      const std::string& characteristic_id,
      ErrorCallback error_callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Called by BluetoothGattNotifySession in response to a call to Stop.
  void OnStopNotifySession(const ExtensionId& extension_id,
                           const std::string& characteristic_id,
                           base::OnceClosure callback);

  // Finds and returns a BluetoothLowEnergyConnection to device with address
  // |device_address| from the managed API resources for extension with ID
  // |extension_id|.
  BluetoothLowEnergyConnection* FindConnection(
      const ExtensionId& extension_id,
      const std::string& device_address);

  // Removes the connection to device with address |device_address| from the
  // managed API resources for extension with ID |extension_id|. Returns false,
  // if the connection could not be found.
  bool RemoveConnection(const ExtensionId& extension_id,
                        const std::string& device_address);

  // Finds and returns a BluetoothLowEnergyNotifySession associated with
  // characteristic with instance ID |characteristic_id| from the managed API
  // API resources for extension with ID |extension_id|.
  BluetoothLowEnergyNotifySession* FindNotifySession(
      const ExtensionId& extension_id,
      const std::string& characteristic_id);

  // Removes the notify session associated with characteristic with
  // instance ID |characteristic_id| from the managed API resources for
  // extension with ID |extension_id|. Returns false, if the session could
  // not be found.
  bool RemoveNotifySession(const ExtensionId& extension_id,
                           const std::string& characteristic_id);

  // Stores a request associated with an app and returns the ID of the request.
  // When an app sends a request to an app for getting/setting an attribute
  // value, we store that request so when the response comes in, we know where
  // to forward the results of the request.
  size_t StoreSentRequest(const ExtensionId& extension_id,
                          std::unique_ptr<AttributeValueRequest> request);

  // Mapping from instance ids to identifiers of owning instances. The keys are
  // used to identify individual instances of GATT objects and are used by
  // bluetoothLowEnergy API functions to obtain the correct GATT object to
  // operate on. Instance IDs are string identifiers that are returned by the
  // device/bluetooth API, by calling GetIdentifier() on the corresponding
  // device::BluetoothGatt* instance.
  //
  // This mapping is necessary, as GATT object instances can only be obtained
  // from the object that owns it, where raw pointers should not be cached. E.g.
  // to obtain a device::BluetoothRemoteGattCharacteristic, it is necessary to
  // obtain
  // a pointer to the associated device::BluetoothDevice, and then to the
  // device::BluetoothRemoteGattService that owns the characteristic.
  using InstanceIdMap = std::map<std::string, std::string>;
  InstanceIdMap service_id_to_device_address_;
  InstanceIdMap chrc_id_to_service_id_;
  InstanceIdMap desc_id_to_chrc_id_;

  // Map of which services are registered by which app. Used to route attribute
  // read/write events.
  InstanceIdMap service_id_to_extension_id_;

  // Pointer to the current BluetoothAdapter instance. This represents a local
  // Bluetooth adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Set of extension ID + device addresses to which a connect/disconnect is
  // currently pending.
  std::set<std::string> connecting_devices_;
  std::set<std::string> disconnecting_devices_;

  // Set of extension ID + characteristic ID to which a request to start a
  // notify session is currently pending.
  std::set<std::string> pending_session_calls_;

  using RequestIdToRequestMap =
      std::map<size_t, std::unique_ptr<AttributeValueRequest>>;
  using ExtensionToRequestsMap =
      std::map<std::string, std::unique_ptr<RequestIdToRequestMap>>;
  // Map of callback requests sent to the app for attribute value read/write
  // requests.
  ExtensionToRequestsMap requests_;
  // The last request ID we used.
  size_t last_callback_request_id_ = 0;

  // Map of locally hosted GATT service ids created by app_id. Used for cleanup.
  std::map<std::string, std::vector<std::string>> app_id_to_service_ids_;

  // BrowserContext passed during initialization.
  raw_ptr<content::BrowserContext> browser_context_;

  // Listen to extension unloaded notification.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLowEnergyEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_EVENT_ROUTER_H_
