// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothAdapter;
class BluetoothGattConnection;
class BluetoothSocket;

// BluetoothDevice represents a remote Bluetooth device, both its properties and
// capabilities as discovered by a local adapter and actions that may be
// performed on the remove device such as pairing, connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the GetAddress() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class DEVICE_BLUETOOTH_EXPORT BluetoothDevice {
 public:
  // Possible values that may be returned by GetVendorIDSource(),
  // indicating different organisations that allocate the identifiers returned
  // by GetVendorID().
  enum VendorIDSource {
    VENDOR_ID_UNKNOWN,
    VENDOR_ID_BLUETOOTH,
    VENDOR_ID_USB,
    VENDOR_ID_MAX_VALUE = VENDOR_ID_USB
  };

  // Possible values that may be returned by GetAddressType().
  enum AddressType {
    ADDR_TYPE_UNKNOWN,
    ADDR_TYPE_PUBLIC,
    ADDR_TYPE_RANDOM,
  };

  // The value returned if the RSSI or transmit power cannot be read.
  static const int kUnknownPower = 127;
  // The value returned if the appearance is not present.
  static const uint16_t kAppearanceNotPresent = 0xffc0;

  struct DEVICE_BLUETOOTH_EXPORT ConnectionInfo {
    int rssi;
    int transmit_power;
    int max_transmit_power;

    ConnectionInfo();
    ConnectionInfo(int rssi, int transmit_power, int max_transmit_power);
    ~ConnectionInfo();
  };

  // Possible connection latency values to pass to SetConnectionLatency().
  enum ConnectionLatency {
    CONNECTION_LATENCY_LOW,
    CONNECTION_LATENCY_MEDIUM,
    CONNECTION_LATENCY_HIGH,
  };

  // Possible errors passed back to an error callback function in case of a
  // failed call to Connect().
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This enum should be kept in sync
  // with the BluetoothDeviceConnectErrorCode enum in
  // src/tools/metrics/histograms/metadata/bluetooth/enums.xml.
  enum ConnectErrorCode {
    ERROR_AUTH_CANCELED = 0,
    ERROR_AUTH_FAILED = 1,
    ERROR_AUTH_REJECTED = 2,
    ERROR_AUTH_TIMEOUT = 3,
    ERROR_FAILED = 4,
    ERROR_INPROGRESS = 5,
    ERROR_UNKNOWN = 6,
    ERROR_UNSUPPORTED_DEVICE = 7,
    ERROR_DEVICE_NOT_READY = 8,
    ERROR_ALREADY_CONNECTED = 9,
    ERROR_DEVICE_ALREADY_EXISTS = 10,
    ERROR_DEVICE_UNCONNECTED = 11,
    ERROR_DOES_NOT_EXIST = 12,
    ERROR_INVALID_ARGS = 13,
    ERROR_NON_AUTH_TIMEOUT = 14,
    ERROR_NO_MEMORY = 15,
    ERROR_JNI_ENVIRONMENT = 16,
    ERROR_JNI_THREAD_ATTACH = 17,
    ERROR_WAKELOCK = 18,
    ERROR_UNEXPECTED_STATE = 19,
    ERROR_SOCKET = 20,
    NUM_CONNECT_ERROR_CODES,  // Keep as last enum.
  };

  // Possible battery types that this device could have information for.
  enum class BatteryType {
    // Used for devices who have a single battery.
    kDefault,
    // The left bud on a True Wireless device.
    kLeftBudTrueWireless,
    // The right bud on a True Wireless device.
    kRightBudTrueWireless,
    // The True Wireless device case.
    kCaseTrueWireless,
  };

  struct DEVICE_BLUETOOTH_EXPORT BatteryInfo {
    enum class ChargeState {
      kUnknown,
      kCharging,
      kDischarging,
    };

    BatteryType type;
    std::optional<uint8_t> percentage;
    ChargeState charge_state;

    BatteryInfo();
    BatteryInfo(BatteryType type, std::optional<uint8_t> percentage);
    BatteryInfo(BatteryType type,
                std::optional<uint8_t> percentage,
                ChargeState charge_state);
    BatteryInfo(const BatteryInfo&);
    BatteryInfo& operator=(const BatteryInfo&);
    BatteryInfo(BatteryInfo&&);
    BatteryInfo& operator=(BatteryInfo&&);
    ~BatteryInfo();
    bool operator==(const BatteryInfo& other);
  };

  typedef std::vector<BluetoothUUID> UUIDList;
  typedef base::flat_set<BluetoothUUID> UUIDSet;
  typedef std::unordered_map<BluetoothUUID,
                             std::vector<uint8_t>,
                             BluetoothUUIDHash>
      ServiceDataMap;
  typedef uint16_t ManufacturerId;
  typedef std::vector<uint8_t> ManufacturerData;
  typedef std::unordered_map<ManufacturerId, ManufacturerData>
      ManufacturerDataMap;
  typedef std::unordered_set<ManufacturerId> ManufacturerIDSet;

  // Mapping from the platform-specific GATT service identifiers to
  // BluetoothRemoteGattService objects.
  typedef std::unordered_map<std::string,
                             std::unique_ptr<BluetoothRemoteGattService>>
      GattServiceMap;

  // Interface for negotiating pairing of bluetooth devices.
  class PairingDelegate {
   public:
    virtual ~PairingDelegate() {}

    // This method will be called when the Bluetooth daemon requires a
    // PIN Code for authentication of the device |device|, the delegate should
    // obtain the code from the user and call SetPinCode() on the device to
    // provide it, or RejectPairing() or CancelPairing() to reject or cancel
    // the request.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    virtual void RequestPinCode(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires a
    // Passkey for authentication of the device |device|, the delegate should
    // obtain the passkey from the user (a numeric in the range 0-999999) and
    // call SetPasskey() on the device to provide it, or RejectPairing() or
    // CancelPairing() to reject or cancel the request.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing.
    virtual void RequestPasskey(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the PIN code |pincode| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.0 and earlier keyboard devices, the
    // |pincode| will always be a six-digit numeric in the range 000000-999999
    // for compatibility with later specifications.
    virtual void DisplayPinCode(BluetoothDevice* device,
                                const std::string& pincode) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the Passkey |passkey| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.1 and later devices that support input
    // but not display, such as keyboards. The Passkey is a numeric in the
    // range 0-999999 and should be always presented zero-padded to six
    // digits.
    virtual void DisplayPasskey(BluetoothDevice* device, uint32_t passkey) = 0;

    // This method will be called when the Bluetooth daemon gets a notification
    // of a key entered on the device |device| while pairing with the device
    // using a PIN code or a Passkey.
    //
    // This method will be called only after DisplayPinCode() or
    // DisplayPasskey() method is called, but is not warranted to be called
    // on every pairing process that requires a PIN code or a Passkey because
    // some device may not support this feature.
    //
    // The |entered| value describes the number of keys entered so far,
    // including the last [enter] key. A first call to KeysEntered() with
    // |entered| as 0 will be sent when the device supports this feature.
    virtual void KeysEntered(BluetoothDevice* device, uint32_t entered) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the Passkey |passkey| is displayed on the screen
    // of the device |device| so that it may be authenticated. The delegate
    // should display to the user and ask for confirmation, then call
    // ConfirmPairing() on the device to confirm, RejectPairing() on the device
    // to reject or CancelPairing() on the device to cancel authentication
    // for any other reason.
    //
    // This is used for Bluetooth 2.1 and later devices that support display,
    // such as other computers or phones. The Passkey is a numeric in the
    // range 0-999999 and should be always present zero-padded to six
    // digits.
    virtual void ConfirmPasskey(BluetoothDevice* device, uint32_t passkey) = 0;

    // This method will be called when the Bluetooth daemon requires that a
    // pairing request, usually only incoming, using the just-works model is
    // authorized. The delegate should decide whether the user should confirm
    // or not, then call ConfirmPairing() on the device to confirm the pairing
    // (whether by user action or by default), RejectPairing() on the device to
    // reject or CancelPairing() on the device to cancel authorization for
    // any other reason.
    virtual void AuthorizePairing(BluetoothDevice* device) = 0;
  };

  BluetoothDevice(const BluetoothDevice&) = delete;
  BluetoothDevice& operator=(const BluetoothDevice&) = delete;

  virtual ~BluetoothDevice();

  // Clamps numbers less than -128 to -128 and numbers greater than 127 to 127.
  static int8_t ClampPower(int power);

  // Returns the Bluetooth class of the device, used by GetDeviceType()
  // and metrics logging,
  virtual uint32_t GetBluetoothClass() const = 0;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Returns the transport type of the device. Some devices only support one
  // of BR/EDR or LE, and some support both.
  virtual BluetoothTransport GetType() const = 0;
#endif

  // Returns the identifier of the bluetooth device.
  virtual std::string GetIdentifier() const;

  // Returns the Bluetooth address of the device. This should be used as
  // a unique key to identify the device and copied where needed.
  virtual std::string GetAddress() const = 0;

  // Returns the OUI portion of the Bluetooth address, which refers to the
  // device's vendor.
  std::string GetOuiPortionOfBluetoothAddress() const;

  // Returns the Bluetooth address type of the device. Currently available on
  // Linux and Chrome OS.
  virtual AddressType GetAddressType() const = 0;

  // Returns the allocation source of the identifier returned by GetVendorID(),
  // where available, or VENDOR_ID_UNKNOWN where not.
  virtual VendorIDSource GetVendorIDSource() const = 0;

  // Returns the Vendor ID of the device, where available.
  virtual uint16_t GetVendorID() const = 0;

  // Returns the Product ID of the device, where available.
  virtual uint16_t GetProductID() const = 0;

  // Returns the Device ID of the device, typically the release or version
  // number in BCD format, where available.
  virtual uint16_t GetDeviceID() const = 0;

  // Returns the appearance of the device.
  virtual uint16_t GetAppearance() const = 0;

  // Returns the name of the device, which may be empty.
  virtual std::optional<std::string> GetName() const = 0;

  // Returns the name of the device suitable for displaying, this may
  // be a synthesized string containing the address and localized type name
  // if the device has no obtained name.
  virtual std::u16string GetNameForDisplay() const;

  // Returns the type of the device, limited to those we support or are
  // aware of, by decoding the bluetooth class information. The returned
  // values are unique, and do not overlap, so DEVICE_KEYBOARD is not also
  // DEVICE_PERIPHERAL.
  //
  // Returns the type of the device, limited to those we support or are aware
  // of, by decoding the bluetooth class information for Classic devices or
  // by decoding the device's appearance for LE devices. For example,
  // Microsoft Universal Foldable Keyboard only advertises the appearance.
  virtual BluetoothDeviceType GetDeviceType() const;

  // Indicates whether the device is known to support pairing based on its
  // device class and address.
  bool IsPairable() const;

  // Indicates whether the device is paired with the adapter.
  virtual bool IsPaired() const = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Indicates whether the device is bonded with the adapter.
  virtual bool IsBonded() const = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Indicates whether the device is currently connected to the adapter.
  // Note that if IsConnected() is true, does not imply that the device is
  // connected to any application or service. If the device is not paired, it
  // could be still connected to the adapter for other reason, for example, to
  // request the adapter's SDP records. The same holds for paired devices, since
  // they could be connected to the adapter but not to an application.
  virtual bool IsConnected() const = 0;

  // Indicates whether an active GATT connection exists to the device.
  virtual bool IsGattConnected() const = 0;

  // Indicates whether the paired device accepts connections initiated from the
  // adapter. This value is undefined for unpaired devices. Only available for
  // Chrome OS.
  virtual bool IsConnectable() const = 0;

  // Indicates whether there is a call to Connect() ongoing. For this attribute,
  // we consider a call is ongoing if none of the callbacks passed to Connect()
  // were called after the corresponding call to Connect().
  virtual bool IsConnecting() const = 0;

  // Returns the set of UUIDs that this device supports.
  //  * For classic Bluetooth devices this data is collected from both the EIR
  //    data and SDP tables.
  //  * For non-connected and connected Low Energy Devices for which services
  //    have not been discovered returns the latest advertised UUIDs.
  //  * For connected Low Energy Devices for which services have been discovered
  //    returns the UUIDs of the device's services and the latest advertised
  //    UUIDs.
  //  * For dual mode devices this may be collected from both.
  //
  // Note: On Android, Mac and WinRT advertised UUIDs are cleared when the
  // adapter stops discovering, as otherwise stale data might be returned.
  //
  // Note: On ChromeOS and Linux, BlueZ persists all services meaning if
  // a device stops advertising a service this function will still return
  // its UUID.
  virtual UUIDSet GetUUIDs() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Sets if this device is blocked by admin policy.
  void SetIsBlockedByPolicy(bool);
  bool IsBlockedByPolicy() const;
#endif

  // Returns the last advertised Service Data. Returns an empty map if the
  // adapter is not discovering.
  //
  // Note: On ChromeOS and Linux, BlueZ persists all service data meaning if
  // a device stops advertising service data for a UUID, this function will
  // still return the cached value for that UUID.
  const ServiceDataMap& GetServiceData() const;

  // Returns the UUIDs of services for which the device advertises Service Data.
  // Returns an empty set if the adapter is not discovering.
  UUIDSet GetServiceDataUUIDs() const;

  // Returns a pointer to the Service Data for Service with |uuid|. Returns
  // nullptr if |uuid| has no Service Data.
  const std::vector<uint8_t>* GetServiceDataForUUID(
      const BluetoothUUID& uuid) const;

  // Returns advertised Manufacturer Data. Keys are 16 bits Manufacturer IDs
  // followed by its byte array value. Returns an empty map if the device
  // does not advertise any Manufacturer Data.
  // Returns cached value if the adapter is not discovering.
  //
  // Note: On ChromeOS and Linux, BlueZ persists all manufacturer data meaning
  // if a device stops advertising manufacturer data for a Manufacturer Id, this
  // function will still return the cached value for that Id.
  //
  // TODO(crbug.com/41284350) Support this on platforms that don't use BlueZ.
  // Only BlueZ supports this now. This method returns an empty map on platforms
  // that don't use BlueZ.
  const ManufacturerDataMap& GetManufacturerData() const;

  // Returns the Manufacturer Data IDs of Manufacturers for which the device
  // advertises Manufacturer Data.
  // Returns cached value if the adapter is not discovering.
  ManufacturerIDSet GetManufacturerDataIDs() const;

  // Returns a pointer to the Manufacturer Data for Manufacturer with
  // |manufacturerID|. Returns nullptr if |manufacturerID| has no Manufacturer
  // Data. Returns cached value if the adapter is not discovering.
  const std::vector<uint8_t>* GetManufacturerDataForID(
      const ManufacturerId manufacturerID) const;

  // The received signal strength, in dBm. This field is avaliable and valid
  // only during discovery.
  // TODO(http://crbug.com/580406): Devirtualize once BlueZ sets inquiry_rssi_.
  virtual std::optional<int8_t> GetInquiryRSSI() const;

  // The transmitted power level. This field is avaliable only for LE devices
  // that include this field in AD. It is avaliable and valid only during
  // discovery.
  // TODO(http://crbug.com/580406): Devirtualize once BlueZ sets
  // inquiry_tx_power_.
  virtual std::optional<int8_t> GetInquiryTxPower() const;

  // Returns Advertising Data Flags.
  // Returns cached value if the adapter is not discovering.
  //
  // Only Chrome OS and WinRT support this now. Upstream BlueZ has this feature
  // as experimental. This method returns std::nullopt on platforms that don't
  // support this feature.
  std::optional<uint8_t> GetAdvertisingDataFlags() const;

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  using ErrorCallback = base::OnceClosure;

  // Reports the status of a device connection attempt. |error_code| will
  // contain a value upon failure, otherwise the attempt was successful.
  using ConnectCallback =
      base::OnceCallback<void(std::optional<ConnectErrorCode> error_code)>;

  using ConnectionInfoCallback =
      base::OnceCallback<void(const ConnectionInfo&)>;

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  virtual bool ExpectingPinCode() const = 0;

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  virtual bool ExpectingPasskey() const = 0;

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  virtual bool ExpectingConfirmation() const = 0;

  // Returns the RSSI and TX power of the active connection to the device:
  //
  // The RSSI indicates the power present in the received radio signal, measured
  // in dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // The transmit power indicates the strength of the signal broadcast from the
  // host's Bluetooth antenna when communicating with the device, measured in
  // dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // If the device isn't connected, then the ConnectionInfo struct passed into
  // the callback will be populated with |kUnknownPower|.
  virtual void GetConnectionInfo(ConnectionInfoCallback callback) = 0;

  // Sets the connection latency for the device. This API is only valid for LE
  // devices.
  virtual void SetConnectionLatency(ConnectionLatency connection_latency,
                                    base::OnceClosure callback,
                                    ErrorCallback error_callback) = 0;

  // Initiates a connection to the device, pairing first if necessary.
  //
  // Method calls will be made on the supplied object |pairing_delegate|
  // to indicate what display, and in response should make method calls
  // back to the device object. Not all devices require user responses
  // during pairing, so it is normal for |pairing_delegate| to receive no
  // calls. To explicitly force a low-security connection without bonding,
  // pass nullptr, though this is ignored if the device is already paired.
  //
  // |callback| will be called with the status of the connection attempt.
  // After calling Connect, CancelPairing should be called to cancel the pairing
  // process and release the pairing delegate if user cancels the pairing and
  // closes the pairing UI.
  virtual void Connect(PairingDelegate* pairing_delegate,
                       ConnectCallback callback) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Initiates a classic connection to the device, pairing first if necessary.
  //
  // Method calls will be made on the supplied object |pairing_delegate|
  // to indicate what display, and in response should make method calls
  // back to the device object. Not all devices require user responses
  // during pairing, so it is normal for |pairing_delegate| to receive no
  // calls. To explicitly force a low-security connection without bonding,
  // pass nullptr, though this is ignored if the device is already paired.
  //
  // |callback| will be called with the status of the connection attempt.  After
  // calling ConnectClassic, CancelPairing should be called to cancel the
  // pairing process and release the pairing delegate if user cancels the
  // pairing and closes the pairing UI.
  virtual void ConnectClassic(PairingDelegate* pairing_delegate,
                              ConnectCallback callback) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Pairs the device. This method triggers pairing unconditially, i.e. it
  // ignores the |IsPaired()| value.
  //
  // In most cases |Connect()| should be preferred. This method is only
  // implemented on ChromeOS, Linux and Windows 10. On Windows, only pairing
  // with a pin code is currently supported.
  virtual void Pair(PairingDelegate* pairing_delegate,
                    ConnectCallback callback);

  // Sends the PIN code |pincode| to the remote device during pairing.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void SetPinCode(const std::string& pincode) = 0;

  // Sends the Passkey |passkey| to the remote device during pairing.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing, and are a numeric in the range 0-999999.
  virtual void SetPasskey(uint32_t passkey) = 0;

  // Confirms to the remote device during pairing that a passkey provided by
  // the ConfirmPasskey() delegate call is displayed on both devices.
  virtual void ConfirmPairing() = 0;

  // Rejects a pairing or connection request from a remote device.
  virtual void RejectPairing() = 0;

  // Cancels a pairing or connection attempt to a remote device, releasing
  // the pairing delegate.
  virtual void CancelPairing() = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it. Link keys and other pairing
  // information are not discarded, and the device object is not deleted.
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  virtual void Disconnect(base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it, and then discards link keys
  // and other pairing information. The device object remains valid until
  // returning from the calling function, after which it should be assumed to
  // have been deleted. If the request fails, |error_callback| will be called.
  // On success |callback| will be invoked, but note that the BluetoothDevice
  // object will have been deleted at that point.
  virtual void Forget(base::OnceClosure callback,
                      ErrorCallback error_callback) = 0;

  // Attempts to initiate an outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. |callback| will be called on a successful connection with a
  // BluetoothSocket instance that is to be owned by the receiver.
  // |error_callback| will be called on failure with a message indicating the
  // cause.
  using ConnectToServiceCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothSocket>)>;
  using ConnectToServiceErrorCallback =
      base::OnceCallback<void(const std::string& message)>;
  virtual void ConnectToService(
      const BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) = 0;

  // Attempts to initiate an insecure outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. Unlike ConnectToService, the outgoing connection will request
  // no bonding rather than general bonding. |callback| will be called on a
  // successful connection with a BluetoothSocket instance that is to be owned
  // by the receiver. |error_callback| will be called on failure with a message
  // indicating the cause.
  virtual void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) = 0;

  // Opens a new GATT connection to this device. On success, |callback| will
  // be called with a valid BluetoothGattConnection and |error_code| will have
  // no value. On error, |callback| will be called with a null connection and
  // a valid |error_code|. The connection will be kept alive,
  // as long as there is at least one active GATT connection. In the case that
  // the underlying connection gets terminated, either due to a call to
  // BluetoothDevice::Disconnect or other unexpected circumstances, the
  // returned BluetoothGattConnection will be automatically marked as inactive.
  // To monitor the state of the connection, observe the
  // BluetoothAdapter::Observer::DeviceChanged method.
  //
  // If |service_uuid| is given, potentially only the service with the given
  // UUID will be discovered. This may speed up GATT discovery times if the
  // platform can take advantage of this optimisation. Note that passing
  // |service_uuid| may cause full GATT service discovery to be skipped. In that
  // case, |IsGattServicesDiscoveryComplete| will not become true but
  // |BluetoothAdapter::Observer::GattServicesDiscovered| is still the correct
  // event to watch for.
  using GattConnectionCallback =
      base::OnceCallback<void(std::unique_ptr<BluetoothGattConnection>,
                              std::optional<ConnectErrorCode> error_code)>;
  virtual void CreateGattConnection(
      GattConnectionCallback callback,
      std::optional<BluetoothUUID> service_uuid = std::nullopt);

  // Set the gatt services discovery complete flag for this device.
  virtual void SetGattServicesDiscoveryComplete(bool complete);

  // Indicates whether full service discovery is complete for this device. If a
  // |service_uuid| was passed to |CreateGattConnection| and a focused discovery
  // was performed, then this will continue to return false even after discovery
  // is complete.
  virtual bool IsGattServicesDiscoveryComplete() const;

  // Returns the list of discovered GATT services.
  virtual std::vector<BluetoothRemoteGattService*> GetGattServices() const;

  // Returns the GATT service with device-specific identifier |identifier|.
  // Returns nullptr, if no such service exists.
  virtual BluetoothRemoteGattService* GetGattService(
      const std::string& identifier) const;

  // Update the last time this device was seen.
  void UpdateTimestamp();

  // Returns the time of the last call to UpdateTimestamp(), or base::Time() if
  // it hasn't been called yet.
  virtual base::Time GetLastUpdateTime() const;

