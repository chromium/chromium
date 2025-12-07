// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Values representing the possible properties of a characteristic.
// Characteristic permissions are inferred from these properties.
// Please see the Bluetooth 4.x spec to see the meaning of each individual
// property.
enum CharacteristicProperty {
  "broadcast",
  "read",
  "writeWithoutResponse",
  "write",
  "notify",
  "indicate",
  "authenticatedSignedWrites",
  "extendedProperties",
  "reliableWrite",
  "writableAuxiliaries",
  "encryptRead",
  "encryptWrite",
  "encryptAuthenticatedRead",
  "encryptAuthenticatedWrite"
};

// Values representing possible permissions for a descriptor.
// Please see the Bluetooth 4.x spec to see the meaning of each individual
// permission.
enum DescriptorPermission {
  "read",
  "write",
  "encryptedRead",
  "encryptedWrite",
  "encryptedAuthenticatedRead",
  "encryptedAuthenticatedWrite"
};

// Type of advertisement. If 'broadcast' is chosen, the sent advertisement
// type will be ADV_NONCONN_IND and the device will broadcast with a random
// MAC Address. If set to 'peripheral', the advertisement type will be
// ADV_IND or ADV_SCAN_IND and the device will broadcast with real Bluetooth
// Adapter's MAC Address.
enum AdvertisementType {
  "broadcast",
  "peripheral"
};

// Represents a bluetooth central device that is connected to the local GATT
// server.
dictionary Device {
  // The address of the device, in the format 'XX:XX:XX:XX:XX:XX'.
  required DOMString address;

  // The human-readable name of the device.
  DOMString name;

  // The class of the device, a bit-field defined by
  // http://www.bluetooth.org/en-us/specification/assigned-numbers/baseband.
  long deviceClass;
};

// Represents a peripheral's Bluetooth GATT Service, a collection of
// characteristics and relationships to other services that encapsulate
// the behavior of part of a device.
dictionary Service {
  // The UUID of the service, e.g. 0000180d-0000-1000-8000-00805f9b34fb.
  required DOMString uuid;

  // Indicates whether the type of this service is primary or secondary.
  required boolean isPrimary;

  // Returns the identifier assigned to this service. Use the instance ID to
  // distinguish between services from a peripheral with the same UUID and
  // to make function calls that take in a service identifier. Present, if
  // this instance represents a remote service.
  DOMString instanceId;

  // The device address of the remote peripheral that the GATT service belongs
  // to. Present, if this instance represents a remote service.
  DOMString deviceAddress;
};

// Represents a GATT characteristic, which is a basic data element that
// provides further information about a peripheral's service.
dictionary Characteristic {
  // The UUID of the characteristic, e.g.
  // 00002a37-0000-1000-8000-00805f9b34fb.
  required DOMString uuid;

  // The GATT service this characteristic belongs to.
  Service service;

  // The properties of this characteristic.
  required sequence<CharacteristicProperty> properties;

  // Returns the identifier assigned to this characteristic. Use the instance
  // ID to distinguish between characteristics from a peripheral with the same
  // UUID and to make function calls that take in a characteristic identifier.
  // Present, if this instance represents a remote characteristic.
  DOMString instanceId;

  // The currently cached characteristic value. This value gets updated when
  // the value of the characteristic is read or updated via a notification
  // or indication.
  ArrayBuffer value;
};

// Represents a GATT characteristic descriptor, which provides further
// information about a characteristic's value.
dictionary Descriptor {
  // The UUID of the characteristic descriptor, e.g.
  // 00002902-0000-1000-8000-00805f9b34fb.
  required DOMString uuid;

  // The GATT characteristic this descriptor belongs to.
  Characteristic characteristic;

  // The permissions of this descriptor.
  required sequence<DescriptorPermission> permissions;

  // Returns the identifier assigned to this descriptor. Use the instance ID
  // to distinguish between descriptors from a peripheral with the same UUID
  // and to make function calls that take in a descriptor identifier. Present,
  // if this instance represents a remote characteristic.
  DOMString instanceId;

  // The currently cached descriptor value. This value gets updated when
  // the value of the descriptor is read.
  ArrayBuffer value;
};

