// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_client.h"

#include "base/notreached.h"

namespace floss {

namespace {
// Randomly generated UUID for use in this client.
constexpr char kDefaultGattClientUuid[] =
    "e060b902-508c-485f-8b0e-27639c7f2d41";

// Default to requesting eatt support with gatt client.
constexpr bool kDefaultEattSupport = true;

void HandleResponse(const char* method, DBusResult<Void> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Call failed: " << method << ": " << result.error();
    return;
  }

  DVLOG(1) << method << " succeeded.";
}
}  // namespace

// Template specializations for dbus parsing

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader, LePhy* phy) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *phy = static_cast<LePhy>(value);
    return true;
  }

  return false;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<LePhy>() {
  static DBusTypeInfo info{"u", "LePhy"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const LePhy& phy) {
  uint32_t value = static_cast<uint32_t>(phy);
  WriteDBusParam(writer, value);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattStatus* status) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *status = static_cast<GattStatus>(value);
    return true;
  }

  return false;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<GattStatus>() {
  static DBusTypeInfo info{"u", "GattStatus"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const AuthRequired& auth_req) {
  uint32_t value = static_cast<uint32_t>(auth_req);
  WriteDBusParam(writer, value);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    WriteType* write_type) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *write_type = static_cast<WriteType>(value);
    return true;
  }

  return false;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const WriteType& write_type) {
  uint32_t value = static_cast<uint32_t>(write_type);
  WriteDBusParam(writer, value);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattDescriptor* descriptor) {
  static FlossDBusClient::StructReader<GattDescriptor> struct_reader({
      {"uuid", CreateFieldReader(&GattDescriptor::uuid)},
      {"instance_id", CreateFieldReader(&GattDescriptor::instance_id)},
      {"permissions", CreateFieldReader(&GattDescriptor::permissions)},
  });

  return struct_reader.ReadDBusParam(reader, descriptor);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<GattDescriptor>() {
  static DBusTypeInfo info{"a{sv}", "GattDescriptor"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattCharacteristic* characteristic) {
  static FlossDBusClient::StructReader<GattCharacteristic> struct_reader({
      {"uuid", CreateFieldReader(&GattCharacteristic::uuid)},
      {"instance_id", CreateFieldReader(&GattCharacteristic::instance_id)},
      {"properties", CreateFieldReader(&GattCharacteristic::properties)},
      {"key_size", CreateFieldReader(&GattCharacteristic::key_size)},
      {"write_type", CreateFieldReader(&GattCharacteristic::write_type)},
      {"descriptors", CreateFieldReader(&GattCharacteristic::descriptors)},
  });

  return struct_reader.ReadDBusParam(reader, characteristic);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<GattCharacteristic>() {
  static DBusTypeInfo info{"a{sv}", "GattCharacteristic"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattService* service) {
  static FlossDBusClient::StructReader<GattService> struct_reader({
      {"uuid", CreateFieldReader(&GattService::uuid)},
      {"instance_id", CreateFieldReader(&GattService::instance_id)},
      {"service_type", CreateFieldReader(&GattService::service_type)},
      {"characteristics", CreateFieldReader(&GattService::characteristics)},
      {"included_services", CreateFieldReader(&GattService::included_services)},
  });

  return struct_reader.ReadDBusParam(reader, service);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<GattService>() {
  static DBusTypeInfo info{"a{sv}", "GattService"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo<std::vector<GattService>>() {
  static DBusTypeInfo info{"av", "std::vector<GattService>"};
  return info;
}

GattDescriptor::GattDescriptor() = default;
GattDescriptor::~GattDescriptor() = default;

GattCharacteristic::GattCharacteristic() = default;
GattCharacteristic::GattCharacteristic(const GattCharacteristic&) = default;
GattCharacteristic::~GattCharacteristic() = default;

GattService::GattService() = default;
GattService::GattService(const GattService&) = default;
GattService::~GattService() = default;

const char FlossGattClient::kExportedCallbacksPath[] =
    "/org/chromium/bluetooth/gattclient";

// static
std::unique_ptr<FlossGattClient> FlossGattClient::Create() {
  return std::make_unique<FlossGattClient>();
}

FlossGattClient::FlossGattClient() = default;
FlossGattClient::~FlossGattClient() {
  if (bus_) {
    exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kExportedCallbacksPath));
  }
}

void FlossGattClient::Connect(ResponseCallback<Void> callback,
                              const std::string& remote_device,
                              const BluetoothTransport& transport) {
  // Gatt client connections occur immediately instead of when next seen.
  const bool is_direct = true;

  // Opportunistic connections.
  const bool opportunistic = true;

  // We want a phy to be chosen automatically.
  const LePhy phy = LePhy::kInvalid;

  CallGattMethod<Void>(std::move(callback), gatt::kClientConnect, client_id_,
                       remote_device, is_direct, transport, opportunistic, phy);
}

void FlossGattClient::Disconnect(ResponseCallback<Void> callback,
                                 const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kClientDisconnect, client_id_,
                       remote_device);
}

void FlossGattClient::Refresh(ResponseCallback<Void> callback,
                              const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kRefreshDevice, client_id_,
                       remote_device);
}

void FlossGattClient::DiscoverAllServices(ResponseCallback<Void> callback,
                                          const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kDiscoverServices, client_id_,
                       remote_device);
}

void FlossGattClient::DiscoverServiceByUuid(ResponseCallback<Void> callback,
                                            const std::string& remote_device,
                                            const device::BluetoothUUID& uuid) {
  CallGattMethod<Void>(std::move(callback), gatt::kDiscoverServiceByUuid,
                       client_id_, remote_device, uuid);
}

void FlossGattClient::ReadCharacteristic(ResponseCallback<Void> callback,
                                         const std::string& remote_device,
                                         const int32_t handle,
                                         const AuthRequired auth_required) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadCharacteristic,
                       client_id_, remote_device, handle, auth_required);
}

