// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_dbus_client.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/message.h"
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_lescan_client.h"

namespace floss {

// All Floss D-Bus methods return immediately, so the timeout can be very short.
int kDBusTimeoutMs = 2000;
// Timeout for waiting HCI enabled changed. Make it longer since it takes longer
// when there is a connected device.
int kAdapterPowerTimeoutMs = 5000;

// TODO(b/189499077) - Expose via floss package
const char kAdapterService[] = "org.chromium.bluetooth";
const char kManagerService[] = "org.chromium.bluetooth.Manager";

const char kAdapterLoggingObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/logging";
const char kAdapterObjectFormat[] = "/org/chromium/bluetooth/hci%d/adapter";
const char kAdminObjectFormat[] = "/org/chromium/bluetooth/hci%d/admin";
const char kBatteryManagerObjectFormat[] =
    "/org/chromium/bluetooth/hci%d/battery_manager";
const char kGattObjectFormat[] = "/org/chromium/bluetooth/hci%d/gatt";
const char kManagerObject[] = "/org/chromium/bluetooth/Manager";
const char kMediaObjectFormat[] = "/org/chromium/bluetooth/hci%d/media";

const char kAdapterInterface[] = "org.chromium.bluetooth.Bluetooth";
const char kAdapterLoggingInterface[] = "org.chromium.bluetooth.Logging";
const char kAdminInterface[] = "org.chromium.bluetooth.BluetoothAdmin";
const char kBatteryManagerInterface[] = "org.chromium.bluetooth.BatteryManager";
const char kExperimentalInterface[] = "org.chromium.bluetooth.Experimental";
const char kGattInterface[] = "org.chromium.bluetooth.BluetoothGatt";
const char kManagerInterface[] = "org.chromium.bluetooth.Manager";
const char kSocketManagerInterface[] = "org.chromium.bluetooth.SocketManager";

namespace adapter {
const char kGetAddress[] = "GetAddress";
const char kGetName[] = "GetName";
const char kSetName[] = "SetName";
const char kGetDiscoverable[] = "GetDiscoverable";
const char kGetDiscoverableTimeout[] = "GetDiscoverableTimeout";
const char kSetDiscoverable[] = "SetDiscoverable";
const char kStartDiscovery[] = "StartDiscovery";
const char kCancelDiscovery[] = "CancelDiscovery";
const char kCreateBond[] = "CreateBond";
const char kCancelBondProcess[] = "CancelBondProcess";
const char kRemoveBond[] = "RemoveBond";
const char kGetRemoteType[] = "GetRemoteType";
const char kGetRemoteClass[] = "GetRemoteClass";
const char kGetRemoteAppearance[] = "GetRemoteAppearance";
const char kGetConnectionState[] = "GetConnectionState";
const char kGetRemoteUuids[] = "GetRemoteUuids";
const char kGetBondState[] = "GetBondState";
const char kConnectAllEnabledProfiles[] = "ConnectAllEnabledProfiles";
const char kDisconnectAllEnabledProfiles[] = "DisconnectAllEnabledProfiles";
const char kRegisterCallback[] = "RegisterCallback";
const char kRegisterConnectionCallback[] = "RegisterConnectionCallback";
const char kRegisterScanner[] = "RegisterScanner";
const char kUnregisterScanner[] = "UnregisterScanner";
const char kRegisterScannerCallback[] = "RegisterScannerCallback";
const char kUnregisterScannerCallback[] = "UnregisterScannerCallback";
const char kStartScan[] = "StartScan";
const char kStopScan[] = "StopScan";
const char kSetPairingConfirmation[] = "SetPairingConfirmation";
const char kSetPin[] = "SetPin";
const char kSetPasskey[] = "SetPasskey";
const char kGetBondedDevices[] = "GetBondedDevices";
const char kGetConnectedDevices[] = "GetConnectedDevices";

// TODO(abps) - Rename this to AdapterCallback in platform and here
const char kCallbackInterface[] = "org.chromium.bluetooth.BluetoothCallback";
const char kConnectionCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothConnectionCallback";

const char kOnAdapterPropertyChanged[] = "OnAdapterPropertyChanged";
const char kOnAddressChanged[] = "OnAddressChanged";
const char kOnNameChanged[] = "OnNameChanged";
const char kOnDiscoverableChanged[] = "OnDiscoverableChanged";
const char kOnDeviceFound[] = "OnDeviceFound";
const char kOnDeviceCleared[] = "OnDeviceCleared";
const char kOnDiscoveringChanged[] = "OnDiscoveringChanged";
const char kOnSspRequest[] = "OnSspRequest";

const char kOnBondStateChanged[] = "OnBondStateChanged";
const char kOnDeviceConnected[] = "OnDeviceConnected";
const char kOnDeviceDisconnected[] = "OnDeviceDisconnected";

const char kOnScannerRegistered[] = "OnScannerRegistered";
const char kOnScanResult[] = "OnScanResult";
const char kOnAdvertisementFound[] = "OnAdvertisementFound";
const char kOnAdvertisementLost[] = "OnAdvertisementLost";
}  // namespace adapter

namespace manager {
const char kStart[] = "Start";
const char kStop[] = "Stop";
const char kGetFlossEnabled[] = "GetFlossEnabled";
const char kSetFlossEnabled[] = "SetFlossEnabled";
const char kGetState[] = "GetState";
const char kGetAvailableAdapters[] = "GetAvailableAdapters";
const char kGetDefaultAdapter[] = "GetDefaultAdapter";
const char kSetDesiredDefaultAdapter[] = "SetDesiredDefaultAdapter";
const char kRegisterCallback[] = "RegisterCallback";
const char kCallbackInterface[] = "org.chromium.bluetooth.ManagerCallback";
const char kOnHciDeviceChanged[] = "OnHciDeviceChanged";
const char kOnHciEnabledChanged[] = "OnHciEnabledChanged";
const char kOnDefaultAdapterChanged[] = "OnDefaultAdapterChanged";
}  // namespace manager

namespace socket_manager {
const char kRegisterCallback[] = "RegisterCallback";
const char kListenUsingInsecureL2capChannel[] =
    "ListenUsingInsecureL2capChannel";
const char kListenUsingInsecureRfcommWithServiceRecord[] =
    "ListenUsingInsecureRfcommWithServiceRecord";
const char kListenUsingL2capChannel[] = "ListenUsingL2capChannel";
const char kListenUsingRfcommWithServiceRecord[] =
    "ListenUsingRfcommWithServiceRecord";
const char kCreateInsecureL2capChannel[] = "CreateInsecureL2capChannel";
const char kCreateInsecureRfcommSocketToServiceRecord[] =
    "CreateInsecureRfcommSocketToServiceRecord";
const char kCreateL2capChannel[] = "CreateL2capChannel";
const char kCreateRfcommSocketToServiceRecord[] =
    "CreateRfcommSocketToServiceRecord";
const char kAccept[] = "Accept";
const char kClose[] = "Close";
const char kCallbackInterface[] =
    "org.chromium.bluetooth.SocketManagerCallback";

const char kOnIncomingSocketReady[] = "OnIncomingSocketReady";
const char kOnIncomingSocketClosed[] = "OnIncomingSocketClosed";
const char kOnHandleIncomingConnection[] = "OnHandleIncomingConnection";
const char kOnOutgoingConnectionResult[] = "OnOutgoingConnectionResult";
}  // namespace socket_manager

namespace gatt {
const char kRegisterClient[] = "RegisterClient";
const char kUnregisterClient[] = "UnregisterClient";
const char kClientConnect[] = "ClientConnect";
const char kClientDisconnect[] = "ClientDisconnect";
const char kRefreshDevice[] = "RefreshDevice";
const char kDiscoverServices[] = "DiscoverServices";
const char kDiscoverServiceByUuid[] = "DiscoverServiceByUuid";
const char kReadCharacteristic[] = "ReadCharacteristic";
const char kReadUsingCharacteristicUuid[] = "ReadUsingCharacteristicUuid";
const char kWriteCharacteristic[] = "WriteCharacteristic";
const char kReadDescriptor[] = "ReadDescriptor";
const char kWriteDescriptor[] = "WriteDescriptor";
const char kRegisterForNotification[] = "RegisterForNotification";
const char kBeginReliableWrite[] = "BeginReliableWrite";
const char kEndReliableWrite[] = "EndReliableWrite";
const char kReadRemoteRssi[] = "ReadRemoteRssi";
const char kConfigureMtu[] = "ConfigureMtu";
const char kConnectionParameterUpdate[] = "ConnectionParameterUpdate";
const char kCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothGattCallback";
const char kServerCallbackInterface[] =
    "org.chromium.bluetooth.BluetoothGattServerCallback";

const char kOnClientRegistered[] = "OnClientRegistered";
const char kOnClientConnectionState[] = "OnClientConnectionState";
const char kOnPhyUpdate[] = "OnPhyUpdate";
const char kOnPhyRead[] = "OnPhyRead";
const char kOnSearchComplete[] = "OnSearchComplete";
const char kOnCharacteristicRead[] = "OnCharacteristicRead";
const char kOnCharacteristicWrite[] = "OnCharacteristicWrite";
const char kOnExecuteWrite[] = "OnExecuteWrite";
const char kOnDescriptorRead[] = "OnDescriptorRead";
const char kOnDescriptorWrite[] = "OnDescriptorWrite";
const char kOnNotify[] = "OnNotify";
const char kOnReadRemoteRssi[] = "OnReadRemoteRssi";
const char kOnConfigureMtu[] = "OnConfigureMtu";
const char kOnConnectionUpdated[] = "OnConnectionUpdated";
const char kOnServiceChanged[] = "OnServiceChanged";

const char kRegisterServer[] = "RegisterServer";
const char kUnregisterServer[] = "UnregisterServer";
const char kServerConnect[] = "ServerConnect";
const char kServerDisconnect[] = "ServerDisconnect";
const char kServerSetPreferredPhy[] = "ServerSetPreferredPhy";
const char kServerReadPhy[] = "ServerReadPhy";
const char kAddService[] = "AddService";
const char kRemoveService[] = "RemoveService";
const char kClearServices[] = "ClearServices";
const char kSendResponse[] = "SendResponse";
const char kServerSendNotification[] = "SendNotification";

const char kOnServerRegistered[] = "OnServerRegistered";
const char kOnServerConnectionState[] = "OnServerConnectionState";
const char kOnServerServiceAdded[] = "OnServiceAdded";
const char kOnServerCharacteristicReadRequest[] = "OnCharacteristicReadRequest";
const char kOnServerDescriptorReadRequest[] = "OnDescriptorReadRequest";
const char kOnServerCharacteristicWriteRequest[] =
    "OnCharacteristicWriteRequest";
const char kOnServerDescriptorWriteRequest[] = "OnDescriptorWriteRequest";
const char kOnServerNotificationSent[] = "OnNotificationSent";
const char kOnServerMtuChanged[] = "OnMtuChanged";
const char kOnServerSubrateChange[] = "OnSubrateChange";
}  // namespace gatt

namespace advertiser {
const char kRegisterCallback[] = "RegisterAdvertiserCallback";
const char kStartAdvertisingSet[] = "StartAdvertisingSet";
const char kStopAdvertisingSet[] = "StopAdvertisingSet";
const char kGetOwnAddress[] = "GetOwnAddress";
const char kEnableAdvertisingSet[] = "EnableAdvertisingSet";
const char kSetAdvertisingData[] = "SetAdvertisingData";
const char kSetScanResponseData[] = "SetScanResponseData";
const char kSetAdvertisingParameters[] = "SetAdvertisingParameters";
const char kSetPeriodicAdvertisingParameters[] =
    "SetPeriodicAdvertisingParameters";
const char kSetPeriodicAdvertisingData[] = "SetPeriodicAdvertisingData";
const char kSetPeriodicAdvertisingEnable[] = "SetPeriodicAdvertisingEnable";

const char kCallbackInterface[] =
    "org.chromium.bluetooth.AdvertisingSetCallback";
const char kOnAdvertisingSetStarted[] = "OnAdvertisingSetStarted";
const char kOnOwnAddressRead[] = "OnOwnAddressRead";
const char kOnAdvertisingSetStopped[] = "OnAdvertisingSetStopped";
const char kOnAdvertisingEnabled[] = "OnAdvertisingEnabled";
const char kOnAdvertisingDataSet[] = "OnAdvertisingDataSet";
const char kOnScanResponseDataSet[] = "OnScanResponseDataSet";
const char kOnAdvertisingParametersUpdated[] = "OnAdvertisingParametersUpdated";
const char kOnPeriodicAdvertisingParametersUpdated[] =
    "OnPeriodicAdvertisingParametersUpdated";
const char kOnPeriodicAdvertisingDataSet[] = "OnPeriodicAdvertisingDataSet";
const char kOnPeriodicAdvertisingEnabled[] = "OnPeriodicAdvertisingEnabled";
}  // namespace advertiser

namespace battery_manager {
const char kCallbackInterface[] =
    "org.chromium.bluetooth.BatteryManagerCallback";
const char kRegisterBatteryCallback[] = "RegisterBatteryCallback";
const char kGetBatteryInformation[] = "GetBatteryInformation";

const char kOnBatteryInfoUpdated[] = "OnBatteryInfoUpdated";
}  // namespace battery_manager

namespace admin {
const char kRegisterCallback[] = "RegisterAdminPolicyCallback";
const char kUnregisterCallback[] = "UnregisterAdminPolicyCallback";
const char kCallbackInterface[] = "org.chromium.bluetooth.AdminPolicyCallback";
const char kOnServiceAllowlistChanged[] = "OnServiceAllowlistChanged";
const char kOnDevicePolicyEffectChanged[] = "OnDevicePolicyEffectChanged";
const char kSetAllowedServices[] = "SetAllowedServices";
const char kGetAllowedServices[] = "GetAllowedServices";
const char kGetDevicePolicyEffect[] = "GetDevicePolicyEffect";
}  // namespace admin

namespace adapter_logging {
const char kIsDebugEnabled[] = "IsDebugEnabled";
const char kSetDebugLogging[] = "SetDebugLogging";
}  // namespace adapter_logging

namespace experimental {
const char kSetLLPrivacy[] = "SetLLPrivacy";
const char kSetDevCoredump[] = "SetDevCoredump";
}  // namespace experimental

namespace {
constexpr char kDeviceIdNameKey[] = "name";
constexpr char kDeviceIdAddressKey[] = "address";
}  // namespace

Error::Error(const std::string& name, const std::string& message)
    : name(name), message(message) {}

std::ostream& operator<<(std::ostream& os, const Error& error) {
  os << error.name;

  if (error.name.size() == 0) {
    os << "<no error name>";
  }

  if (error.message.size()) {
    os << ": " << error.message;
  }

  return os;
}

std::string Error::ToString() {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const bool*) {
  static DBusTypeInfo info{"b", "bool"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const uint8_t*) {
  static DBusTypeInfo info{"y", "uint8_t"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const int8_t*) {
  static DBusTypeInfo info{"n", "int8"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const uint16_t*) {
  static DBusTypeInfo info{"q", "uint16"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const int16_t*) {
  static DBusTypeInfo info{"n", "int16"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const uint32_t*) {
  static DBusTypeInfo info{"u", "uint32"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const int32_t*) {
  static DBusTypeInfo info{"i", "int32"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const uint64_t*) {
  static DBusTypeInfo info{"t", "uint64"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const int64_t*) {
  static DBusTypeInfo info{"x", "int64"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const double*) {
  static DBusTypeInfo info{"d", "double"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const std::string*) {
  static DBusTypeInfo info{"s", "string"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const dbus::ObjectPath*) {
  static DBusTypeInfo info{"o", "object_path"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const base::ScopedFD*) {
  static DBusTypeInfo info{"h", "FD"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const FlossDeviceId*) {
  static DBusTypeInfo info{"a{sv}", "FlossDeviceId"};
  return info;
}

template <>
DEVICE_BLUETOOTH_EXPORT const DBusTypeInfo& GetDBusTypeInfo(
    const device::BluetoothUUID*) {
  static DBusTypeInfo info{"ay", "BluetoothUUID"};
  return info;
}

FlossDBusClient::FlossDBusClient() = default;
FlossDBusClient::~FlossDBusClient() = default;

const char FlossDBusClient::kErrorDBus[] = "org.chromium.Error.DBus";
const char FlossDBusClient::kErrorNoResponse[] =
    "org.chromium.Error.NoResponse";
const char FlossDBusClient::kErrorInvalidParameters[] =
    "org.chromium.Error.InvalidParameters";
const char FlossDBusClient::kErrorInvalidReturn[] =
    "org.chromium.Error.InvalidReturn";
const char FlossDBusClient::kErrorDoesNotExist[] =
    "org.chromium.Error.DoesNotExist";
const char FlossDBusClient::kOptionalValueKey[] = "optional_value";

// static
dbus::ObjectPath FlossDBusClient::GenerateAdapterPath(int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kAdapterObjectFormat, adapter_index));
}

// static
dbus::ObjectPath FlossDBusClient::GenerateGattPath(int adapter_index) {
  return dbus::ObjectPath(base::StringPrintf(kGattObjectFormat, adapter_index));
}

// static
dbus::ObjectPath FlossDBusClient::GenerateBatteryManagerPath(
    int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kBatteryManagerObjectFormat, adapter_index));
}

dbus::ObjectPath FlossDBusClient::GenerateAdminPath(int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kAdminObjectFormat, adapter_index));
}

dbus::ObjectPath FlossDBusClient::GenerateLoggingPath(int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kAdapterLoggingObjectFormat, adapter_index));
}

// Default error handler for dbus clients is to just print the error right now.
// TODO(abps) - Deprecate this once error handling is implemented in the upper
//              layers.
void FlossDBusClient::LogErrorResponse(const std::string& message,
                                       dbus::ErrorResponse* error) {
  if (!error) {
    return;
  }

  dbus::MessageReader reader(error);
  auto error_name = error->GetErrorName();
  std::string error_message;
  reader.PopString(&error_message);

  LOG(ERROR) << message << ": " << error_name << ": " << error_message;
}

// static
Error FlossDBusClient::ErrorResponseToError(const std::string& default_name,
                                            const std::string& default_message,
                                            dbus::ErrorResponse* error) {
  Error result(default_name, default_message);

  if (error) {
    dbus::MessageReader reader(error);
    result.name = error->GetErrorName();
    reader.PopString(&result.message);
  }

  return result;
}

// static
// No-op read for a void value.
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader, Void* value) {
  return true;
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader, bool* value) {
  return reader->PopBool(value);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    uint8_t* value) {
  return reader->PopByte(value);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    int8_t* value) {
  int16_t val;
  bool success;

  success = reader->PopInt16(&val);
  *value = static_cast<int8_t>(val);

  return success;
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    uint16_t* value) {
  return reader->PopUint16(value);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    uint32_t* value) {
  return reader->PopUint32(value);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    uint64_t* value) {
  return reader->PopUint64(value);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    int32_t* value) {
  return reader->PopInt32(value);
}

// static
template bool FlossDBusClient::ReadDBusParam<int32_t>(
    dbus::MessageReader* reader,
    absl::optional<int32_t>* value);

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    std::string* value) {
  return reader->PopString(value);
}

// static
template bool FlossDBusClient::ReadDBusParam<std::string>(
    dbus::MessageReader* reader,
    absl::optional<std::string>* value);

// static
template <>
bool FlossDBusClient::ReadDBusParam(
    dbus::MessageReader* reader,
    FlossAdapterClient::BluetoothDeviceType* value) {
  uint32_t val;
  bool success;

  success = reader->PopUint32(&val);
  *value = static_cast<FlossAdapterClient::BluetoothDeviceType>(val);

  return success;
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    device::BluetoothUUID* uuid) {
  const uint8_t* bytes = nullptr;
  size_t length = 0;

  if (reader->PopArrayOfBytes(&bytes, &length)) {
    if (length == 16U) {
      device::BluetoothUUID found_uuid(base::StringPrintf(
          "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%"
          "02x",
          bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
          bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
          bytes[13], bytes[14], bytes[15]));
      DCHECK(found_uuid.IsValid());
      *uuid = found_uuid;
      return true;
    }
  }

  return false;
}

// static
template bool FlossDBusClient::ReadDBusParam<device::BluetoothUUID>(
    dbus::MessageReader* reader,
    absl::optional<device::BluetoothUUID>* uuid);

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossDBusClient::BtifStatus* status) {
  uint32_t raw_type = 0;
  bool read = FlossDBusClient::ReadDBusParam(reader, &raw_type);

  if (read) {
    *status = static_cast<FlossDBusClient::BtifStatus>(raw_type);
  }

  return read;
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    base::ScopedFD* fd) {
  return reader->PopFileDescriptor(fd);
}

// static
template bool FlossDBusClient::ReadDBusParam<base::ScopedFD>(
    dbus::MessageReader* reader,
    absl::optional<base::ScopedFD>* fd);

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossDeviceId* device) {
  static StructReader<FlossDeviceId> struct_reader({
      {"address", CreateFieldReader(&FlossDeviceId::address)},
      {"name", CreateFieldReader(&FlossDeviceId::name)},
  });

  return struct_reader.ReadDBusParam(reader, device);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const FlossDeviceId& device) {
  dbus::MessageWriter array(nullptr);
  dbus::MessageWriter dict(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kDeviceIdNameKey, device.name);
  WriteDictEntry(&array, kDeviceIdAddressKey, device.address);

  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const uint64_t& data) {
  writer->AppendUint64(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const uint32_t& data) {
  writer->AppendUint32(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const uint8_t& data) {
  writer->AppendByte(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const int8_t& data) {
  return writer->AppendInt16(static_cast<int16_t>(data));
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const uint16_t& data) {
  return writer->AppendUint16(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const int16_t& data) {
  return writer->AppendInt16(data);
}

template void FlossDBusClient::WriteDBusParam<uint32_t>(
    dbus::MessageWriter* writer,
    const absl::optional<uint32_t>& data);

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const int32_t& data) {
  writer->AppendInt32(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const std::string& data) {
  writer->AppendString(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const bool& data) {
  writer->AppendBool(data);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const std::vector<uint8_t>& data) {
  writer->AppendArrayOfBytes(data.data(), data.size());
}

template <>
DEVICE_BLUETOOTH_EXPORT void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const device::BluetoothUUID& uuid) {
  WriteDBusParam(writer, uuid.GetBytes());
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const base::ScopedFD& fd) {
  writer->AppendFileDescriptor(fd.get());
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const dbus::ObjectPath& path) {
  writer->AppendObjectPath(path);
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossDBusClient::BtifStatus& status) {
  uint32_t raw_type = static_cast<uint32_t>(status);
  WriteDBusParam(writer, raw_type);
}

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<bool> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<uint8_t> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<uint32_t> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<std::string> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<std::vector<FlossDeviceId>> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<FlossAdapterClient::BluetoothDeviceType> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<device::BluetoothDevice::UUIDList> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<FlossDBusClient::BtifStatus> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

void FlossDBusClient::DefaultResponse(const std::string& caller,
                                      dbus::Response* response,
                                      dbus::ErrorResponse* error_response) {
  if (error_response) {
    FlossDBusClient::LogErrorResponse(caller, error_response);
  } else {
    DVLOG(1) << caller << "::OnResponse";
  }
}

}  // namespace floss