// The connection properties specified during a call to $(ref:connect).
dictionary ConnectProperties {
  // Flag indicating whether a connection to the device is left open when the
  // event page of the application is unloaded (see <a
  // href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is <code>false.</code>
  required boolean persistent;
};

// Optional characteristic notification session properties specified during a
// call to $(ref:startCharacteristicNotifications).
dictionary NotificationProperties {
  // Flag indicating whether the app should receive notifications when the
  // event page of the application is unloaded (see <a
  // href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is <code>false</code>.
  required boolean persistent;
};

// Represents an entry of the "Manufacturer Specific Data" field of Bluetooth
// LE advertisement data.
dictionary ManufacturerData {
  required long id;
  required sequence<long> data;
};

// Represents an entry of the "Service Data" field of Bluetooth LE advertisement
// data.
dictionary ServiceData {
  required DOMString uuid;
  required sequence<long> data;
};

// Represents a Bluetooth LE advertisement instance.
dictionary Advertisement {
  // Type of advertisement.
  required AdvertisementType type;

  // List of UUIDs to include in the "Service UUIDs" field of the Advertising
  // Data. These UUIDs can be of the 16bit, 32bit or 128 formats.
  sequence<DOMString> serviceUuids;

  // List of manufacturer specific data to be included in "Manufacturer Specific
  // Data" fields of the advertising data.
  sequence<ManufacturerData> manufacturerData;

  // List of UUIDs to include in the "Solicit UUIDs" field of the Advertising
  // Data. These UUIDs can be of the 16bit, 32bit or 128 formats.
  sequence<DOMString> solicitUuids;

  // List of service data to be included in "Service Data" fields of the
  // advertising data.
  sequence<ServiceData> serviceData;
};

// Represents a an attribute read/write request.
dictionary Request {
  // Unique ID for this request. Use this ID when responding to this request.
  required long requestId;
  // Device that send this request.
  required Device device;
  // Value to write (if this is a write request).
  ArrayBuffer value;
};

// Represents a response to an attribute read/write request.
dictionary Response {
  // Id of the request this is a response to.
  required long requestId;
  // If this is an error response, this should be true.
  required boolean isError;
  // Response value. Write requests and error responses will ignore this
  // parameter.
  ArrayBuffer value;
};

// Represents a notification to be sent to a remote device.
dictionary Notification {
  // New value of the characteristic.
  required ArrayBuffer value;
  // Optional flag for sending an indication instead of a notification.
  boolean shouldIndicate;
};

// |service|: The GATT service that was added.
callback OnServiceAddedListener = undefined(Service service);
interface OnServiceAddedEvent : ExtensionEvent {
  static undefined addListener(OnServiceAddedListener listener);
  static undefined removeListener(OnServiceAddedListener listener);
  static boolean hasListener(OnServiceAddedListener listener);
};

// |service|: The GATT service whose state has changed.
callback OnServiceChangedListener = undefined(Service service);
interface OnServiceChangedEvent : ExtensionEvent {
  static undefined addListener(OnServiceChangedListener listener);
  static undefined removeListener(OnServiceChangedListener listener);
  static boolean hasListener(OnServiceChangedListener listener);
};

// |service|: The GATT service that was removed.
callback OnServiceRemovedListener = undefined(Service service);
interface OnServiceRemovedEvent : ExtensionEvent {
  static undefined addListener(OnServiceRemovedListener listener);
  static undefined removeListener(OnServiceRemovedListener listener);
  static boolean hasListener(OnServiceRemovedListener listener);
};

// |characteristic|: The GATT characteristic whose value has changed.
callback OnCharacteristicValueChangedListener =
    undefined(Characteristic characteristic);
interface OnCharacteristicValueChangedEvent : ExtensionEvent {
  static undefined addListener(OnCharacteristicValueChangedListener listener);
  static undefined removeListener(
      OnCharacteristicValueChangedListener listener);
  static boolean hasListener(OnCharacteristicValueChangedListener listener);
};

