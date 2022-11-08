// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_client.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace floss {

namespace {
// Randomly generated UUID for use in this client.
constexpr char kDefaultGattClientUuid[] = "e060b902508c485f8b0e27639c7f2d41";

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
const DBusTypeInfo& GetDBusTypeInfo(const LePhy*) {
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
const DBusTypeInfo& GetDBusTypeInfo(const GattStatus*) {
  static DBusTypeInfo info{"u", "GattStatus"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const AuthRequired& auth_req) {
  int32_t value = static_cast<int32_t>(auth_req);
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
const DBusTypeInfo& GetDBusTypeInfo(const GattDescriptor*) {
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
const DBusTypeInfo& GetDBusTypeInfo(const GattCharacteristic*) {
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
const DBusTypeInfo& GetDBusTypeInfo(const GattService*) {
  static DBusTypeInfo info{"a{sv}", "GattService"};
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

void FlossGattClient::AddObserver(FlossGattClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossGattClient::RemoveObserver(FlossGattClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FlossGattClient::Connect(ResponseCallback<Void> callback,
                              const std::string& remote_device,
                              const BluetoothTransport& transport) {
  // Gatt client connections occur immediately instead of when next seen.
  const bool is_direct = true;

  // Opportunistic connections should be false because we want connections to
  // immediately fail with timeout if it doesn't work out.
  const bool opportunistic = false;

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
                       client_id_, remote_device, uuid.canonical_value());
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
                       client_id_, remote_device, uuid.canonical_value(),
                       start_handle, end_handle, auth_required);
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

void FlossGattClient::RegisterForNotification(
    ResponseCallback<GattStatus> callback,
    const std::string& remote_device,
    const int32_t handle) {
  const bool enable_notification = true;
  CallGattMethod<Void>(
      base::BindOnce(&FlossGattClient::OnRegisterNotificationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     enable_notification),
      gatt::kRegisterForNotification, client_id_, remote_device, handle,
      enable_notification);
}

void FlossGattClient::UnregisterNotification(
    ResponseCallback<GattStatus> callback,
    const std::string& remote_device,
    const int32_t handle) {
  const bool enable_notification = false;
  CallGattMethod<Void>(
      base::BindOnce(&FlossGattClient::OnRegisterNotificationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     enable_notification),
      gatt::kRegisterForNotification, client_id_, remote_device, handle,
      enable_notification);
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

void FlossGattClient::UpdateConnectionParameters(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    const int32_t min_interval,
    const int32_t max_interval,
    const int32_t latency,
    const int32_t timeout,
    const uint16_t min_ce_len,
    const uint16_t max_ce_len) {
  CallGattMethod<Void>(std::move(callback), gatt::kConnectionParameterUpdate,
                       client_id_, remote_device, min_interval, max_interval,
                       latency, timeout, min_ce_len, max_ce_len);
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
      gatt::kOnClientRegistered,
      &FlossGattClientObserver::GattClientRegistered);
  exported_callback_manager_.AddMethod(
      gatt::kOnClientConnectionState,
      &FlossGattClientObserver::GattClientConnectionState);
  exported_callback_manager_.AddMethod(gatt::kOnPhyUpdate,
                                       &FlossGattClientObserver::GattPhyUpdate);
  exported_callback_manager_.AddMethod(gatt::kOnPhyRead,
                                       &FlossGattClientObserver::GattPhyRead);
  exported_callback_manager_.AddMethod(
      gatt::kOnSearchComplete, &FlossGattClientObserver::GattSearchComplete);
  exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicRead,
      &FlossGattClientObserver::GattCharacteristicRead);
  exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicWrite,
      &FlossGattClientObserver::GattCharacteristicWrite);
  exported_callback_manager_.AddMethod(
      gatt::kOnExecuteWrite, &FlossGattClientObserver::GattExecuteWrite);
  exported_callback_manager_.AddMethod(
      gatt::kOnDescriptorRead, &FlossGattClientObserver::GattDescriptorRead);
  exported_callback_manager_.AddMethod(
      gatt::kOnDescriptorWrite, &FlossGattClientObserver::GattDescriptorWrite);
  exported_callback_manager_.AddMethod(gatt::kOnNotify,
                                       &FlossGattClientObserver::GattNotify);
  exported_callback_manager_.AddMethod(
      gatt::kOnReadRemoteRssi, &FlossGattClientObserver::GattReadRemoteRssi);
  exported_callback_manager_.AddMethod(
      gatt::kOnConfigureMtu, &FlossGattClientObserver::GattConfigureMtu);
  exported_callback_manager_.AddMethod(
      gatt::kOnConnectionUpdated,
      &FlossGattClientObserver::GattConnectionUpdated);
  exported_callback_manager_.AddMethod(
      gatt::kOnServiceChanged, &FlossGattClientObserver::GattServiceChanged);

  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr())) {
    LOG(ERROR) << "Unable to successfully export FlossGattClientObserver.";
    return;
  }

  RegisterClient();
}

void FlossGattClient::RegisterClient() {
  // Finish registering client. We will get client id via
  // |GattClientRegistered|.
  CallGattMethod<Void>(
      base::BindOnce(&HandleResponse, gatt::kRegisterClient),
      gatt::kRegisterClient, std::string(kDefaultGattClientUuid),
      dbus::ObjectPath(kExportedCallbacksPath), kDefaultEattSupport);
}

void FlossGattClient::GattClientRegistered(GattStatus status,
                                           int32_t client_id) {
  if (client_id_ != 0) {
    LOG(ERROR) << "Unexpected GattClientRegistered with id = " << client_id
               << " when we already have id = " << client_id_;
    return;
  }

  if (status != GattStatus::kSuccess) {
    LOG(ERROR) << "RegisterClient failed with status = "
               << static_cast<uint32_t>(status);
    return;
  }

  client_id_ = client_id;
}

void FlossGattClient::GattClientConnectionState(GattStatus status,
                                                int32_t client_id,
                                                bool connected,
                                                std::string address) {
  // Ignore updates for other clients.
  if (client_id != client_id_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.GattClientConnectionState(status, client_id, connected, address);
  }
}

void FlossGattClient::GattPhyUpdate(std::string address,
                                    LePhy tx,
                                    LePhy rx,
                                    GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattPhyUpdate(address, tx, rx, status);
  }
}

void FlossGattClient::GattPhyRead(std::string address,
                                  LePhy tx,
                                  LePhy rx,
                                  GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattPhyRead(address, tx, rx, status);
  }
}

void FlossGattClient::GattSearchComplete(
    std::string address,
    const std::vector<GattService>& services,
    GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattSearchComplete(address, services, status);
  }
}

void FlossGattClient::GattCharacteristicRead(std::string address,
                                             GattStatus status,
                                             int32_t handle,
                                             const std::vector<uint8_t>& data) {
  for (auto& observer : observers_) {
    observer.GattCharacteristicRead(address, status, handle, data);
  }
}

void FlossGattClient::GattCharacteristicWrite(std::string address,
                                              GattStatus status,
                                              int32_t handle) {
  for (auto& observer : observers_) {
    observer.GattCharacteristicWrite(address, status, handle);
  }
}

void FlossGattClient::GattExecuteWrite(std::string address, GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattExecuteWrite(address, status);
  }
}