#if BUILDFLAG(IS_APPLE)
  // Returns true if this device is a Low Energy device.
  virtual bool IsLowEnergyDevice() = 0;
#endif  // BUILDFLAG(IS_APPLE)

  // Called by BluetoothAdapter when a new Advertisement is seen for this
  // device. This replaces previously seen Advertisement Data. The order of
  // arguments matches the order of their corresponding Data Type specified in
  // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile.
  void UpdateAdvertisementData(int8_t rssi,
                               std::optional<uint8_t> flags,
                               UUIDList advertised_uuids,
                               std::optional<int8_t> tx_power,
                               ServiceDataMap service_data,
                               ManufacturerDataMap manufacturer_data);

  // Called by BluetoothAdapter when it stops discoverying.
  void ClearAdvertisementData();

  // Return associated BluetoothAdapter.
  BluetoothAdapter* GetAdapter() { return adapter_; }

  std::vector<BluetoothRemoteGattService*> GetPrimaryServices();

  std::vector<BluetoothRemoteGattService*> GetPrimaryServicesByUUID(
      const BluetoothUUID& service_uuid);

#if BUILDFLAG(IS_CHROMEOS)
  using ExecuteWriteErrorCallback =
      base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>;
  using AbortWriteErrorCallback =
      base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>;
  // Executes all the previous prepare writes in a reliable write session.
  virtual void ExecuteWrite(base::OnceClosure callback,
                            ExecuteWriteErrorCallback error_callback) = 0;
  // Aborts all the previous prepare writes in a reliable write session.
  virtual void AbortWrite(base::OnceClosure callback,
                          AbortWriteErrorCallback error_callback) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Set the battery information for the battery type |info.type|. Overrides
  // previously set value (if any).
  void SetBatteryInfo(const BatteryInfo& info);
  // Removes the battery information associated with |type|.
  // Returns true if removed, otherwise false.
  bool RemoveBatteryInfo(const BatteryType& type);
  std::optional<BatteryInfo> GetBatteryInfo(const BatteryType& type) const;
  // Returns the list of currently set BatteryTypes.
  std::vector<BatteryType> GetAvailableBatteryTypes();
