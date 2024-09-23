// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_version.h"

namespace floss {

extern DEVICE_BLUETOOTH_EXPORT int kDBusTimeoutMs;
extern DEVICE_BLUETOOTH_EXPORT int kAdapterEnabledTimeoutMs;

// TODO(b/189499077) - Expose via floss package
inline constexpr char kAdapterService[] = "org.chromium.bluetooth";
inline constexpr char kManagerService[] = "org.chromium.bluetooth.Manager";

inline constexpr char kAdapterLoggingObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/logging";
inline constexpr char kAdapterObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/adapter";
inline constexpr char kAdminObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/admin";
inline constexpr char kBatteryManagerObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/battery_manager";
inline constexpr char kBluetoothTelephonyObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/telephony";
inline constexpr char kGattObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/gatt";
inline constexpr char kManagerObject[] = "/org/chromium/bluetooth/Manager";
inline constexpr char kMediaObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/media";

inline constexpr char kAdapterInterface[] = "org.chromium.bluetooth.Bluetooth";
inline constexpr char kAdapterLoggingInterface[] =
    "org.chromium.bluetooth.Logging";
inline constexpr char kAdminInterface[] =
    "org.chromium.bluetooth.BluetoothAdmin";
inline constexpr char kBatteryManagerInterface[] =
    "org.chromium.bluetooth.BatteryManager";
inline constexpr char kBluetoothTelephonyInterface[] =
    "org.chromium.bluetooth.BluetoothTelephony";
inline constexpr char kExperimentalInterface[] =
    "org.chromium.bluetooth.Experimental";
inline constexpr char kGattInterface[] = "org.chromium.bluetooth.BluetoothGatt";
inline constexpr char kManagerInterface[] = "org.chromium.bluetooth.Manager";
inline constexpr char kSocketManagerInterface[] =
    "org.chromium.bluetooth.SocketManager";

namespace adapter {
inline constexpr char kGetAddress[] = "GetAddress";
inline constexpr char kGetName[] = "GetName";
inline constexpr char kSetName[] = "SetName";
inline constexpr char kGetDiscoverable[] = "GetDiscoverable";
inline constexpr char kGetDiscoverableTimeout[] = "GetDiscoverableTimeout";
inline constexpr char kSetDiscoverable[] = "SetDiscoverable";
inline constexpr char kIsLeExtendedAdvertisingSupported[] =
    "IsLeExtendedAdvertisingSupported";
inline constexpr char kStartDiscovery[] = "StartDiscovery";
inline constexpr char kCancelDiscovery[] = "CancelDiscovery";
inline constexpr char kCreateBond[] = "CreateBond";
inline constexpr char kCancelBondProcess[] = "CancelBondProcess";
inline constexpr char kRemoveBond[] = "RemoveBond";
inline constexpr char kGetRemoteType[] = "GetRemoteType";
inline constexpr char kGetRemoteClass[] = "GetRemoteClass";
inline constexpr char kGetRemoteAppearance[] = "GetRemoteAppearance";
inline constexpr char kGetRemoteVendorProductInfo[] =
    "GetRemoteVendorProductInfo";
inline constexpr char kGetRemoteAddressType[] = "GetRemoteAddressType";
inline constexpr char kGetConnectionState[] = "GetConnectionState";
inline constexpr char kGetRemoteUuids[] = "GetRemoteUuids";
inline constexpr char kFetchRemoteUuids[] = "FetchRemoteUuids";
inline constexpr char kGetBondState[] = "GetBondState";
inline constexpr char kConnectAllEnabledProfiles[] =
    "ConnectAllEnabledProfiles";
inline constexpr char kDisconnectAllEnabledProfiles[] =
    "DisconnectAllEnabledProfiles";
inline constexpr char kRegisterCallback[] = "RegisterCallback";
inline constexpr char kUnregisterCallback[] = "UnregisterCallback";
inline constexpr char kRegisterConnectionCallback[] =
    "RegisterConnectionCallback";
inline constexpr char kUnregisterConnectionCallback[] =
    "UnregisterConnectionCallback";
inline constexpr char kRegisterScanner[] = "RegisterScanner";
inline constexpr char kUnregisterScanner[] = "UnregisterScanner";
inline constexpr char kRegisterScannerCallback[] = "RegisterScannerCallback";
inline constexpr char kUnregisterScannerCallback[] =
    "UnregisterScannerCallback";
inline constexpr char kStartScan[] = "StartScan";
inline constexpr char kStopScan[] = "StopScan";
inline constexpr char kSetPairingConfirmation[] = "SetPairingConfirmation";
inline constexpr char kSetPin[] = "SetPin";
inline constexpr char kSetPasskey[] = "SetPasskey";
inline constexpr char kGetBondedDevices[] = "GetBondedDevices";
inline constexpr char kGetConnectedDevices[] = "GetConnectedDevices";
inline constexpr char kSdpSearch[] = "SdpSearch";
inline constexpr char kCreateSdpRecord[] = "CreateSdpRecord";
inline constexpr char kRemoveSdpRecord[] = "RemoveSdpRecord";
inline constexpr char kGetSupportedRoles[] = "GetSupportedRoles";

// TODO(abps) - Rename this to AdapterCallback in platform and here
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothCallback";
inline constexpr char kConnectionCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothConnectionCallback";

inline constexpr char kOnAdapterPropertyChanged[] = "OnAdapterPropertyChanged";
inline constexpr char kOnAddressChanged[] = "OnAddressChanged";
inline constexpr char kOnNameChanged[] = "OnNameChanged";
inline constexpr char kOnDiscoverableChanged[] = "OnDiscoverableChanged";
inline constexpr char kOnDeviceFound[] = "OnDeviceFound";
inline constexpr char kOnDeviceCleared[] = "OnDeviceCleared";
inline constexpr char kOnDevicePropertiesChanged[] =
    "OnDevicePropertiesChanged";
inline constexpr char kOnDiscoveringChanged[] = "OnDiscoveringChanged";
inline constexpr char kOnSspRequest[] = "OnSspRequest";
inline constexpr char kOnPinDisplay[] = "OnPinDisplay";
inline constexpr char kOnPinRequest[] = "OnPinRequest";

inline constexpr char kOnBondStateChanged[] = "OnBondStateChanged";
inline constexpr char kOnSdpSearchComplete[] = "OnSdpSearchComplete";
inline constexpr char kOnSdpRecordCreated[] = "OnSdpRecordCreated";
inline constexpr char kOnDeviceConnected[] = "OnDeviceConnected";
inline constexpr char kOnDeviceDisconnected[] = "OnDeviceDisconnected";

inline constexpr char kOnScannerRegistered[] = "OnScannerRegistered";
inline constexpr char kOnScanResult[] = "OnScanResult";
inline constexpr char kOnAdvertisementFound[] = "OnAdvertisementFound";
inline constexpr char kOnAdvertisementLost[] = "OnAdvertisementLost";
}  // namespace adapter

namespace manager {
inline constexpr char kStart[] = "Start";
inline constexpr char kStop[] = "Stop";
inline constexpr char kGetAdapterEnabled[] = "GetAdapterEnabled";
inline constexpr char kGetFlossEnabled[] = "GetFlossEnabled";
inline constexpr char kSetFlossEnabled[] = "SetFlossEnabled";
inline constexpr char kGetState[] = "GetState";
inline constexpr char kGetAvailableAdapters[] = "GetAvailableAdapters";
inline constexpr char kGetDefaultAdapter[] = "GetDefaultAdapter";
inline constexpr char kSetDesiredDefaultAdapter[] = "SetDesiredDefaultAdapter";
inline constexpr char kGetFlossApiVersion[] = "GetFlossApiVersion";
inline constexpr char kRegisterCallback[] = "RegisterCallback";
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.ManagerCallback";
inline constexpr char kOnHciDeviceChanged[] = "OnHciDeviceChanged";
inline constexpr char kOnHciEnabledChanged[] = "OnHciEnabledChanged";
inline constexpr char kOnDefaultAdapterChanged[] = "OnDefaultAdapterChanged";
}  // namespace manager

namespace socket_manager {
inline constexpr char kRegisterCallback[] = "RegisterCallback";
inline constexpr char kUnregisterCallback[] = "UnregisterCallback";
inline constexpr char kListenUsingInsecureL2capChannel[] =
    "ListenUsingInsecureL2capChannel";
inline constexpr char kListenUsingInsecureL2capLeChannel[] =
    "ListenUsingInsecureL2capLeChannel";
inline constexpr char kListenUsingInsecureRfcommWithServiceRecord[] =
    "ListenUsingInsecureRfcommWithServiceRecord";
inline constexpr char kListenUsingL2capChannel[] = "ListenUsingL2capChannel";
inline constexpr char kListenUsingL2capLeChannel[] =
    "ListenUsingL2capLeChannel";
inline constexpr char kListenUsingRfcomm[] = "ListenUsingRfcomm";
inline constexpr char kListenUsingRfcommWithServiceRecord[] =
    "ListenUsingRfcommWithServiceRecord";
inline constexpr char kCreateInsecureL2capChannel[] =
    "CreateInsecureL2capChannel";
inline constexpr char kCreateInsecureL2capLeChannel[] =
    "CreateInsecureL2capLeChannel";
inline constexpr char kCreateInsecureRfcommSocketToServiceRecord[] =
    "CreateInsecureRfcommSocketToServiceRecord";
inline constexpr char kCreateL2capChannel[] = "CreateL2capChannel";
inline constexpr char kCreateL2capLeChannel[] = "CreateL2capLeChannel";
inline constexpr char kCreateRfcommSocketToServiceRecord[] =
    "CreateRfcommSocketToServiceRecord";
inline constexpr char kAccept[] = "Accept";
inline constexpr char kClose[] = "Close";
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.SocketManagerCallback";

inline constexpr char kOnIncomingSocketReady[] = "OnIncomingSocketReady";
inline constexpr char kOnIncomingSocketClosed[] = "OnIncomingSocketClosed";
inline constexpr char kOnHandleIncomingConnection[] =
    "OnHandleIncomingConnection";
inline constexpr char kOnOutgoingConnectionResult[] =
    "OnOutgoingConnectionResult";
}  // namespace socket_manager

namespace gatt {
inline constexpr char kRegisterClient[] = "RegisterClient";
inline constexpr char kUnregisterClient[] = "UnregisterClient";
inline constexpr char kClientConnect[] = "ClientConnect";
inline constexpr char kClientDisconnect[] = "ClientDisconnect";
inline constexpr char kRefreshDevice[] = "RefreshDevice";
inline constexpr char kDiscoverServices[] = "DiscoverServices";
inline constexpr char kDiscoverServiceByUuid[] = "DiscoverServiceByUuid";
inline constexpr char kReadCharacteristic[] = "ReadCharacteristic";
inline constexpr char kReadUsingCharacteristicUuid[] =
    "ReadUsingCharacteristicUuid";
inline constexpr char kWriteCharacteristic[] = "WriteCharacteristic";
inline constexpr char kReadDescriptor[] = "ReadDescriptor";
inline constexpr char kWriteDescriptor[] = "WriteDescriptor";
inline constexpr char kRegisterForNotification[] = "RegisterForNotification";
inline constexpr char kBeginReliableWrite[] = "BeginReliableWrite";
inline constexpr char kEndReliableWrite[] = "EndReliableWrite";
inline constexpr char kReadRemoteRssi[] = "ReadRemoteRssi";
inline constexpr char kConfigureMtu[] = "ConfigureMtu";
inline constexpr char kConnectionParameterUpdate[] =
    "ConnectionParameterUpdate";
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothGattCallback";
inline constexpr char kServerCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothGattServerCallback";

inline constexpr char kOnClientRegistered[] = "OnClientRegistered";
inline constexpr char kOnClientConnectionState[] = "OnClientConnectionState";
inline constexpr char kOnPhyUpdate[] = "OnPhyUpdate";
inline constexpr char kOnPhyRead[] = "OnPhyRead";
inline constexpr char kOnSearchComplete[] = "OnSearchComplete";
inline constexpr char kOnCharacteristicRead[] = "OnCharacteristicRead";
inline constexpr char kOnCharacteristicWrite[] = "OnCharacteristicWrite";
inline constexpr char kOnExecuteWrite[] = "OnExecuteWrite";
inline constexpr char kOnDescriptorRead[] = "OnDescriptorRead";
inline constexpr char kOnDescriptorWrite[] = "OnDescriptorWrite";
inline constexpr char kOnNotify[] = "OnNotify";
inline constexpr char kOnReadRemoteRssi[] = "OnReadRemoteRssi";
inline constexpr char kOnConfigureMtu[] = "OnConfigureMtu";
inline constexpr char kOnConnectionUpdated[] = "OnConnectionUpdated";
inline constexpr char kOnServiceChanged[] = "OnServiceChanged";

inline constexpr char kRegisterServer[] = "RegisterServer";
inline constexpr char kUnregisterServer[] = "UnregisterServer";
inline constexpr char kServerConnect[] = "ServerConnect";
inline constexpr char kServerDisconnect[] = "ServerDisconnect";
inline constexpr char kServerSetPreferredPhy[] = "ServerSetPreferredPhy";
inline constexpr char kServerReadPhy[] = "ServerReadPhy";
inline constexpr char kAddService[] = "AddService";
inline constexpr char kRemoveService[] = "RemoveService";
inline constexpr char kClearServices[] = "ClearServices";
inline constexpr char kSendResponse[] = "SendResponse";
inline constexpr char kServerSendNotification[] = "SendNotification";

inline constexpr char kOnServerRegistered[] = "OnServerRegistered";
inline constexpr char kOnServerConnectionState[] = "OnServerConnectionState";
inline constexpr char kOnServerServiceAdded[] = "OnServiceAdded";
inline constexpr char kOnServerServiceRemoved[] = "OnServiceRemoved";
inline constexpr char kOnServerCharacteristicReadRequest[] =
    "OnCharacteristicReadRequest";
inline constexpr char kOnServerDescriptorReadRequest[] =
    "OnDescriptorReadRequest";
inline constexpr char kOnServerCharacteristicWriteRequest[] =
    "OnCharacteristicWriteRequest";
inline constexpr char kOnServerDescriptorWriteRequest[] =
    "OnDescriptorWriteRequest";
inline constexpr char kOnServerNotificationSent[] = "OnNotificationSent";
inline constexpr char kOnServerMtuChanged[] = "OnMtuChanged";
inline constexpr char kOnServerSubrateChange[] = "OnSubrateChange";
}  // namespace gatt

namespace advertiser {
inline constexpr char kRegisterCallback[] = "RegisterAdvertiserCallback";
inline constexpr char kUnregisterCallback[] = "UnregisterAdvertiserCallback";
inline constexpr char kStartAdvertisingSet[] = "StartAdvertisingSet";
inline constexpr char kStopAdvertisingSet[] = "StopAdvertisingSet";
inline constexpr char kGetOwnAddress[] = "GetOwnAddress";
inline constexpr char kEnableAdvertisingSet[] = "EnableAdvertisingSet";
inline constexpr char kSetAdvertisingData[] = "SetAdvertisingData";
inline constexpr char kSetScanResponseData[] = "SetScanResponseData";
inline constexpr char kSetAdvertisingParameters[] = "SetAdvertisingParameters";
inline constexpr char kSetPeriodicAdvertisingParameters[] =
    "SetPeriodicAdvertisingParameters";
inline constexpr char kSetPeriodicAdvertisingData[] =
    "SetPeriodicAdvertisingData";
inline constexpr char kSetPeriodicAdvertisingEnable[] =
    "SetPeriodicAdvertisingEnable";

inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.AdvertisingSetCallback";
inline constexpr char kOnAdvertisingSetStarted[] = "OnAdvertisingSetStarted";
inline constexpr char kOnOwnAddressRead[] = "OnOwnAddressRead";
inline constexpr char kOnAdvertisingSetStopped[] = "OnAdvertisingSetStopped";
inline constexpr char kOnAdvertisingEnabled[] = "OnAdvertisingEnabled";
inline constexpr char kOnAdvertisingDataSet[] = "OnAdvertisingDataSet";
inline constexpr char kOnScanResponseDataSet[] = "OnScanResponseDataSet";
inline constexpr char kOnAdvertisingParametersUpdated[] =
    "OnAdvertisingParametersUpdated";
inline constexpr char kOnPeriodicAdvertisingParametersUpdated[] =
    "OnPeriodicAdvertisingParametersUpdated";
inline constexpr char kOnPeriodicAdvertisingDataSet[] =
    "OnPeriodicAdvertisingDataSet";
inline constexpr char kOnPeriodicAdvertisingEnabled[] =
    "OnPeriodicAdvertisingEnabled";
}  // namespace advertiser

namespace battery_manager {
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.BatteryManagerCallback";
inline constexpr char kRegisterBatteryCallback[] = "RegisterBatteryCallback";
inline constexpr char kUnregisterBatteryCallback[] =
    "UnregisterBatteryCallback";
inline constexpr char kGetBatteryInformation[] = "GetBatteryInformation";

inline constexpr char kOnBatteryInfoUpdated[] = "OnBatteryInfoUpdated";
}  // namespace battery_manager

namespace bluetooth_telephony {
inline constexpr char kSetPhoneOpsEnabled[] = "SetPhoneOpsEnabled";
}  // namespace bluetooth_telephony

namespace admin {
inline constexpr char kRegisterCallback[] = "RegisterAdminPolicyCallback";
inline constexpr char kUnregisterCallback[] = "UnregisterAdminPolicyCallback";
inline constexpr char kCallbackInterface[] =
    "org.chromium.bluetooth.AdminPolicyCallback";
inline constexpr char kOnServiceAllowlistChanged[] =
    "OnServiceAllowlistChanged";
inline constexpr char kOnDevicePolicyEffectChanged[] =
    "OnDevicePolicyEffectChanged";
inline constexpr char kSetAllowedServices[] = "SetAllowedServices";
inline constexpr char kGetAllowedServices[] = "GetAllowedServices";
inline constexpr char kGetDevicePolicyEffect[] = "GetDevicePolicyEffect";
}  // namespace admin

namespace adapter_logging {
inline constexpr char kIsDebugEnabled[] = "IsDebugEnabled";
inline constexpr char kSetDebugLogging[] = "SetDebugLogging";
}  // namespace adapter_logging

namespace experimental {
inline constexpr char kSetLLPrivacy[] = "SetLLPrivacy";
inline constexpr char kSetDevCoredump[] = "SetDevCoredump";
}  // namespace experimental

// BluetoothDevice structure for DBus apis.
struct DEVICE_BLUETOOTH_EXPORT FlossDeviceId {
  std::string address;
  std::string name;

