// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Direction, Recipient, RequestType, and TransferType all map to their
// namesakes within the USB specification.
enum Direction {
  "in",
  "out"
};

enum Recipient {
  "device",
  "interface",
  "endpoint",
  "other"
};

enum RequestType {
  "standard",
  "class",
  "vendor",
  "reserved"
};

enum TransferType {
  "control",
  "interrupt",
  "isochronous",
  "bulk"
};

// For interrupt and isochronous modes, SynchronizationType and UsageType map
// to their namesakes within the USB specification.
enum SynchronizationType {
  "asynchronous",
  "adaptive",
  "synchronous"
};

enum UsageType {
  "data",
  "feedback",
  "explicitFeedback",
  "periodic",
  "notification"
};

dictionary Device {
  // An opaque ID for the USB device. It remains unchanged until the device is
  // unplugged.
  required long device;
  // The device vendor ID.
  required long vendorId;
  // The product ID.
  required long productId;
  // The device version (bcdDevice field).
  required long version;
  // The iProduct string read from the device, if available.
  required DOMString productName;
  // The iManufacturer string read from the device, if available.
  required DOMString manufacturerName;
  // The iSerialNumber string read from the device, if available.
  required DOMString serialNumber;
};

dictionary ConnectionHandle {
  // An opaque handle representing this connection to the USB device and all
  // associated claimed interfaces and pending transfers. A new handle is
  // created each time the device is opened. The connection handle is
  // different from $(ref:Device.device).
  required long handle;
  // The device vendor ID.
  required long vendorId;
  // The product ID.
  required long productId;
};

dictionary EndpointDescriptor {
  // Endpoint address.
  required long address;
  // Transfer type.
  required TransferType type;
  // Transfer direction.
  required Direction direction;
  // Maximum packet size.
  required long maximumPacketSize;
  // Transfer synchronization mode (isochronous only).
  SynchronizationType synchronization;
  // Endpoint usage hint.
  UsageType usage;
  // Polling interval (interrupt and isochronous only).
  long pollingInterval;
  // Extra descriptor data associated with this endpoint.
  required ArrayBuffer extra_data;
};

dictionary InterfaceDescriptor {
  // The interface number.
  required long interfaceNumber;
  // The interface alternate setting number (defaults to <code>0</code).
  required long alternateSetting;
  // The USB interface class.
  required long interfaceClass;
  // The USB interface sub-class.
  required long interfaceSubclass;
  // The USB interface protocol.
  required long interfaceProtocol;
  // Description of the interface.
  DOMString description;
  // Available endpoints.
  required sequence<EndpointDescriptor> endpoints;
  // Extra descriptor data associated with this interface.
  required ArrayBuffer extra_data;
};

dictionary ConfigDescriptor {
  // Is this the active configuration?
  required boolean active;
  // The configuration number.
  required long configurationValue;
  // Description of the configuration.
  DOMString description;
  // The device is self-powered.
  required boolean selfPowered;
  // The device supports remote wakeup.
  required boolean remoteWakeup;
  // The maximum power needed by this device in milliamps (mA).
  required long maxPower;
  // Available interfaces.
  required sequence<InterfaceDescriptor> interfaces;
  // Extra descriptor data associated with this configuration.
  required ArrayBuffer extra_data;
};

dictionary ControlTransferInfo {
  // The transfer direction (<code>"in"</code> or <code>"out"</code>).
  required Direction direction;

  // The transfer target. The target given by <code>index</code> must be
  // claimed if <code>"interface"</code> or <code>"endpoint"</code>.
  required Recipient recipient;

  // The request type.
  required RequestType requestType;

  // The <code>bRequest</code> field, see <i>Universal Serial Bus
  // Specification Revision 1.1</i> &sect; 9.3.
  required long request;
  // The <code>wValue</code> field, see <i>Ibid</i>.
  required long value;
  // The <code>wIndex</code> field, see <i>Ibid</i>.
  required long index;

  // The maximum number of bytes to receive (required only by input
  // transfers).
  long length;

  // The data to transmit (required only by output transfers).
  ArrayBuffer data;

  // Request timeout (in milliseconds). The default value <code>0</code>
  // indicates no timeout.
  long timeout;
};

dictionary GenericTransferInfo {
  // The transfer direction (<code>"in"</code> or <code>"out"</code>).
  required Direction direction;

  // The target endpoint address. The interface containing this endpoint must
  // be claimed.
  required long endpoint;

  // The maximum number of bytes to receive (required only by input
  // transfers).
  long length;

  // The data to transmit (required only by output transfers).
  ArrayBuffer data;

  // Request timeout (in milliseconds). The default value <code>0</code>
  // indicates no timeout.
  long timeout;
};