// |descriptor|: The GATT characteristic descriptor whose value has
// changed.
callback OnDescriptorValueChangedListener = undefined(Descriptor descriptor);
interface OnDescriptorValueChangedEvent : ExtensionEvent {
  static undefined addListener(OnDescriptorValueChangedListener listener);
  static undefined removeListener(OnDescriptorValueChangedListener listener);
  static boolean hasListener(OnDescriptorValueChangedListener listener);
};

// |request|: Request data for this request.
// |characteristicId|: The instance ID of the GATT characteristic whose
// value is requested.
callback OnCharacteristicReadRequestListener =
    undefined(Request request, DOMString characteristicId);
interface OnCharacteristicReadRequestEvent : ExtensionEvent {
  static undefined addListener(OnCharacteristicReadRequestListener listener);
  static undefined removeListener(OnCharacteristicReadRequestListener listener);
  static boolean hasListener(OnCharacteristicReadRequestListener listener);
};

// |request|: Request data for this request.
// |characteristicId|: The instance ID of the GATT characteristic whose
// value is being written.
callback OnCharacteristicWriteRequestListener =
    undefined(Request request, DOMString characteristicId);
interface OnCharacteristicWriteRequestEvent : ExtensionEvent {
  static undefined addListener(OnCharacteristicWriteRequestListener listener);
  static undefined removeListener(
      OnCharacteristicWriteRequestListener listener);
  static boolean hasListener(OnCharacteristicWriteRequestListener listener);
};

// |request|: Request data for this request.
// |descriptorId|: The instance ID of the GATT descriptor whose value is
// requested.
callback OnDescriptorReadRequestListener =
    undefined(Request request, DOMString descriptorId);
interface OnDescriptorReadRequestEvent : ExtensionEvent {
  static undefined addListener(OnDescriptorReadRequestListener listener);
  static undefined removeListener(OnDescriptorReadRequestListener listener);
  static boolean hasListener(OnDescriptorReadRequestListener listener);
};

// |request|: Request data for this request.
// |descriptorId|: The instance ID of the GATT descriptor whose value is
// being written.
callback OnDescriptorWriteRequestListener =
    undefined(Request request, DOMString descriptorId);
interface OnDescriptorWriteRequestEvent : ExtensionEvent {
  static undefined addListener(OnDescriptorWriteRequestListener listener);
  static undefined removeListener(OnDescriptorWriteRequestListener listener);
  static boolean hasListener(OnDescriptorWriteRequestListener listener);
};

// The <code>chrome.bluetoothLowEnergy</code> API is used to communicate with
// Bluetooth Smart (Low Energy) devices using the
// <a href="https://developer.bluetooth.org/TechnologyOverview/Pages/GATT.aspx">
// Generic Attribute Profile (GATT)</a>.
interface BluetoothLowEnergy {
  // Establishes a connection between the application and the device with the
  // given address. A device may be already connected and its GATT services
  // available without calling <code>connect</code>, however, an app that
  // wants to access GATT services of a device should call this function to
  // make sure that a connection to the device is maintained. If the device
  // is not connected, all GATT services of the device will be discovered
  // after a successful call to <code>connect</code>.
  // |deviceAddress|: The Bluetooth address of the remote device to which a
  // GATT connection should be opened.
  // |properties|: Connection properties (optional).
  // |Returns|: Called when the connect request has completed.
  [requiredCallback] static Promise<undefined> connect(
      DOMString deviceAddress,
      optional ConnectProperties properties);

  // Closes the app's connection to the device with the given address. Note
  // that this will not always destroy the physical link itself, since there
  // may be other apps with open connections.
  // |deviceAddress|: The Bluetooth address of the remote device.
  // |Returns|: Called when the disconnect request has completed.
  static Promise<undefined> disconnect(DOMString deviceAddress);

  // Get the GATT service with the given instance ID.
  // |serviceId|: The instance ID of the requested GATT service.
  // |Returns|: Called with the requested Service object.
  // |PromiseValue|: result
  [requiredCallback] static Promise<Service> getService(DOMString serviceId);

  // Create a locally hosted GATT service. This service can be registered
  // to be available on a local GATT server.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |service|: The service to create.
  // |Returns|: Called with the created services's unique ID.
  // |PromiseValue|: serviceId
  [requiredCallback] static Promise<DOMString> createService(Service service);