  inline bool operator==(const FlossDeviceId& rhs) const {
    return address == rhs.address && name == rhs.name;
  }

  friend std::ostream& operator<<(std::ostream& os, const FlossDeviceId& id) {
    return os << "FlossDeviceId(" << id.address << ", " << id.name << ")";
  }

  static const char kDeviceIdNameKey[];
  static const char kDeviceIdAddressKey[];
};

// Represents an error sent through DBus.
//
// In a D-Bus message, error contains 2 parts: error name and error message.
// This is a structure to hold these error info and provides a utility for human
// readable representation.
struct DEVICE_BLUETOOTH_EXPORT Error {
  Error(const std::string& name, const std::string& message);

  // Presents the error as "<error name>: <error message>".
  friend std::ostream& operator<<(std::ostream& os, const Error& error);
  std::string ToString();

  std::string name;
  std::string message;
};

// Represents void return type of D-Bus (no return). Needed so that we can use
// "void" as a type in C++ templates.
// Needs to be exported because there are template instantiations using this.
struct DEVICE_BLUETOOTH_EXPORT Void {};

// Represents the result of D-Bus method call. A Floss method call returns
// either a data or a D-Bus error.
template <typename T>
using DBusResult = base::expected<T, Error>;

// A callback of Floss API method call. This encapsulates RPC-level status
// (in Floss case D-Bus status and return data parsing) so that each return can
// be either "ok" (contains T) or "error" (contains error name and message).
template <typename T>
using ResponseCallback = base::OnceCallback<void(DBusResult<T>)>;

// A Weakly Owned base::OnceCallback<void(T)>. The main usecase for this is to
// have a weak pointer available for |PostDelayedTask|, where deleting the main
// object will automatically cancel the posted task.
template <typename T>
class WeaklyOwnedCallback {
 public:
  explicit WeaklyOwnedCallback(base::OnceCallback<void(T)> cb)
      : cb_(std::move(cb)) {}
  ~WeaklyOwnedCallback() = default;