void FlossGattClient::GattDescriptorRead(std::string address,
                                         GattStatus status,
                                         int32_t handle,
                                         const std::vector<uint8_t>& data) {
  for (auto& observer : observers_) {
    observer.GattDescriptorRead(address, status, handle, data);
  }
}

void FlossGattClient::GattDescriptorWrite(std::string address,
                                          GattStatus status,
                                          int32_t handle) {
  for (auto& observer : observers_) {
    observer.GattDescriptorWrite(address, status, handle);
  }
}

void FlossGattClient::GattNotify(std::string address,
                                 int32_t handle,
                                 const std::vector<uint8_t>& data) {
  for (auto& observer : observers_) {
    observer.GattNotify(address, handle, data);
  }
}

void FlossGattClient::GattReadRemoteRssi(std::string address,
                                         int32_t rssi,
                                         GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattReadRemoteRssi(address, rssi, status);
  }
}

void FlossGattClient::GattConfigureMtu(std::string address,
                                       int32_t mtu,
                                       GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattConfigureMtu(address, mtu, status);
  }
}

void FlossGattClient::GattConnectionUpdated(std::string address,
                                            int32_t interval,
                                            int32_t latency,
                                            int32_t timeout,
                                            GattStatus status) {
  for (auto& observer : observers_) {
    observer.GattConnectionUpdated(address, interval, latency, timeout, status);
  }
}

void FlossGattClient::GattServiceChanged(std::string address) {
  for (auto& observer : observers_) {
    observer.GattServiceChanged(address);
  }
}

// TODO(b/193685841) - Floss currently doesn't emit a callback when
// a notification registers. Once a callback is available, we should report that
// via the callback here instead.
void FlossGattClient::OnRegisterNotificationResponse(
    ResponseCallback<GattStatus> callback,
    bool is_registering,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    std::move(callback).Run(GattStatus::kError);
    return;
  }

  std::move(callback).Run(GattStatus::kSuccess);
}

}  // namespace floss