void FlossGattClient::ReadUsingCharacteristicUuid(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    const device::BluetoothUUID& uuid,
    const int32_t start_handle,
    const int32_t end_handle,
    const AuthRequired auth_required) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadUsingCharacteristicUuid,
                       client_id_, remote_device, uuid, start_handle,
                       end_handle, auth_required);
}

void FlossGattClient::WriteCharacteristic(ResponseCallback<Void> callback,
                                          const std::string& remote_device,
                                          const int32_t handle,
                                          const WriteType write_type,
                                          const AuthRequired auth_required,
                                          const std::vector<uint8_t> data) {
  CallGattMethod<Void>(std::move(callback), gatt::kWriteCharacteristic,
                       client_id_, remote_device, handle, write_type,
                       auth_required, data);
}

void FlossGattClient::ReadDescriptor(ResponseCallback<Void> callback,
                                     const std::string& remote_device,
                                     const int32_t handle,
                                     const AuthRequired auth_required) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadDescriptor, client_id_,
                       remote_device, handle, auth_required);
}

void FlossGattClient::WriteDescriptor(ResponseCallback<Void> callback,
                                      const std::string& remote_device,
                                      const int32_t handle,
                                      const AuthRequired auth_required,
                                      const std::vector<uint8_t> data) {
  CallGattMethod<Void>(std::move(callback), gatt::kWriteDescriptor, client_id_,
                       remote_device, handle, auth_required, data);
}

void FlossGattClient::RegisterForNotification(ResponseCallback<Void> callback,
                                              const std::string& remote_device,
                                              const int32_t handle) {
  const bool enable_notification = true;
  CallGattMethod<Void>(std::move(callback), gatt::kRegisterForNotification,
                       client_id_, remote_device, handle, enable_notification);
}

void FlossGattClient::UnregisterNotification(ResponseCallback<Void> callback,
                                             const std::string& remote_device,
                                             const int32_t handle) {
  const bool enable_notification = false;
  CallGattMethod<Void>(std::move(callback), gatt::kRegisterForNotification,
                       client_id_, remote_device, handle, enable_notification);
}

void FlossGattClient::ReadRemoteRssi(ResponseCallback<Void> callback,
                                     const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadRemoteRssi, client_id_,
                       remote_device);
}

void FlossGattClient::ConfigureMTU(ResponseCallback<Void> callback,
                                   const std::string& remote_device,
                                   const int32_t mtu) {
  CallGattMethod<Void>(std::move(callback), gatt::kConfigureMtu, client_id_,
                       remote_device, mtu);
}