  // Get all the GATT services that were discovered on the remote device with
  // the given device address.
  //
  // <em>Note:</em> If service discovery is not yet complete on the device,
  // this API will return a subset (possibly empty) of services. A work around
  // is to add a time based delay and/or call repeatedly until the expected
  // number of services is returned.
  //
  // |deviceAddress|: The Bluetooth address of the remote device whose GATT
  // services should be returned.
  // |Returns|: Called with the list of requested Service objects.
  // |PromiseValue|: result
  [requiredCallback] static Promise<sequence<Service>> getServices(
      DOMString deviceAddress);

  // Get the GATT characteristic with the given instance ID that belongs to
  // the given GATT service, if the characteristic exists.
  // |characteristicId|: The instance ID of the requested GATT
  // characteristic.
  // |Returns|: Called with the requested Characteristic object.
  // |PromiseValue|: result
  [requiredCallback] static Promise<Characteristic> getCharacteristic(
      DOMString characteristicId);

  // Create a locally hosted GATT characteristic. This characteristic must
  // be hosted under a valid service. If the service ID is not valid, the
  // lastError will be set.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |characteristic|: The characteristic to create.
  // |serviceId|: ID of the service to create this characteristic for.
  // |Returns|: Called with the created characteristic's unique ID.
  // |PromiseValue|: characteristicId
  [requiredCallback] static Promise<DOMString> createCharacteristic(
      Characteristic characteristic,
      DOMString serviceId);

  // Get a list of all discovered GATT characteristics that belong to the
  // given service.
  // |serviceId|: The instance ID of the GATT service whose characteristics
  // should be returned.
  // |Returns|: Called with the list of characteristics that belong to the
  // given service.
  // |PromiseValue|: result
  [requiredCallback]
  static Promise<sequence<Characteristic>> getCharacteristics(
      DOMString serviceId);

  // Get a list of GATT services that are included by the given service.
  // |serviceId|: The instance ID of the GATT service whose included
  // services should be returned.
  // |Returns|: Called with the list of GATT services included from the
  // given service.
  // |PromiseValue|: result
  [requiredCallback] static Promise<sequence<Service>> getIncludedServices(
      DOMString serviceId);

  // Get the GATT characteristic descriptor with the given instance ID.
  // |descriptorId|: The instance ID of the requested GATT characteristic
  // descriptor.
  // |Returns|: Called with the requested Descriptor object.
  // |PromiseValue|: result
  [requiredCallback] static Promise<Descriptor> getDescriptor(
      DOMString descriptorId);

  // Create a locally hosted GATT descriptor. This descriptor must
  // be hosted under a valid characteristic. If the characteristic ID is not
  // valid, the lastError will be set.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |descriptor|: The descriptor to create.
  // |characteristicId|: ID of the characteristic to create this descriptor
  // for.
  // |Returns|: Called with the created descriptor's unique ID.
  // |PromiseValue|: descriptorId
  [requiredCallback] static Promise<DOMString> createDescriptor(
      Descriptor descriptor,
      DOMString characteristicId);

  // Get a list of GATT characteristic descriptors that belong to the given
  // characteristic.
  // |characteristicId|: The instance ID of the GATT characteristic whose
  // descriptors should be returned.
  // |Returns|: Called with the list of descriptors that belong to the given
  // characteristic.
  // |PromiseValue|: result
  [requiredCallback] static Promise<sequence<Descriptor>> getDescriptors(
      DOMString characteristicId);

  // Retrieve the value of a specified characteristic from a remote
  // peripheral.
  // |characteristicId|: The instance ID of the GATT characteristic whose
  // value should be read from the remote device.
  // |Returns|: Called with the Characteristic object whose value was
  // requested. The <code>value</code> field of the returned Characteristic
  // object contains the result of the read request.
  // |PromiseValue|: result
  [requiredCallback] static Promise<Characteristic> readCharacteristicValue(
      DOMString characteristicId);

  // Write the value of a specified characteristic from a remote peripheral.
  // |characteristicId|: The instance ID of the GATT characteristic whose
  // value should be written to.
  // |value|: The value that should be sent to the remote characteristic as
  // part of the write request.
  // |Returns|: Called when the write request has completed.
  [requiredCallback] static Promise<undefined> writeCharacteristicValue(
      DOMString characteristicId,
      ArrayBuffer value);