  static std::unique_ptr<WeaklyOwnedCallback> Create(
      base::OnceCallback<void(T)> cb) {
    return std::make_unique<WeaklyOwnedCallback>(std::move(cb));
  }

  // Creates a pointer to the callback which will run automatically after
  // |timeout_ms| with given return value unless the callback is deleted.
  static std::unique_ptr<WeaklyOwnedCallback> CreateWithTimeout(
      base::OnceCallback<void(T)> cb,
      int timeout_ms,
      T error_ret) {
    std::unique_ptr<WeaklyOwnedCallback> self = Create(std::move(cb));
    self->PostDelayed(timeout_ms, error_ret);

    return self;
  }

  // If the callback hasn't been executed, run it.
  void Run(T ret) {
    if (cb_) {
      std::move(cb_).Run(std::move(ret));
    }
  }

  base::WeakPtr<WeaklyOwnedCallback> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void PostDelayed(int timeout_ms, T error_ret) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WeaklyOwnedCallback<T>::Run,
                       weak_ptr_factory_.GetWeakPtr(), error_ret),
        base::Milliseconds(timeout_ms));
  }

  base::OnceCallback<void(T)> cb_;
  base::WeakPtrFactory<WeaklyOwnedCallback> weak_ptr_factory_{this};
};

// A Weakly Owned ResponseCallback<T>.
template <typename T>
using WeaklyOwnedResponseCallback = WeaklyOwnedCallback<DBusResult<T>>;

