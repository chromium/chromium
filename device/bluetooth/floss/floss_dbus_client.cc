// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
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
int kAdapterEnabledTimeoutMs = 5000;

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
const DBusTypeInfo& GetDBusTypeInfo(
    const FlossAdapterClient::VendorProductInfo*) {
  static DBusTypeInfo info{"a{sv}", "VendorProductInfo"};
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

// static
dbus::ObjectPath FlossDBusClient::GenerateBluetoothTelephonyPath(
    int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kBluetoothTelephonyObjectFormat, adapter_index));
}

dbus::ObjectPath FlossDBusClient::GenerateAdminPath(int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kAdminObjectFormat, adapter_index));
}

dbus::ObjectPath FlossDBusClient::GenerateLoggingPath(int adapter_index) {
  return dbus::ObjectPath(
      base::StringPrintf(kAdapterLoggingObjectFormat, adapter_index));
}

device::BluetoothDevice::ConnectErrorCode
FlossDBusClient::BtifStatusToConnectErrorCode(
    FlossDBusClient::BtifStatus status) {
  switch (status) {
    case BtifStatus::kSuccess:
      NOTREACHED();
    case BtifStatus::kFail:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED;
    case BtifStatus::kNotReady:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_NOT_READY;
    case BtifStatus::kAuthFailure:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED;
    case BtifStatus::kAuthRejected:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED;
    case BtifStatus::kDone:
    case BtifStatus::kBusy:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS;
    case BtifStatus::kUnsupported:
      return device::BluetoothDevice::ConnectErrorCode::
          ERROR_UNSUPPORTED_DEVICE;
    case BtifStatus::kNomem:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_NO_MEMORY;
    case BtifStatus::kParmInvalid:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_INVALID_ARGS;
    case BtifStatus::kUnhandled:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN;
    case BtifStatus::kRmtDevDown:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_DOES_NOT_EXIST;
    case BtifStatus::kJniEnvironmentError:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_ENVIRONMENT;
    case BtifStatus::kJniThreadAttachError:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_THREAD_ATTACH;
    case BtifStatus::kWakelockError:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_WAKELOCK;
    case BtifStatus::kTimeout:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_NON_AUTH_TIMEOUT;
    case BtifStatus::kDeviceNotFound:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_DOES_NOT_EXIST;
    case BtifStatus::kUnexpectedState:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_UNEXPECTED_STATE;
    case BtifStatus::kSocketError:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_SOCKET;
    default:
      return device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED;
  }
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
    std::optional<int32_t>* value);

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    std::string* value) {
  return reader->PopString(value);
}

// static
template bool FlossDBusClient::ReadDBusParam<std::string>(
    dbus::MessageReader* reader,
    std::optional<std::string>* value);

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
    std::optional<device::BluetoothUUID>* uuid);

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
    std::optional<base::ScopedFD>* fd);

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

// static
template <>
bool FlossDBusClient::ReadDBusParam(
    dbus::MessageReader* reader,
    FlossAdapterClient::VendorProductInfo* vpi) {
  static StructReader<FlossAdapterClient::VendorProductInfo> struct_reader({
      {"vendor_id_src",
       CreateFieldReader(&FlossAdapterClient::VendorProductInfo::vendorIdSrc)},
      {"vendor_id",
       CreateFieldReader(&FlossAdapterClient::VendorProductInfo::vendorId)},
      {"product_id",
       CreateFieldReader(&FlossAdapterClient::VendorProductInfo::productId)},
      {"version",
       CreateFieldReader(&FlossAdapterClient::VendorProductInfo::version)},
  });

  return struct_reader.ReadDBusParam(reader, vpi);
}

// static
template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossAdapterClient::BtAddressType* type) {
  uint32_t val;
  bool success;

  success = reader->PopUint32(&val);
  *type = static_cast<FlossAdapterClient::BtAddressType>(val);

  return success;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossAdapterClient::BtAdapterRole* type) {
  uint32_t val;
  bool success;

  success = reader->PopUint32(&val);
  *type = static_cast<FlossAdapterClient::BtAdapterRole>(val);

  return success;
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
    const std::optional<uint32_t>& data);

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
  writer->AppendArrayOfBytes(data);
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
    ResponseCallback<FlossAdapterClient::VendorProductInfo> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<FlossAdapterClient::BtAddressType> callback,
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