#endif

  // Returns whether this device supports discovering specific services, i.e.
  // whether the |service_uuid| argument to |CreateGattConnection| is
  // meaningful. This should only be called by tests. Non-test code should
  // optimistically pass a |service_uuid| argument if appropriate for the need.
  bool supports_service_specific_discovery() const;

 protected:
  // BluetoothGattConnection is a friend to call Add/RemoveGattConnection.
  friend BluetoothGattConnection;
  FRIEND_TEST_ALL_PREFIXES(BluetoothDeviceTest, GattConnectionErrorReentrancy);
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTest,
      BluetoothGattConnection_DisconnectGatt_SimulateConnect);
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTest,
      BluetoothGattConnection_DisconnectGatt_SimulateDisconnect);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest,
                           BluetoothGattConnection_ErrorAfterConnection);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest,
                           BluetoothGattConnection_DisconnectGatt_Cleanup);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, GetName_NullName);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, RemoveOutdatedDevices);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTest, RemoveOutdatedDeviceGattConnect);

  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTestWinrt,
      BluetoothGattConnection_DisconnectGatt_SimulateConnect);
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothTestWinrt,
      BluetoothGattConnection_DisconnectGatt_SimulateDisconnect);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTestWinrt,
                           BluetoothGattConnection_ErrorAfterConnection);
  FRIEND_TEST_ALL_PREFIXES(BluetoothTestWinrt,
                           BluetoothGattConnection_DisconnectGatt_Cleanup);

  // Helper class to easily update the sets of UUIDs and keep them in sync with
  // the set of all the device's UUIDs.
  class DEVICE_BLUETOOTH_EXPORT DeviceUUIDs {
   public:
    DeviceUUIDs();
    ~DeviceUUIDs();

    DeviceUUIDs(const DeviceUUIDs& other);
    DeviceUUIDs& operator=(const DeviceUUIDs& other);

    // Advertised Service UUIDs functions
    void ReplaceAdvertisedUUIDs(UUIDList new_advertised_uuids);

    void ClearAdvertisedUUIDs();

    // Service UUIDs functions
    void ReplaceServiceUUIDs(
        const BluetoothDevice::GattServiceMap& gatt_services);

    void ReplaceServiceUUIDs(UUIDList new_service_uuids);

    void ClearServiceUUIDs();

    // Returns the union of Advertised UUIDs and Service UUIDs.
    const UUIDSet& GetUUIDs() const;

   private:
    void UpdateDeviceUUIDs();

    BluetoothDevice::UUIDSet advertised_uuids_;
    BluetoothDevice::UUIDSet service_uuids_;
    BluetoothDevice::UUIDSet device_uuids_;
  };

  explicit BluetoothDevice(BluetoothAdapter* adapter);

  // Implements platform specific operations to initiate a GATT connection.
  // Subclasses must also call DidConnectGatt or DidDisconnectGatt immediately
  // or asynchronously as the connection state changes.
  virtual void CreateGattConnectionImpl(
      std::optional<BluetoothUUID> service_uuid) = 0;

  // UpgradeToFullDiscovery is called when there is a pending or current GATT
  // connection that was created with a service UUID, but now discovery of all
  // services is required because of a new connection request. This will only
  // be called if the subclass sets |supports_service_specific_discovery_|.
  virtual void UpgradeToFullDiscovery();

  // Disconnects GATT connection on platforms that maintain a specific GATT
  // connection.
  virtual void DisconnectGatt() = 0;

  // Returns a |BluetoothGattConnection| object that represents a reference to a
  // GATT connection to this device.
  virtual std::unique_ptr<BluetoothGattConnection>
  CreateBluetoothGattConnectionObject();

  // Calls any pending callbacks for CreateGattConnection based on result of
  // subclasses actions initiated in CreateGattConnectionImpl or related
  // disconnection events. These may be called at any time, even multiple times,
  // to ensure a change in platform state is correctly tracked.
  //
  // Under normal behavior it is expected that after CreateGattConnectionImpl
  // a platform will call DidConnectGatt but not DidDisconnectGatt.
  void DidConnectGatt(std::optional<ConnectErrorCode> error_code);
  void DidDisconnectGatt();

  // Tracks BluetoothGattConnection instances that act as a reference count
  // keeping the GATT connection open. Instances call Add/RemoveGattConnection
  // at creation & deletion.
  void AddGattConnection(BluetoothGattConnection*);
  void RemoveGattConnection(BluetoothGattConnection*);

  // Update last_update_time_ so that the device appears as expired.
  void SetAsExpiredForTesting();

  // Raw pointer to adapter owning this device object. Subclasses use platform
  // specific pointers via adapter_.
  // This field is not a raw_ptr<> because problems related to passing to a
  // templated && parameter, which is later forwarded to something that doesn't
  // vibe with raw_ptr<T>.
  RAW_PTR_EXCLUSION BluetoothAdapter* const adapter_;

  // Indicates whether this device supports limited discovery of a specific
  // service. This is configured by the constructor of subclasses. If false,
  // the UUID argument to |CreateGattConnection| is ignored.
  bool supports_service_specific_discovery_ = false;

  // Contains the specified service that was targeted for discovery. Only ever
  // contains a value if |supports_service_specific_discovery_| is true.
  std::optional<BluetoothUUID> target_service_;

  // Callbacks for result of CreateGattConnection.
  std::vector<GattConnectionCallback> create_gatt_connection_callbacks_;

  // BluetoothGattConnection objects keeping the GATT connection alive.
  std::set<raw_ptr<BluetoothGattConnection, SetExperimental>> gatt_connections_;

  GattServiceMap gatt_services_;
  bool gatt_services_discovery_complete_;

  // Received Signal Strength Indicator of the advertisement received.
  std::optional<int8_t> inquiry_rssi_;

  // Advertising Data flags of the device.
  std::optional<uint8_t> advertising_data_flags_;

  // Tx Power advertised by the device.
  std::optional<int8_t> inquiry_tx_power_;

  // Class that holds the union of Advertised UUIDs and Service UUIDs.
  DeviceUUIDs device_uuids_;

  // Map of BluetoothUUIDs to their advertised Service Data.
  ServiceDataMap service_data_;

  // Map of Manufacturer IDs to their advertised Manufacturer Data.
  ManufacturerDataMap manufacturer_data_;

  // Timestamp for when an advertisement was last seen.
  base::Time last_update_time_;

 private:
  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  std::u16string GetAddressWithLocalizedDeviceTypeName() const;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Battery information for the known battery types for this device.
  base::flat_map<BatteryType, BatteryInfo> battery_info_map_;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Indicate whether or not this device is blocked by admin policy. This would
  // be true if any of its auto-connect service does not exist in the
  // ServiceAllowList under org.bluez.AdminPolicyStatus1.
  bool is_blocked_by_policy_ = false;
#endif
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