dictionary IsochronousTransferInfo {
  // Transfer parameters. The transfer length or data buffer specified in this
  // parameter block is split along <code>packetLength</code> boundaries to
  // form the individual packets of the transfer.
  required GenericTransferInfo transferInfo;

  // The total number of packets in this transfer.
  required long packets;

  // The length of each of the packets in this transfer.
  required long packetLength;
};

dictionary TransferResultInfo {
  // A value of <code>0</code> indicates that the transfer was a success.
  // Other values indicate failure.
  long resultCode;

  // The data returned by an input transfer. <code>undefined</code> for output
  // transfers.
  ArrayBuffer data;
};

dictionary DeviceFilter {
  // Device vendor ID.
  long vendorId;
  // Device product ID, checked only if the vendor ID matches.
  long productId;
  // USB interface class, matches any interface on the device.
  long interfaceClass;
  // USB interface sub-class, checked only if the interface class matches.
  long interfaceSubclass;
  // USB interface protocol, checked only if the interface sub-class matches.
  long interfaceProtocol;
};

dictionary EnumerateDevicesOptions {
  [deprecated="Equivalent to setting $(ref:DeviceFilter.vendorId)."]
  long vendorId;
  [deprecated="Equivalent to setting $(ref:DeviceFilter.productId)."]
  long productId;
  // A device matching any given filter will be returned. An empty filter list
  // will return all devices the app has permission for.
  sequence<DeviceFilter> filters;
};

dictionary EnumerateDevicesAndRequestAccessOptions {
  // The device vendor ID.
  required long vendorId;
  // The product ID.
  required long productId;
  // The interface ID to request access to.
  // Only available on Chrome OS. It has no effect on other platforms.
  long interfaceId;
};

dictionary DevicePromptOptions {
  // Allow the user to select multiple devices.
  boolean multiple;
  // Filter the list of devices presented to the user. If multiple filters are
  // provided devices matching any filter will be displayed.
  sequence<DeviceFilter> filters;
};

callback OnDeviceAddedListener = undefined (Device device);

interface OnDeviceAddedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceAddedListener listener);
  static undefined removeListener(OnDeviceAddedListener listener);
  static boolean hasListener(OnDeviceAddedListener listener);
};

callback OnDeviceRemovedListener = undefined (Device device);

interface OnDeviceRemovedEvent : ExtensionEvent {
  static undefined addListener(OnDeviceRemovedListener listener);
  static undefined removeListener(OnDeviceRemovedListener listener);
  static boolean hasListener(OnDeviceRemovedListener listener);
};

// Use the <code>chrome.usb</code> API to interact with connected USB
// devices. This API provides access to USB operations from within the context
// of an app. Using this API, apps can function as drivers for hardware devices.
// Errors generated by this API are reported by setting
// $(ref:runtime.lastError) and executing the function's regular callback. The
// callback's regular parameters will be undefined in this case.
interface Usb {
  // Enumerates connected USB devices.
  // |options|: The properties to search for on target devices.
  // |PromiseValue|: devices
  [requiredCallback] static Promise<sequence<Device>> getDevices(
      EnumerateDevicesOptions options);

  // Presents a device picker to the user and returns the $(ref:Device)s
  // selected.
  // If the user cancels the picker devices will be empty. A user gesture
  // is required for the dialog to display. Without a user gesture, the
  // callback will run as though the user cancelled.
  // |options|: Configuration of the device picker dialog box.
  // |Returns|: Invoked with a list of chosen $(ref:Device)s.
  // |PromiseValue|: devices
  [requiredCallback] static Promise<sequence<Device>> getUserSelectedDevices(
      DevicePromptOptions options);

  // Returns the full set of device configuration descriptors.
  // |device|: The $(ref:Device) to fetch descriptors from.
  // |PromiseValue|: configs
  [requiredCallback]
  static Promise<sequence<ConfigDescriptor>> getConfigurations(
      Device device);