struct DBusTypeInfo {
  const std::string dbus_signature;
  const std::string type_name;
};

// To minimize the overhead of constructing a struct, this function returns
// a const reference. Specialization implementations are recommended to return
// a statically allocated DBusTypeInfo for this reason.
template <typename T>
DEVICE_BLUETOOTH_EXPORT const DBusTypeInfo& GetDBusTypeInfo(const T*);

template <typename T>
const DBusTypeInfo& GetDBusTypeInfo(const std::vector<T>*) {
  static const base::NoDestructor<DBusTypeInfo> elem_info(
      GetDBusTypeInfo(static_cast<T*>(nullptr)));
  static const base::NoDestructor<DBusTypeInfo> info{
      {base::StrCat({"a", elem_info->dbus_signature}),
       base::StrCat({"vector<", elem_info->type_name, ">"})}};
  return *info;
}

template <typename T, typename U>
const DBusTypeInfo& GetDBusTypeInfo(const std::map<T, U>*) {
  static const base::NoDestructor<DBusTypeInfo> key_info(
      GetDBusTypeInfo(static_cast<T*>(nullptr)));
  static const base::NoDestructor<DBusTypeInfo> val_info(
      GetDBusTypeInfo(static_cast<U*>(nullptr)));
  static const base::NoDestructor<DBusTypeInfo> info{
      {base::StrCat(
           {"a{", key_info->dbus_signature, val_info->dbus_signature, "}"}),
       base::StrCat(
           {"map<", key_info->type_name, ", ", val_info->type_name, ">"})}};
  return *info;
}