  // Enable value notifications/indications from the specified characteristic.
  // Once enabled, an application can listen to notifications using the
  // $(ref:onCharacteristicValueChanged) event.
  // |characteristicId|: The instance ID of the GATT characteristic that
  // notifications should be enabled on.
  // |properties|: Notification session properties (optional).
  // |Returns|: Called when the request has completed.
  [requiredCallback] static Promise<undefined> startCharacteristicNotifications(
      DOMString characteristicId,
      optional NotificationProperties properties);

  // Disable value notifications/indications from the specified
  // characteristic. After a successful call, the application will stop
  // receiving notifications/indications from this characteristic.
  // |characteristicId|: The instance ID of the GATT characteristic on which
  // this app's notification session should be stopped.
  // |Returns|: Called when the request has completed (optional).
  static Promise<undefined> stopCharacteristicNotifications(
      DOMString characteristicId);

  // Notify a remote device of a new value for a characteristic. If the
  // shouldIndicate flag in the notification object is true, an indication
  // will be sent instead of a notification. Note, the characteristic needs
  // to correctly set the 'notify' or 'indicate' property during creation for
  // this call to succeed.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |characteristicId|: The characteristic to send the notication for.
  // |notifcation|: The notification to send.
  // |Returns|: Callback called once the notification or indication has
  // been sent successfully.
  [requiredCallback] static Promise<undefined> notifyCharacteristicValueChanged(
      DOMString characteristicId,
      Notification notification);

  // Retrieve the value of a specified characteristic descriptor from a remote
  // peripheral.
  // |descriptorId|: The instance ID of the GATT characteristic descriptor
  // whose value should be read from the remote device.
  // |Returns|: Called with the Descriptor object whose value was requested.
  // The <code>value</code> field of the returned Descriptor object contains
  // the result of the read request.
  // |PromiseValue|: result
  [requiredCallback] static Promise<Descriptor> readDescriptorValue(
      DOMString descriptorId);

  // Write the value of a specified characteristic descriptor from a remote
  // peripheral.
  // |descriptorId|: The instance ID of the GATT characteristic descriptor
  // whose value should be written to.
  // |value|: The value that should be sent to the remote descriptor as part
  // of the write request.
  // |Returns|: Called when the write request has completed.
  [requiredCallback] static Promise<undefined> writeDescriptorValue(
      DOMString descriptorId,
      ArrayBuffer value);

  // Register the given service with the local GATT server. If the service
  // ID is invalid, the lastError will be set.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |serviceId|: Unique ID of a created service.
  // |Returns|: Callback with the result of the register operation.
  [requiredCallback] static Promise<undefined> registerService(
      DOMString serviceId);

  // Unregister the given service with the local GATT server. If the service
  // ID is invalid, the lastError will be set.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |serviceId|: Unique ID of a current registered service.
  // |Returns|: Callback with the result of the register operation.
  [requiredCallback] static Promise<undefined> unregisterService(
      DOMString serviceId);

  // Remove the specified service, unregistering it if it was registered.
  // If the service ID is invalid, the lastError will be set.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |serviceId|: Unique ID of a current registered service.
  // |Returns|: Callback called once the service is removed.
  static Promise<undefined> removeService(DOMString serviceId);

  // Create an advertisement and register it for advertising. To call this
  // function, the app must have the bluetooth:low_energy and
  // bluetooth:peripheral permissions set to true. Additionally this API
  // is only available to auto launched apps in Kiosk Mode or by setting
  // the '--enable-ble-advertising-in-apps' command-line switch.
  // See https://developer.chrome.com/apps/manifest/bluetooth
  // Note: On some hardware, central and peripheral modes at the same time is
  // supported but on hardware that doesn't support this, making this call
  // will switch the device to peripheral mode. In the case of hardware which
  // does not support both central and peripheral mode, attempting to use the
  // device in both modes will lead to undefined behavior or prevent other
  // central-role applications from behaving correctly (including the
  // discovery of Bluetooth Low Energy devices).
  // |advertisement|: The advertisement to advertise.
  // |Returns|: Called once the registeration is done and we've started
  // advertising. Returns the id of the created advertisement.
  // |PromiseValue|: advertisementId
  [requiredCallback] static Promise<long> registerAdvertisement(
      Advertisement advertisement);

