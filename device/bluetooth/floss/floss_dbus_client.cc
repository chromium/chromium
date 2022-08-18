// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_dbus_client.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/message.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace floss {

// All Floss D-Bus methods return immediately, so the timeout can be very short.
int kDBusTimeoutMs = 2000;

// TODO(b/189499077) - Expose via floss package
const char kAdapterService[] = "org.chromium.bluetooth";
const char kManagerService[] = "org.chromium.bluetooth.Manager";
const char kAdapterInterface[] = "org.chromium.bluetooth.Bluetooth";
const char kManagerInterface[] = "org.chromium.bluetooth.Manager";
const char kManagerObject[] = "/org/chromium/bluetooth/Manager";
const char kAdapterObjectFormat[] = "/org/chromium/bluetooth/hci%d/adapter";

const char kSocketManagerInterface[] = "org.chromium.bluetooth.SocketManager";

namespace adapter {
const char kGetAddress[] = "GetAddress";
const char kGetName[] = "GetName";
const char kSetName[] = "SetName";
const char kGetDiscoverable[] = "GetDiscoverable";
const char kSetDiscoverable[] = "SetDiscoverable";
const char kStartDiscovery[] = "StartDiscovery";
const char kCancelDiscovery[] = "CancelDiscovery";
const char kCreateBond[] = "CreateBond";
const char kCancelBondProcess[] = "CancelBondProcess";
const char kRemoveBond[] = "RemoveBond";
const char kGetRemoteType[] = "GetRemoteType";
const char kGetRemoteClass[] = "GetRemoteClass";
const char kGetConnectionState[] = "GetConnectionState";
const char kGetRemoteUuids[] = "GetRemoteUuids";
const char kGetBondState[] = "GetBondState";
const char kConnectAllEnabledProfiles[] = "ConnectAllEnabledProfiles";
const char kDisconnectAllEnabledProfiles[] = "DisconnectAllEnabledProfiles";
const char kRegisterCallback[] = "RegisterCallback";
const char kRegisterConnectionCallback[] = "RegisterConnectionCallback";
const char kSetPairingConfirmation[] = "SetPairingConfirmation";
const char kSetPin[] = "SetPin";
const char kSetPasskey[] = "SetPasskey";
const char kGetBondedDevices[] = "GetBondedDevices";

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
}  // namespace adapter

namespace manager {
const char kStart[] = "Start";
const char kStop[] = "Stop";
const char kGetFlossEnabled[] = "GetFlossEnabled";
const char kSetFlossEnabled[] = "SetFlossEnabled";
const char kGetState[] = "GetState";
const char kGetAvailableAdapters[] = "GetAvailableAdapters";
const char kRegisterCallback[] = "RegisterCallback";
const char kCallbackInterface[] = "org.chromium.bluetooth.ManagerCallback";
const char kOnHciDeviceChanged[] = "OnHciDeviceChanged";
const char kOnHciEnabledChanged[] = "OnHciEnabledChanged";
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
const DBusTypeInfo& GetDBusTypeInfo<bool>() {
  static DBusTypeInfo info{"b", "bool"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<uint8_t>() {
  static DBusTypeInfo info{"y", "uint8_t"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<int8_t>() {
  static DBusTypeInfo info{"y", "int8"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<uint16_t>() {
  static DBusTypeInfo info{"q", "uint16"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<int16_t>() {
  static DBusTypeInfo info{"n", "int16"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<uint32_t>() {
  static DBusTypeInfo info{"u", "uint32"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<int32_t>() {
  static DBusTypeInfo info{"i", "int32"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<uint64_t>() {
  static DBusTypeInfo info{"t", "uint64"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<int64_t>() {
  static DBusTypeInfo info{"x", "int64"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<double>() {
  static DBusTypeInfo info{"d", "double"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<std::string>() {
  static DBusTypeInfo info{"s", "string"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<dbus::ObjectPath>() {
  static DBusTypeInfo info{"o", "object_path"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<base::ScopedFD>() {
  static DBusTypeInfo info{"h", "FD"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<FlossDeviceId>() {
  static DBusTypeInfo info{"a{sv}", "FlossDeviceId"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<device::BluetoothUUID>() {
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
// Specialization for vector of anything.
template <typename T>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    std::vector<T>* value) {
  dbus::MessageReader subreader(nullptr);
  if (!reader->PopArray(&subreader))
    return false;

  while (subreader.HasMoreData()) {
    T element;
    if (!FlossDBusClient::ReadDBusParam<T>(&subreader, &element))
      return false;

    value->push_back(element);
  }

  return true;
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossDeviceId* device) {
  // Parse a FlossDeviceId from a message.
  //
  // The format:
  // array (
  //  dict_entry (
  //    key "name"
  //    variant string("")
  //  )
  //  dict entry (
  //    key "address"
  //    variant string("")
  //  )
  // )

  dbus::MessageReader array(nullptr);
  dbus::MessageReader dict(nullptr);
  bool found_name = false;
  bool found_address = false;

  if (reader->PopArray(&array)) {
    while (array.PopDictEntry(&dict)) {
      std::string key;
      dict.PopString(&key);

      if (key == kDeviceIdNameKey) {
        found_name = dict.PopVariantOfString(&device->name);
      } else if (key == kDeviceIdAddressKey) {
        found_address = dict.PopVariantOfString(&device->address);
      }
    }
  }

  return found_name && found_address;
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
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const device::BluetoothUUID& uuid) {
  WriteDBusParam(writer, uuid.GetBytes());
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const base::ScopedFD& fd) {
  writer->AppendFileDescriptor(fd.get());
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