template <typename T>
const DBusTypeInfo& GetDBusTypeInfo(const std::optional<T>*) {
  static const base::NoDestructor<DBusTypeInfo> elem_info(
      GetDBusTypeInfo(static_cast<T*>(nullptr)));
  static const base::NoDestructor<DBusTypeInfo> info{
      {"a{sv}", base::StrCat({"optional<", elem_info->type_name, ">"})}};
  return *info;
}

// Restrict all access to DBus client initialization to FlossDBusManager so we
// can enforce the proper ordering of initialization and shutdowns.
class DEVICE_BLUETOOTH_EXPORT FlossDBusClient {
 public:
  // Adopted from bt_status_t in system/include/hardware/bluetooth.h
  enum class BtifStatus : uint32_t {
    kSuccess = 0,
    kFail,
    kNotReady,
    kNomem,
    kBusy,
    kDone,
    kUnsupported,
    kParmInvalid,
    kUnhandled,
    kAuthFailure,
    kRmtDevDown,
    kAuthRejected,
    kJniEnvironmentError,
    kJniThreadAttachError,
    kWakelockError,
    kTimeout,
    kDeviceNotFound,
    kUnexpectedState,
    kSocketError,
  };

  enum class BluetoothTransport {
    kAuto = 0,
    kBrEdr = 1,
    kLe = 2,
  };

  // Error: DBus error.
  static const char kErrorDBus[];

  // Error: No response from bus.
  static const char kErrorNoResponse[];

  // Error: Invalid parameters.
  static const char kErrorInvalidParameters[];

  // Error: Invalid return.
  static const char kErrorInvalidReturn[];

  // Property key for std::optional dbus serialization.
  static const char kOptionalValueKey[];

  // Error: does not exist.
  static const char DEVICE_BLUETOOTH_EXPORT kErrorDoesNotExist[];

  // Convert adapter number to adapter object path.
  static dbus::ObjectPath GenerateAdapterPath(int adapter_index);

  // Convert adapter number to gatt object path.
  static dbus::ObjectPath GenerateGattPath(int adapter_index);

  // Convert adapter number to battery_manager object path.
  static dbus::ObjectPath GenerateBatteryManagerPath(int adapter_index);

  // Convert adapter number to bluetooth_telephony object path.
  static dbus::ObjectPath GenerateBluetoothTelephonyPath(int adapter_index);

  // Convert adapter number to admin object path.
  static dbus::ObjectPath GenerateAdminPath(int adapter_index);

  // Convert adapter number to logging object path.
  static dbus::ObjectPath GenerateLoggingPath(int adapter_index);