  // Unregisters an advertisement and stops its advertising. If the
  // advertisement fails to unregister the only way to stop advertising
  // might be to restart the device.
  // |advertisementId|: Id of the advertisement to unregister.
  // |Returns|: Called once the advertisement is unregistered and is no
  // longer being advertised.
  [requiredCallback] static Promise<undefined> unregisterAdvertisement(
      long advertisementId);

  // Resets advertising on the current device. It will unregister and
  // stop all existing advertisements.
  // |Returns|: Called once the advertisements are reset.
  [requiredCallback] static Promise<undefined> resetAdvertising();

  // Set's the interval betweeen two consecutive advertisements. Note:
  // This is a best effort. The actual interval may vary non-trivially
  // from the requested intervals. On some hardware, there is a minimum
  // interval of 100ms. The minimum and maximum values cannot exceed the
  // the range allowed by the Bluetooth 4.2 specification.
  // |minInterval|: Minimum interval between advertisments (in
  // milliseconds). This cannot be lower than 20ms (as per the spec).
  // |maxInterval|: Maximum interval between advertisments (in
  // milliseconds). This cannot be more than 10240ms (as per the spec).
  // |Returns|: Called once the interval has been set.
  [requiredCallback] static Promise<undefined> setAdvertisingInterval(
      long minInterval,
      long maxInterval);

  // Sends a response for a characteristic or descriptor read/write
  // request.
  // This function is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  // |response|: The response to the request.
  static undefined sendRequestResponse(Response response);

  // Fired whan a new GATT service has been discovered on a remote device.
  static attribute OnServiceAddedEvent onServiceAdded;

  // Fired when the state of a remote GATT service changes. This involves any
  // characteristics and/or descriptors that get added or removed from the
  // service, as well as "ServiceChanged" notifications from the remote
  // device.
  static attribute OnServiceChangedEvent onServiceChanged;

  // Fired when a GATT service that was previously discovered on a remote
  // device has been removed.
  static attribute OnServiceRemovedEvent onServiceRemoved;

  // Fired when the value of a remote GATT characteristic changes, either as
  // a result of a read request, or a value change notification/indication
  // This event will only be sent if the app has enabled notifications by
  // calling $(ref:startCharacteristicNotifications).
  static attribute OnCharacteristicValueChangedEvent onCharacteristicValueChanged;

  // Fired when the value of a remote GATT characteristic descriptor changes,
  // usually as a result of a read request. This event exists
  // mostly for convenience and will always be sent after a successful
  // call to $(ref:readDescriptorValue).
  static attribute OnDescriptorValueChangedEvent onDescriptorValueChanged;

  // Fired when a connected central device requests to read the value of a
  // characteristic registered on the local GATT server. Not responding
  // to this request for a long time may lead to a disconnection.
  // This event is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  static attribute OnCharacteristicReadRequestEvent onCharacteristicReadRequest;

  // Fired when a connected central device requests to write the value of a
  // characteristic registered on the local GATT server. Not responding
  // to this request for a long time may lead to a disconnection.
  // This event is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  static attribute OnCharacteristicWriteRequestEvent onCharacteristicWriteRequest;

  // Fired when a connected central device requests to read the value of a
  // descriptor registered on the local GATT server. Not responding to
  // this request for a long time may lead to a disconnection.
  // This event is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  static attribute OnDescriptorReadRequestEvent onDescriptorReadRequest;

  // Fired when a connected central device requests to write the value of a
  // descriptor registered on the local GATT server. Not responding to
  // this request for a long time may lead to a disconnection.
  // This event is only available if the app has both the
  // bluetooth:low_energy and the bluetooth:peripheral permissions set to
  // true. The peripheral permission may not be available to all apps.
  static attribute OnDescriptorWriteRequestEvent onDescriptorWriteRequest;
};

partial interface Browser {
  static attribute BluetoothLowEnergy bluetoothLowEnergy;
};