  // Requests access from the permission broker to a device claimed by
  // Chrome OS if the given interface on the device is not claimed.
  //
  // |device|: The $(ref:Device) to request access to.
  // |interfaceId|: The particular interface requested.
  // |PromiseValue|: success
  [deprecated="This function was Chrome OS specific and calling it on other
    platforms would fail. This operation is now implicitly performed as part of
    $(ref:openDevice) and this function will return <code>true</code> on all
    platforms.", requiredCallback]
  static Promise<boolean> requestAccess(Device device,
                            long interfaceId);

  // Opens a USB device returned by $(ref:getDevices).
  // |device|: The $(ref:Device) to open.
  // |PromiseValue|: handle
  [requiredCallback] static Promise<ConnectionHandle> openDevice(
      Device device);

  // Finds USB devices specified by the vendor, product and (optionally)
  // interface IDs and if permissions allow opens them for use.
  //
  // If the access request is rejected or the device fails to be opened a
  // connection handle will not be created or returned.
  //
  // Calling this method is equivalent to calling $(ref:getDevices) followed
  // by $(ref:openDevice) for each device.
  //
  // |options|: The properties to search for on target devices.
  // |PromiseValue|: handles
  [requiredCallback] static Promise<sequence<ConnectionHandle>> findDevices(
      EnumerateDevicesAndRequestAccessOptions options);

  // Closes a connection handle. Invoking operations on a handle after it
  // has been closed is a safe operation but causes no action to be taken.
  // |handle|: The $(ref:ConnectionHandle) to close.
  static Promise<undefined> closeDevice(
      ConnectionHandle handle);

  // Select a device configuration.
  //
  // This function effectively resets the device by selecting one of the
  // device's available configurations. Only configuration values greater
  // than <code>0</code> are valid however some buggy devices have a working
  // configuration <code>0</code> and so this value is allowed.
  // |handle|: An open connection to the device.
  [requiredCallback] static Promise<undefined> setConfiguration(
      ConnectionHandle handle,
      long configurationValue);

  // Gets the configuration descriptor for the currently selected
  // configuration.
  // |handle|: An open connection to the device.
  // |PromiseValue|: config
  [requiredCallback] static Promise<ConfigDescriptor> getConfiguration(
      ConnectionHandle handle);

  // Lists all interfaces on a USB device.
  // |handle|: An open connection to the device.
  // |PromiseValue|: descriptors
  [requiredCallback]
  static Promise<sequence<InterfaceDescriptor>> listInterfaces(
      ConnectionHandle handle);

  // Claims an interface on a USB device.
  // Before data can be transfered to an interface or associated endpoints the
  // interface must be claimed. Only one connection handle can claim an
  // interface at any given time. If the interface is already claimed, this
  // call will fail.
  //
  // $(ref:releaseInterface) should be called when the interface is no longer
  // needed.
  //
  // |handle|: An open connection to the device.
  // |interfaceNumber|: The interface to be claimed.
  [requiredCallback] static Promise<undefined> claimInterface(
      ConnectionHandle handle,
      long interfaceNumber);

  // Releases a claimed interface.
  // |handle|: An open connection to the device.
  // |interfaceNumber|: The interface to be released.
  [requiredCallback] static Promise<undefined> releaseInterface(
      ConnectionHandle handle,
      long interfaceNumber);

  // Selects an alternate setting on a previously claimed interface.
  // |handle|: An open connection to the device where this interface has been
  //     claimed.
  // |interfaceNumber|: The interface to configure.
  // |alternateSetting|: The alternate setting to configure.
  [requiredCallback] static Promise<undefined> setInterfaceAlternateSetting(
      ConnectionHandle handle,
      long interfaceNumber,
      long alternateSetting);

  // Performs a control transfer on the specified device.
  //
  // Control transfers refer to either the device, an interface or an
  // endpoint. Transfers to an interface or endpoint require the interface to
  // be claimed.
  //
  // |handle|: An open connection to the device.
  // |PromiseValue|: info
  [requiredCallback] static Promise<TransferResultInfo> controlTransfer(
      ConnectionHandle handle,
      ControlTransferInfo transferInfo);

  // Performs a bulk transfer on the specified device.
  // |handle|: An open connection to the device.
  // |transferInfo|: The transfer parameters.
  // |PromiseValue|: info
  [requiredCallback] static Promise<TransferResultInfo> bulkTransfer(
      ConnectionHandle handle,
      GenericTransferInfo transferInfo);

  // Performs an interrupt transfer on the specified device.
  // |handle|: An open connection to the device.
  // |transferInfo|: The transfer parameters.
  // |PromiseValue|: info
  [requiredCallback] static Promise<TransferResultInfo> interruptTransfer(
      ConnectionHandle handle,
      GenericTransferInfo transferInfo);

  // Performs an isochronous transfer on the specific device.
  // |handle|: An open connection to the device.
  // |PromiseValue|: info
  [requiredCallback] static Promise<TransferResultInfo> isochronousTransfer(
      ConnectionHandle handle,
      IsochronousTransferInfo transferInfo);

  // Tries to reset the USB device.
  // If the reset fails, the given connection handle will be closed and the
  // USB device will appear to be disconnected then reconnected.
  // In this case $(ref:getDevices) or $(ref:findDevices) must be called again
  // to acquire the device.
  //
  // |handle|: A connection handle to reset.
  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean> resetDevice(
      ConnectionHandle handle);

  // Event generated when a device is added to the system. Events are only
  // broadcast to apps and extensions that have permission to access the
  // device. Permission may have been granted at install time, when the user
  // accepted an optional permission (see $(ref:permissions.request)), or
  // through $(ref:getUserSelectedDevices).
  static attribute OnDeviceAddedEvent onDeviceAdded;

  // Event generated when a device is removed from the system. See
  // $(ref:onDeviceAdded) for which events are delivered.
  static attribute OnDeviceRemovedEvent onDeviceRemoved;
};

partial interface Browser {
  static attribute Usb usb;
};