void FlossGattClient::Init(dbus::Bus* bus,
                           const std::string& service_name,
                           const int adapter_index) {
  bus_ = bus;
  service_name_ = service_name;
  gatt_adapter_path_ = GenerateGattPath(adapter_index);

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, gatt_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossGattClient couldn't init. Object proxy was null.";
    return;
  }

  exported_callback_manager_.Init(bus_.get());
  exported_callback_manager_.AddMethod(
      gatt::kOnClientRegistered, &FlossGattClientCallbacks::OnClientRegistered);
  exported_callback_manager_.AddMethod(
      gatt::kOnClientConnectionState,
      &FlossGattClientCallbacks::OnClientConnectionState);
  exported_callback_manager_.AddMethod(gatt::kOnPhyUpdate,
                                       &FlossGattClientCallbacks::OnPhyUpdate);
  exported_callback_manager_.AddMethod(gatt::kOnPhyRead,
                                       &FlossGattClientCallbacks::OnPhyRead);
  exported_callback_manager_.AddMethod(
      gatt::kOnSearchComplete, &FlossGattClientCallbacks::OnSearchComplete);
  exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicRead,
      &FlossGattClientCallbacks::OnCharacteristicRead);
  exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicWrite,
      &FlossGattClientCallbacks::OnCharacteristicWrite);
  exported_callback_manager_.AddMethod(
      gatt::kOnExecuteWrite, &FlossGattClientCallbacks::OnExecuteWrite);
  exported_callback_manager_.AddMethod(
      gatt::kOnDescriptorRead, &FlossGattClientCallbacks::OnDescriptorRead);
  exported_callback_manager_.AddMethod(gatt::kOnNotify,
                                       &FlossGattClientCallbacks::OnNotify);
  exported_callback_manager_.AddMethod(
      gatt::kOnReadRemoteRssi, &FlossGattClientCallbacks::OnReadRemoteRssi);
  exported_callback_manager_.AddMethod(
      gatt::kOnConfigureMtu, &FlossGattClientCallbacks::OnConfigureMtu);
  exported_callback_manager_.AddMethod(
      gatt::kOnConnectionUpdated,
      &FlossGattClientCallbacks::OnConnectionUpdated);
  exported_callback_manager_.AddMethod(
      gatt::kOnServiceChanged, &FlossGattClientCallbacks::OnServiceChanged);

  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr())) {
    LOG(ERROR) << "Unable to successfully export FlossGattClientCallbacks.";
    return;
  }

  // Finish registering client. We will get client id via |OnClientRegistered|.
  CallGattMethod<Void>(
      base::BindOnce(&HandleResponse, gatt::kRegisterClient),
      gatt::kRegisterClient, device::BluetoothUUID(kDefaultGattClientUuid),
      dbus::ObjectPath(kExportedCallbacksPath), kDefaultEattSupport);
}

void FlossGattClient::OnClientRegistered(int32_t status, int32_t client_id) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnClientConnectionState(int32_t status,
                                              int32_t client_id,
                                              bool connected,
                                              std::string address) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnPhyUpdate(std::string address,
                                  LePhy tx,
                                  LePhy rx,
                                  GattStatus status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnPhyRead(std::string address,
                                LePhy tx,
                                LePhy rx,
                                GattStatus status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnSearchComplete(std::string address,
                                       std::vector<GattService> services,
                                       int32_t status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnCharacteristicRead(std::string address,
                                           int32_t status,
                                           int32_t handle,
                                           std::vector<uint8_t> data) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnCharacteristicWrite(std::string address,
                                            int32_t status,
                                            int32_t handle) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnExecuteWrite(std::string address, int32_t status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnDescriptorRead(std::string address,
                                       int32_t status,
                                       int32_t handle,
                                       std::vector<uint8_t> data) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnDescriptorWrite(std::string address,
                                        int32_t status,
                                        int32_t handle) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnNotify(std::string address,
                               int32_t handle,
                               std::vector<uint8_t> data) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnReadRemoteRssi(std::string address,
                                       int32_t rssi,
                                       int32_t status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnConfigureMtu(std::string address,
                                     int32_t mtu,
                                     int32_t status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnConnectionUpdated(std::string address,
                                          int32_t interval,
                                          int32_t latency,
                                          int32_t timeout,
                                          int32_t status) {
  NOTIMPLEMENTED();
}

void FlossGattClient::OnServiceChanged(std::string address) {
  NOTIMPLEMENTED();
}

}  // namespace floss