  // Convert Floss error codes to BluetoothDevice defined error codes.
  static device::BluetoothDevice::ConnectErrorCode BtifStatusToConnectErrorCode(
      FlossDBusClient::BtifStatus status);

  // Generalized DBus serialization (used for generalized method call
  // invocation).
  template <typename T>
  static void WriteDBusParam(dbus::MessageWriter* writer, const T& data);

  // Generalized writer for container types using variants (i.e. a{sv}).
  template <typename T>
  static void WriteDBusParamIntoVariant(dbus::MessageWriter* writer,
                                        const T& data) {
    dbus::MessageWriter variant(nullptr);
    writer->OpenVariant(GetDBusTypeInfo(&data).dbus_signature, &variant);
    WriteDBusParam(&variant, data);
    writer->CloseContainer(&variant);
  }

  // Generalized write for std::vector.
  template <typename T>
  static void WriteDBusParam(dbus::MessageWriter* writer,
                             const std::vector<T>& value) {
    dbus::MessageWriter array_writer(nullptr);
    writer->OpenArray(GetDBusTypeInfo(static_cast<T*>(nullptr)).dbus_signature,
                      &array_writer);
    for (const auto& entry : value) {
      WriteDBusParam<>(&array_writer, entry);
    }
    writer->CloseContainer(&array_writer);
  }

  // Generalized write for std::map.
  template <typename T, typename U>
  static void WriteDBusParam(dbus::MessageWriter* writer,
                             const std::map<T, U>& data) {
    std::string signature = base::StrCat(
        {"{", GetDBusTypeInfo(static_cast<T*>(nullptr)).dbus_signature,
         GetDBusTypeInfo(static_cast<U*>(nullptr)).dbus_signature, "}"});
    dbus::MessageWriter array(nullptr);
    writer->OpenArray(signature, &array);
    for (auto const& [key, val] : data) {
      dbus::MessageWriter dict(nullptr);
      array.OpenDictEntry(&dict);
      WriteDBusParam<>(&dict, key);
      WriteDBusParam<>(&dict, val);
      array.CloseContainer(&dict);
    }
    writer->CloseContainer(&array);
  }

  // Optional container type needs to be explicitly listed here.
  template <typename T>
  static void WriteDBusParam(dbus::MessageWriter* writer,
                             const std::optional<T>& data) {
    dbus::MessageWriter array(nullptr);
    dbus::MessageWriter dict(nullptr);

    writer->OpenArray("{sv}", &array);

    // Only serialize optional value if it exists.
    if (data) {
      array.OpenDictEntry(&dict);
      dict.AppendString(kOptionalValueKey);
      WriteDBusParamIntoVariant<T>(&dict, *data);
      array.CloseContainer(&dict);
    }
    writer->CloseContainer(&array);
  }

  template <typename T>
  static void WriteDBusParamIntoVariant(dbus::MessageWriter* writer,
                                        const std::optional<T>& data) {
    dbus::MessageWriter variant(nullptr);
    writer->OpenVariant("a{sv}", &variant);
    WriteDBusParam(&variant, data);
    writer->CloseContainer(&variant);
  }

  // Base case for variadic write.
  static void WriteAllDBusParams(dbus::MessageWriter* writer) {}

  // Variadic write method that expands to multiple WriteDBusParam calls.
  template <typename T, typename... Args>
  static void WriteAllDBusParams(dbus::MessageWriter* writer,
                                 const T& first,
                                 const Args&... args) {
    WriteDBusParam(writer, first);
    WriteAllDBusParams(writer, args...);
  }

  template <typename T>
  static void WriteDictEntry(dbus::MessageWriter* writer,
                             const std::string& key,
                             const T& value) {
    dbus::MessageWriter dict(nullptr);

    writer->OpenDictEntry(&dict);
    dict.AppendString(key);
    WriteDBusParamIntoVariant(&dict, value);
    writer->CloseContainer(&dict);
  }

  // Generalized DBus deserialization (used for generalized method call returns
  // and can be used for exported methods as well). Implement for each type that
  // you want deserialized.
  template <typename T>
  static bool ReadDBusParam(dbus::MessageReader* reader, T* value);

  // Generalized reader for container types using variants (i.e. a{sv}).
  template <typename T>
  static bool ReadDBusParamFromVariant(dbus::MessageReader* reader, T* value) {
    dbus::MessageReader variant_reader(nullptr);
    if (!reader->PopVariant(&variant_reader)) {
      return false;
    }

    return ReadDBusParam(&variant_reader, value);
  }

  // Specialization for vector of anything.
  template <typename T>
  static bool ReadDBusParam(dbus::MessageReader* reader,
                            std::vector<T>* value) {
    dbus::MessageReader subreader(nullptr);
    if (!reader->PopArray(&subreader))
      return false;

    while (subreader.HasMoreData()) {
      T element;
      if (!ReadDBusParam<>(&subreader, &element))
        return false;

      value->emplace_back(std::move(element));
    }

    return true;
  }

  // Specialization for std::map.
  template <typename T, typename U>
  static bool ReadDBusParam(dbus::MessageReader* reader, std::map<T, U>* data) {
    dbus::MessageReader array_reader(nullptr);
    if (!reader->PopArray(&array_reader))
      return false;

    while (array_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      if (!array_reader.PopDictEntry(&dict_entry_reader))
        return false;

      T key;
      U value;
      if (!ReadDBusParam<>(&dict_entry_reader, &key) ||
          !ReadDBusParam<>(&dict_entry_reader, &value))
        return false;

      data->insert({key, value});
    }

    return true;
  }

  // Optional container type needs to be explicitly implemented here.
  template <typename T>
  static bool ReadDBusParam(dbus::MessageReader* reader,
                            std::optional<T>* value) {
    dbus::MessageReader array(nullptr);
    dbus::MessageReader dict(nullptr);

    T inner;

    if (!reader->PopArray(&array)) {
      return false;
    }

    while (array.PopDictEntry(&dict)) {
      std::string key;
      dict.PopString(&key);

      if (key == kOptionalValueKey) {
        if (!ReadDBusParamFromVariant<T>(&dict, &inner)) {
          return false;
        }

        *value = std::move(std::optional<T>(std::move(inner)));
      }
    }

    return true;
  }

  // Base case for variadic read.
  static bool ReadAllDBusParams(dbus::MessageReader* reader) { return true; }

  // Variadic read method that expands to multiple ReadDBusParam calls.
  // Individual calls to |ReadDBusParam| must succeed before the next call is
  // done.
  template <typename T, typename... Args>
  static bool ReadAllDBusParams(dbus::MessageReader* reader,
                                T* first,
                                Args*... args) {
    return ReadDBusParam(reader, first) && ReadAllDBusParams(reader, args...);
  }

  template <typename T>
  using FieldReader = std::function<bool(dbus::MessageReader*, T* data)>;

  // Useful to generate D-Bus reader of a struct. Example usage:
  //
  // template <>
  // bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
  //                                     ScanResult* scan_result) {
  //   static StructReader<ScanResult> struct_reader({
  //       {"address", CreateFieldReader(&ScanResult::address)},
  //       {"addr_type", CreateFieldReader(&ScanResult::addr_type)},
  //       <just define more fields here>
  //   });
  //   return struct_reader.ReadDBusParam(reader, scan_result);
  // }
  template <typename T>
  class StructReader {
   private:
    std::unordered_map<std::string, FieldReader<T>> fields_;

   public:
    explicit StructReader(
        std::vector<std::pair<std::string, FieldReader<T>>> fields) {
      for (auto const& kv : fields) {
        fields_.insert(kv);
      }
    }

    bool ReadDBusParam(dbus::MessageReader* reader, T* data) {
      // Keep track of parsed fields to detect missing and duplicate fields.
      std::unordered_set<std::string> parsed_fields;

      dbus::MessageReader array_reader(nullptr);
      if (!reader->PopArray(&array_reader))
        return false;

      // For each dictionary entry
      while (array_reader.HasMoreData()) {
        dbus::MessageReader entry_reader(nullptr);
        if (!array_reader.PopDictEntry(&entry_reader))
          return false;

        std::string key;
        if (!entry_reader.PopString(&key))
          return false;

        if (base::Contains(fields_, key)) {
          dbus::MessageReader variant_reader(nullptr);
          entry_reader.PopVariant(&variant_reader);

          if (!fields_[key](&variant_reader, data))
            return false;

          if (base::Contains(parsed_fields, key))
            return false;

          parsed_fields.insert(key);
        } else {
          DBusTypeInfo type_info = GetDBusTypeInfo(data);
          VLOG(3) << "Does not know how to read field " << type_info.type_name
                  << "." << key;
        }
      }

      // All defined fields are required.
      return parsed_fields.size() == fields_.size();
    }
  };

  // S is the type of the container struct.
  // T is the type of the field.
  template <typename S, typename T>
  static FieldReader<S> CreateFieldReader(T S::*field) {
    return [field](dbus::MessageReader* reader, S* container) -> bool {
      return FlossDBusClient::ReadDBusParam(reader, &(container->*field));
    };
  }

  template <typename R, typename... Args>
  void CallMethod(ResponseCallback<R> callback,
                  dbus::Bus* bus,
                  const std::string& service_name,
                  const std::string& interface_name,
                  const dbus::ObjectPath& object_path,
                  const char* method_name,
                  Args... args) {
    if (bus == nullptr) {
      LOG(ERROR) << "D-Bus is not initialized, cannot call method "
                 << method_name << " on " << object_path.value();
      std::move(callback).Run(base::unexpected(
          Error(std::string(kErrorDBus), "DBus not initialized")));
      return;
    }

    dbus::ObjectProxy* object_proxy =
        bus->GetObjectProxy(service_name, object_path);
    if (!object_proxy) {
      VLOG(1) << "Object proxy does not exist when trying to call "
              << method_name;
      std::move(callback).Run(base::unexpected(
          Error(std::string(kErrorDBus), "Invalid object proxy")));
      return;
    }

    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);

    FlossDBusClient::WriteAllDBusParams(&writer, args...);

    object_proxy->CallMethodWithErrorResponse(
        &method_call, kDBusTimeoutMs,
        base::BindOnce(&FlossDBusClient::DefaultResponseWithCallback<R>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  FlossDBusClient(const FlossDBusClient&) = delete;
  FlossDBusClient& operator=(const FlossDBusClient&) = delete;

  // Common init signature for all clients. |on_ready| should be called once the
  // client is ready to be used.
  virtual void Init(dbus::Bus* bus,
                    const std::string& bluetooth_service_name,
                    const int adapter_index,
                    base::Version version,
                    base::OnceClosure on_ready) = 0;

 protected:
  // Convert a dbus::ErrorResponse into a floss::Error struct.
  static Error ErrorResponseToError(const std::string& default_name,
                                    const std::string& default_message,
                                    dbus::ErrorResponse* error);

  FlossDBusClient();
  virtual ~FlossDBusClient();

  // Log a dbus::ErrorResponse.
  void LogErrorResponse(const std::string& message, dbus::ErrorResponse* error);

  // Default handler that runs |callback| with the callback with an optional
  // return and optional error.
  template <typename T>
  void DefaultResponseWithCallback(ResponseCallback<T> callback,
                                   dbus::Response* response,
                                   dbus::ErrorResponse* error_response) {
    if (response) {
      T ret;
      dbus::MessageReader reader(response);

      if (!FlossDBusClient::ReadAllDBusParams<T>(&reader, &ret)) {
        LOG(ERROR) << "Failed reading return from response";
        std::move(callback).Run(
            base::unexpected(Error(kErrorInvalidReturn, "")));
        return;
      }

      std::move(callback).Run(ret);
      return;
    }

    std::move(callback).Run(base::unexpected(ErrorResponseToError(
        kErrorNoResponse, /*default_message=*/std::string(), error_response)));
  }

  // Default handler for a response. It will either log the error response or
  // print |caller| to VLOG. |caller| should be the name of the DBus method that
  // is being called.
  void DefaultResponse(const std::string& caller,
                       dbus::Response* response,
                       dbus::ErrorResponse* error_response);

  // API version.
  base::Version version_;

 private:
  base::WeakPtrFactory<FlossDBusClient> weak_ptr_factory_{this};
};

// Utility to keep a property that takes care of getting the initial value,
// monitoring for updates, and notifying when value is updated.
//
// In Floss API, it is a pattern to abstract a value as a "property". These
// values always have:
// * A getter: A method exposed by Floss daemon to get current value.
// * An update callback: A method to be called by Floss daemon to client when
//   the value is updated.
//
// To simplify repetitive code performing common operations above, use this
// utility by just specifying the property getter, update method, and the
// interface names.
//
// |T| is the type of the property.
template <typename T>
class FlossProperty {
 public:
  // Instantiates a property, given:
  // |interface| - The D-Bus interface of the getter.
  // |callback_interface| - The D-Bus interface of the value update method.
  // |getter| - The method name of the getter.
  // |on_update| - The method name of the value update.
  FlossProperty(const char* interface,
                const char* callback_interface,
                const char* getter,
                const char* on_update)
      : interface_(interface),
        callback_interface_(callback_interface),
        getter_(getter),
        on_update_(on_update) {}

  // Initializes the property. Once Init-ed, it takes care of getting the
  // initial value, keeping it updated, notifying when there is an update.
  // |bus| - D-Bus connection.
  // |service_name| - Floss daemon D-Bus name.
  // |path| - Object path where the getter is available.
  // |callback_path| - Object path where the daemon calls back on value updates.
  // |update_callback| - Caller can provide this to be notified when there are
  //                     updates.
  void Init(FlossDBusClient* client,
            dbus::Bus* bus,
            const std::string& service_name,
            const dbus::ObjectPath& path,
            const dbus::ObjectPath& callback_path,
            base::RepeatingCallback<void(const T&)> update_callback) {
    update_callback_ = update_callback;

    // Get the initial value.
    client->CallMethod(base::BindOnce(&FlossProperty::OnGetInitialValue,
                                      weak_ptr_factory_.GetWeakPtr()),
                       bus, service_name, interface_, path, getter_);

    if (!on_update_) {
      return;
    }

    // Listen for property updates.
    dbus::ExportedObject* exported_object =
        bus->GetExportedObject(callback_path);
    if (!exported_object) {
      LOG(ERROR) << "Could not export callback to listen for property updates"
                 << callback_path.value();
      return;
    }

    exported_object->ExportMethod(
        callback_interface_, on_update_,
        base::BindRepeating(&FlossProperty::OnValueUpdated,
                            weak_ptr_factory_.GetWeakPtr()),
        base::DoNothing());
  }

  // Returns the current value of this property.
  const T& Get() const { return value_; }

 private:
  void OnGetInitialValue(DBusResult<T> ret) {
    if (!ret.has_value()) {
      LOG(ERROR) << "Error getting initial value";
      return;
    }

    UpdateValue(std::move(*ret));
  }

  void OnValueUpdated(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender) {
    T data;
    dbus::MessageReader reader(method_call);
    if (!FlossDBusClient::ReadDBusParam(&reader, &data)) {
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, floss::FlossDBusClient::kErrorInvalidParameters,
              "Error parsing property value"));
      return;
    }

    UpdateValue(std::move(data));

    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  }

  void UpdateValue(T val) {
    value_ = std::move(val);
    update_callback_.Run(value_);
  }

  // Interfaces.
  const char* interface_;
  const char* callback_interface_;

  // Method name to call to Floss daemon to get property value.
  const char* getter_;
  // Method name called by Floss daemon to us to notify updates.
  // Null means the property value never changes.
  const char* on_update_;

  // Keeps the property value.
  T value_ = T();

  // To update caller when property is updated.
  base::RepeatingCallback<void(const T&)> update_callback_;

  // WeakPtrFactory must be last.
  base::WeakPtrFactory<FlossProperty> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_DBUS_CLIENT_H_
