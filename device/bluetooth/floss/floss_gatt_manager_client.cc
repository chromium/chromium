// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_manager_client.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace floss {
namespace {
constexpr char kCharacteristics[] = "characteristics";
constexpr char kDescriptors[] = "descriptors";
constexpr char kIncludedServices[] = "included_services";
constexpr char kInstanceId[] = "instance_id";
constexpr char kKeySize[] = "key_size";
constexpr char kPermissions[] = "permissions";
constexpr char kProperties[] = "properties";
constexpr char kServiceType[] = "service_type";
constexpr char kUuid[] = "uuid";
constexpr char kWriteType[] = "write_type";
}  // namespace

namespace {
// Randomly generated UUID for use in this client.
constexpr char kDefaultGattManagerClientUuid[] =
// Ash and LaCrOS should use the different APP UUID, otherwise the latter one
// (usually LaCrOS) fails on registering.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "e060b902508c485f8b0e27639c7f2d41";
#else
    "58523f357dbc4390ab78ed075b15a634";
#endif

// Default to not requesting eatt support with gatt client.
constexpr bool kDefaultEattSupport = false;

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
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    LeDiscoverableMode* mode) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *mode = static_cast<LeDiscoverableMode>(value);
    return true;
  }

  return false;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const LeDiscoverableMode*) {
  static DBusTypeInfo info{"u", "LeDiscoverableMode"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const LeDiscoverableMode& mode) {
  uint32_t value = static_cast<uint32_t>(mode);
  WriteDBusParam(writer, value);
}

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
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const GattStatus& status) {
  uint32_t value = static_cast<uint32_t>(status);
  WriteDBusParam(writer, value);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const GattStatus*) {
  static DBusTypeInfo info{"u", "GattStatus"};
  return info;
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattWriteRequestStatus* status) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *status = static_cast<GattWriteRequestStatus>(value);
    return true;
  }

  return false;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const GattWriteRequestStatus*) {
  static DBusTypeInfo info{"u", "GattWriteRequestStatus"};
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
const DBusTypeInfo& GetDBusTypeInfo(const WriteType*) {
  static DBusTypeInfo info{"u", "WriteType"};
  return info;
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const WriteType& write_type) {
  uint32_t value = static_cast<uint32_t>(write_type);
  WriteDBusParam(writer, value);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const GattService& service) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);
  WriteDictEntry(&array, kUuid, service.uuid);
  WriteDictEntry(&array, kInstanceId, service.instance_id);
  WriteDictEntry(&array, kServiceType, service.service_type);
  WriteDictEntry(&array, kCharacteristics, service.characteristics);
  WriteDictEntry(&array, kIncludedServices, service.included_services);
  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const GattCharacteristic& characteristic) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);
  WriteDictEntry(&array, kUuid, characteristic.uuid);
  WriteDictEntry(&array, kInstanceId, characteristic.instance_id);
  WriteDictEntry(&array, kProperties, characteristic.properties);
  WriteDictEntry(&array, kPermissions, characteristic.permissions);
  WriteDictEntry(&array, kKeySize, characteristic.key_size);
  WriteDictEntry(&array, kWriteType, characteristic.write_type);
  WriteDictEntry(&array, kDescriptors, characteristic.descriptors);
  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const GattDescriptor& descriptor) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);
  WriteDictEntry(&array, kUuid, descriptor.uuid);
  WriteDictEntry(&array, kInstanceId, descriptor.instance_id);
  WriteDictEntry(&array, kPermissions, descriptor.permissions);
  writer->CloseContainer(&array);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    GattDescriptor* descriptor) {
  static FlossDBusClient::StructReader<GattDescriptor> struct_reader({
      {kUuid, CreateFieldReader(&GattDescriptor::uuid)},
      {kInstanceId, CreateFieldReader(&GattDescriptor::instance_id)},
      {kPermissions, CreateFieldReader(&GattDescriptor::permissions)},
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
      {kUuid, CreateFieldReader(&GattCharacteristic::uuid)},
      {kInstanceId, CreateFieldReader(&GattCharacteristic::instance_id)},
      {kProperties, CreateFieldReader(&GattCharacteristic::properties)},
      {kKeySize, CreateFieldReader(&GattCharacteristic::key_size)},
      {kWriteType, CreateFieldReader(&GattCharacteristic::write_type)},
      {kDescriptors, CreateFieldReader(&GattCharacteristic::descriptors)},
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
      {kUuid, CreateFieldReader(&GattService::uuid)},
      {kInstanceId, CreateFieldReader(&GattService::instance_id)},
      {kServiceType, CreateFieldReader(&GattService::service_type)},
      {kCharacteristics, CreateFieldReader(&GattService::characteristics)},
      {kIncludedServices, CreateFieldReader(&GattService::included_services)},
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

const char FlossGattManagerClient::kExportedCallbacksPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/gatt/callback/lacros";
#else
    "/org/chromium/bluetooth/gatt/callback";
#endif

// static
std::unique_ptr<FlossGattManagerClient> FlossGattManagerClient::Create() {
  return std::make_unique<FlossGattManagerClient>();
}

FlossGattManagerClient::FlossGattManagerClient() = default;
FlossGattManagerClient::~FlossGattManagerClient() {
  if (bus_) {
    gatt_client_exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kExportedCallbacksPath));
    gatt_server_exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kExportedCallbacksPath));
  }

  if (client_id_ != 0) {
    CallGattMethod<Void>(base::DoNothing(), gatt::kUnregisterClient,
                         client_id_);
  }
  if (server_id_ != 0) {
    CallGattMethod<Void>(base::DoNothing(), gatt::kUnregisterServer,
                         server_id_);
  }
}

void FlossGattManagerClient::AddObserver(FlossGattClientObserver* observer) {
  gatt_client_observers_.AddObserver(observer);
}

void FlossGattManagerClient::AddServerObserver(
    FlossGattServerObserver* observer) {
  gatt_server_observers_.AddObserver(observer);
}

void FlossGattManagerClient::RemoveObserver(FlossGattClientObserver* observer) {
  gatt_client_observers_.RemoveObserver(observer);
}

void FlossGattManagerClient::RemoveServerObserver(
    FlossGattServerObserver* observer) {
  gatt_server_observers_.RemoveObserver(observer);
}

void FlossGattManagerClient::Connect(ResponseCallback<Void> callback,
                                     const std::string& remote_device,
                                     const BluetoothTransport& transport,
                                     bool is_direct) {
  // Opportunistic connections should be false because we want connections to
  // immediately fail with timeout if it doesn't work out.
  const bool opportunistic = false;

  // We want a phy to be chosen automatically.
  const LePhy phy = LePhy::kInvalid;

  CallGattMethod<Void>(std::move(callback), gatt::kClientConnect, client_id_,
                       remote_device, is_direct, transport, opportunistic, phy);
}

void FlossGattManagerClient::Disconnect(ResponseCallback<Void> callback,
                                        const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kClientDisconnect, client_id_,
                       remote_device);
}

void FlossGattManagerClient::BeginReliableWrite(
    ResponseCallback<Void> callback,
    const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kBeginReliableWrite,
                       client_id_, remote_device);
}

void FlossGattManagerClient::EndReliableWrite(ResponseCallback<Void> callback,
                                              const std::string& remote_device,
                                              bool execute) {
  CallGattMethod<Void>(std::move(callback), gatt::kEndReliableWrite, client_id_,
                       remote_device, execute);
}

void FlossGattManagerClient::Refresh(ResponseCallback<Void> callback,
                                     const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kRefreshDevice, client_id_,
                       remote_device);
}

void FlossGattManagerClient::DiscoverAllServices(
    ResponseCallback<Void> callback,
    const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kDiscoverServices, client_id_,
                       remote_device);
}

void FlossGattManagerClient::DiscoverServiceByUuid(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    const device::BluetoothUUID& uuid) {
  CallGattMethod<Void>(std::move(callback), gatt::kDiscoverServiceByUuid,
                       client_id_, remote_device, uuid.canonical_value());
}

void FlossGattManagerClient::ReadCharacteristic(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    const int32_t handle,
    const AuthRequired auth_required) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadCharacteristic,
                       client_id_, remote_device, handle, auth_required);
}

void FlossGattManagerClient::ReadUsingCharacteristicUuid(
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

void FlossGattManagerClient::WriteCharacteristic(
    ResponseCallback<GattWriteRequestStatus> callback,
    const std::string& remote_device,
    const int32_t handle,
    const WriteType write_type,
    const AuthRequired auth_required,
    const std::vector<uint8_t> data) {
  CallGattMethod(std::move(callback), gatt::kWriteCharacteristic, client_id_,
                 remote_device, handle, write_type, auth_required, data);
}

void FlossGattManagerClient::ReadDescriptor(ResponseCallback<Void> callback,
                                            const std::string& remote_device,
                                            const int32_t handle,
                                            const AuthRequired auth_required) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadDescriptor, client_id_,
                       remote_device, handle, auth_required);
}

void FlossGattManagerClient::WriteDescriptor(ResponseCallback<Void> callback,
                                             const std::string& remote_device,
                                             const int32_t handle,
                                             const AuthRequired auth_required,
                                             const std::vector<uint8_t> data) {
  CallGattMethod<Void>(std::move(callback), gatt::kWriteDescriptor, client_id_,
                       remote_device, handle, auth_required, data);
}

void FlossGattManagerClient::RegisterForNotification(
    ResponseCallback<GattStatus> callback,
    const std::string& remote_device,
    const int32_t handle) {
  const bool enable_notification = true;
  CallGattMethod<Void>(
      base::BindOnce(&FlossGattManagerClient::OnRegisterNotificationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     enable_notification),
      gatt::kRegisterForNotification, client_id_, remote_device, handle,
      enable_notification);
}

void FlossGattManagerClient::UnregisterNotification(
    ResponseCallback<GattStatus> callback,
    const std::string& remote_device,
    const int32_t handle) {
  const bool enable_notification = false;
  CallGattMethod<Void>(
      base::BindOnce(&FlossGattManagerClient::OnRegisterNotificationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     enable_notification),
      gatt::kRegisterForNotification, client_id_, remote_device, handle,
      enable_notification);
}

void FlossGattManagerClient::ReadRemoteRssi(ResponseCallback<Void> callback,
                                            const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kReadRemoteRssi, client_id_,
                       remote_device);
}

void FlossGattManagerClient::ConfigureMTU(ResponseCallback<Void> callback,
                                          const std::string& remote_device,
                                          const int32_t mtu) {
  CallGattMethod<Void>(std::move(callback), gatt::kConfigureMtu, client_id_,
                       remote_device, mtu);
}

void FlossGattManagerClient::UpdateConnectionParameters(
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

void FlossGattManagerClient::ServerConnect(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    const BluetoothTransport& transport) {
  // Connect immediately, instead of when next seen.
  constexpr bool is_direct = true;

  CallGattMethod<Void>(std::move(callback), gatt::kServerConnect, server_id_,
                       remote_device, is_direct, transport);
}

void FlossGattManagerClient::ServerDisconnect(
    ResponseCallback<Void> callback,
    const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kServerDisconnect, server_id_,
                       remote_device);
}

void FlossGattManagerClient::ServerSetPreferredPhy(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    LePhy tx_phy,
    LePhy rx_phy,
    int32_t phy_options) {
  CallGattMethod<Void>(std::move(callback), gatt::kServerSetPreferredPhy,
                       server_id_, remote_device, tx_phy, rx_phy, phy_options);
}

void FlossGattManagerClient::ServerReadPhy(ResponseCallback<Void> callback,
                                           const std::string& remote_device) {
  CallGattMethod<Void>(std::move(callback), gatt::kServerReadPhy, server_id_,
                       remote_device);
}

void FlossGattManagerClient::AddService(ResponseCallback<Void> callback,
                                        GattService service) {
  CallGattMethod<Void>(std::move(callback), gatt::kAddService, server_id_,
                       service);
}

void FlossGattManagerClient::RemoveService(ResponseCallback<Void> callback,
                                           int32_t handle) {
  CallGattMethod<Void>(std::move(callback), gatt::kRemoveService, server_id_,
                       handle);
}

void FlossGattManagerClient::ClearServices(ResponseCallback<Void> callback) {
  CallGattMethod<Void>(std::move(callback), gatt::kClearServices, server_id_);
}

void FlossGattManagerClient::SendResponse(ResponseCallback<Void> callback,
                                          const std::string& remote_device,
                                          int32_t request_id,
                                          GattStatus status,
                                          int32_t offset,
                                          std::vector<uint8_t> value) {
  CallGattMethod<Void>(std::move(callback), gatt::kSendResponse, server_id_,
                       remote_device, request_id, status, offset, value);
}

void FlossGattManagerClient::ServerSendNotification(
    ResponseCallback<Void> callback,
    const std::string& remote_device,
    int32_t handle,
    bool confirm,
    std::vector<uint8_t> value) {
  CallGattMethod<Void>(std::move(callback), gatt::kServerSendNotification,
                       server_id_, remote_device, handle, confirm, value);
}

void FlossGattManagerClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index,
                                  base::Version version,
                                  base::OnceClosure on_ready) {
  // Set field variables.
  bus_ = bus;
  service_name_ = service_name;
  gatt_adapter_path_ = GenerateGattPath(adapter_index);
  version_ = version;

  // Initialize DBus object proxy.
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, gatt_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR)
        << "FlossGattManagerClient couldn't init. Object proxy was null.";
    return;
  }

  // Register all callbacks.
  gatt_client_exported_callback_manager_.Init(bus_.get());
  gatt_server_exported_callback_manager_.Init(bus_.get());

  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnClientRegistered,
      &FlossGattClientObserver::GattClientRegistered);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnClientConnectionState,
      &FlossGattClientObserver::GattClientConnectionState);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnPhyUpdate, &FlossGattClientObserver::GattPhyUpdate);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnPhyRead, &FlossGattClientObserver::GattPhyRead);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnSearchComplete, &FlossGattClientObserver::GattSearchComplete);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicRead,
      &FlossGattClientObserver::GattCharacteristicRead);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnCharacteristicWrite,
      &FlossGattClientObserver::GattCharacteristicWrite);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnExecuteWrite, &FlossGattClientObserver::GattExecuteWrite);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnDescriptorRead, &FlossGattClientObserver::GattDescriptorRead);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnDescriptorWrite, &FlossGattClientObserver::GattDescriptorWrite);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnNotify, &FlossGattClientObserver::GattNotify);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnReadRemoteRssi, &FlossGattClientObserver::GattReadRemoteRssi);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnConfigureMtu, &FlossGattClientObserver::GattConfigureMtu);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnConnectionUpdated,
      &FlossGattClientObserver::GattConnectionUpdated);
  gatt_client_exported_callback_manager_.AddMethod(
      gatt::kOnServiceChanged, &FlossGattClientObserver::GattServiceChanged);

  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerRegistered,
      &FlossGattServerObserver::GattServerRegistered);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerConnectionState,
      &FlossGattServerObserver::GattServerConnectionState);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerServiceAdded,
      &FlossGattServerObserver::GattServerServiceAdded);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerServiceRemoved,
      &FlossGattServerObserver::GattServerServiceRemoved);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerCharacteristicReadRequest,
      &FlossGattServerObserver::GattServerCharacteristicReadRequest);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerDescriptorReadRequest,
      &FlossGattServerObserver::GattServerDescriptorReadRequest);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerCharacteristicWriteRequest,
      &FlossGattServerObserver::GattServerCharacteristicWriteRequest);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerDescriptorWriteRequest,
      &FlossGattServerObserver::GattServerDescriptorWriteRequest);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnExecuteWrite, &FlossGattServerObserver::GattServerExecuteWrite);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerNotificationSent,
      &FlossGattServerObserver::GattServerNotificationSent);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerMtuChanged,
      &FlossGattServerObserver::GattServerMtuChanged);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnPhyUpdate, &FlossGattServerObserver::GattServerPhyUpdate);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnPhyRead, &FlossGattServerObserver::GattServerPhyRead);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnConnectionUpdated,
      &FlossGattServerObserver::GattServerConnectionUpdate);
  gatt_server_exported_callback_manager_.AddMethod(
      gatt::kOnServerSubrateChange,
      &FlossGattServerObserver::GattServerSubrateChange);

  // Export callbacks.
  if (!gatt_client_exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossGattManagerClient::RegisterClient,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Unable to successfully export FlossGattClientObserver.";
    return;
  }
  if (!gatt_server_exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kExportedCallbacksPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossGattManagerClient::RegisterServer,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Unable to successfully export FlossGattServerObserver.";
    return;
  }

  property_msft_supported_.Init(this, bus_, service_name_, gatt_adapter_path_,
                                dbus::ObjectPath(kExportedCallbacksPath),
                                base::DoNothing());

  // Everything is queued for registration so save |on_ready| for later.
  on_ready_ = std::move(on_ready);
}

void FlossGattManagerClient::RegisterClient() {
  // Finish registering client. We will get gatt client id via
  // |GattClientRegistered|.
  CallGattMethod<Void>(
      base::BindOnce(&HandleResponse, gatt::kRegisterClient),
      gatt::kRegisterClient, std::string(kDefaultGattManagerClientUuid),
      dbus::ObjectPath(kExportedCallbacksPath), kDefaultEattSupport);
}

void FlossGattManagerClient::RegisterServer() {
  // Finish registering server. We will get gatt server id via
  // |GattClientRegistered|.
  CallGattMethod<Void>(
      base::BindOnce(&HandleResponse, gatt::kRegisterServer),
      gatt::kRegisterServer, std::string(kDefaultGattManagerClientUuid),
      dbus::ObjectPath(kExportedCallbacksPath), kDefaultEattSupport);
}

void FlossGattManagerClient::CompleteInit() {
  if (client_id_ && server_id_ && on_ready_) {
    std::move(on_ready_).Run();
  }
}

void FlossGattManagerClient::GattClientRegistered(GattStatus status,
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
  CompleteInit();
}

void FlossGattManagerClient::GattClientConnectionState(GattStatus status,
                                                       int32_t client_id,
                                                       bool connected,
                                                       std::string address) {
  if (client_id != client_id_) {
    return;
  }

  for (auto& observer : gatt_client_observers_) {
    observer.GattClientConnectionState(status, client_id, connected, address);
  }
}

void FlossGattManagerClient::GattPhyUpdate(std::string address,
                                           LePhy tx,
                                           LePhy rx,
                                           GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattPhyUpdate(address, tx, rx, status);
  }
}

void FlossGattManagerClient::GattPhyRead(std::string address,
                                         LePhy tx,
                                         LePhy rx,
                                         GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattPhyRead(address, tx, rx, status);
  }
}

void FlossGattManagerClient::GattSearchComplete(
    std::string address,
    const std::vector<GattService>& services,
    GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattSearchComplete(address, services, status);
  }
}

void FlossGattManagerClient::GattCharacteristicRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattCharacteristicRead(address, status, handle, data);
  }
}

void FlossGattManagerClient::GattCharacteristicWrite(std::string address,
                                                     GattStatus status,
                                                     int32_t handle) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattCharacteristicWrite(address, status, handle);
  }
}

void FlossGattManagerClient::GattExecuteWrite(std::string address,
                                              GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattExecuteWrite(address, status);
  }
}

void FlossGattManagerClient::GattDescriptorRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattDescriptorRead(address, status, handle, data);
  }
}

void FlossGattManagerClient::GattDescriptorWrite(std::string address,
                                                 GattStatus status,
                                                 int32_t handle) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattDescriptorWrite(address, status, handle);
  }
}

void FlossGattManagerClient::GattNotify(std::string address,
                                        int32_t handle,
                                        const std::vector<uint8_t>& data) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattNotify(address, handle, data);
  }
}

void FlossGattManagerClient::GattReadRemoteRssi(std::string address,
                                                int32_t rssi,
                                                GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattReadRemoteRssi(address, rssi, status);
  }
}

void FlossGattManagerClient::GattConfigureMtu(std::string address,
                                              int32_t mtu,
                                              GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattConfigureMtu(address, mtu, status);
  }
}

void FlossGattManagerClient::GattConnectionUpdated(std::string address,
                                                   int32_t interval,
                                                   int32_t latency,
                                                   int32_t timeout,
                                                   GattStatus status) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattConnectionUpdated(address, interval, latency, timeout, status);
  }
}

void FlossGattManagerClient::GattServiceChanged(std::string address) {
  for (auto& observer : gatt_client_observers_) {
    observer.GattServiceChanged(address);
  }
}

// TODO(b/193685841) - Floss currently doesn't emit a callback when
// a notification registers. Once a callback is available, we should report that
// via the callback here instead.
void FlossGattManagerClient::OnRegisterNotificationResponse(
    ResponseCallback<GattStatus> callback,
    bool is_registering,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    std::move(callback).Run(GattStatus::kError);
    return;
  }

  std::move(callback).Run(GattStatus::kSuccess);
}

void FlossGattManagerClient::GattServerRegistered(GattStatus status,
                                                  int32_t server_id) {
  if (server_id_ != 0) {
    LOG(ERROR) << "Unexpected GattServerRegistered with id = " << server_id
               << " when we already have id = " << server_id_;
    return;
  }

  if (status != GattStatus::kSuccess) {
    LOG(ERROR) << "RegisterServer failed with status = "
               << static_cast<uint32_t>(status);
    return;
  }

  server_id_ = server_id;
  CompleteInit();
}

void FlossGattManagerClient::GattServerConnectionState(int32_t server_id,
                                                       bool connected,
                                                       std::string address) {
  if (server_id != server_id_) {
    return;
  }

  for (auto& observer : gatt_server_observers_) {
    observer.GattServerConnectionState(server_id, connected, address);
  }
}

void FlossGattManagerClient::GattServerServiceAdded(GattStatus status,
                                                    GattService service) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerServiceAdded(status, service);
  }
}

void FlossGattManagerClient::GattServerServiceRemoved(GattStatus status,
                                                      int32_t handle) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerServiceRemoved(status, handle);
  }
}

void FlossGattManagerClient::GattServerCharacteristicReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerCharacteristicReadRequest(address, request_id, offset,
                                                 is_long, handle);
  }
}

void FlossGattManagerClient::GattServerDescriptorReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerDescriptorReadRequest(address, request_id, offset,
                                             is_long, handle);
  }
}

void FlossGattManagerClient::GattServerCharacteristicWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerCharacteristicWriteRequest(
        address, request_id, offset, length, is_prepared_write, needs_response,
        handle, value);
  }
}

void FlossGattManagerClient::GattServerDescriptorWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerDescriptorWriteRequest(address, request_id, offset,
                                              length, is_prepared_write,
                                              needs_response, handle, value);
  }
}

void FlossGattManagerClient::GattServerExecuteWrite(std::string address,
                                                    int32_t request_id,
                                                    bool execute_write) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerExecuteWrite(address, request_id, execute_write);
  }
}

void FlossGattManagerClient::GattServerNotificationSent(std::string address,
                                                        GattStatus status) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerNotificationSent(address, status);
  }
}

void FlossGattManagerClient::GattServerMtuChanged(std::string address,
                                                  int32_t mtu) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerMtuChanged(address, mtu);
  }
}

void FlossGattManagerClient::GattServerPhyUpdate(std::string address,
                                                 LePhy tx_phy,
                                                 LePhy rx_phy,
                                                 GattStatus status) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerPhyUpdate(address, tx_phy, rx_phy, status);
  }
}

void FlossGattManagerClient::GattServerPhyRead(std::string address,
                                               LePhy tx_phy,
                                               LePhy rx_phy,
                                               GattStatus status) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerPhyRead(address, tx_phy, rx_phy, status);
  }
}

void FlossGattManagerClient::GattServerConnectionUpdate(std::string address,
                                                        int32_t interval,
                                                        int32_t latency,
                                                        int32_t timeout,
                                                        GattStatus status) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerConnectionUpdate(address, interval, latency, timeout,
                                        status);
  }
}

void FlossGattManagerClient::GattServerSubrateChange(std::string address,
                                                     int32_t subrate_factor,
                                                     int32_t latency,
                                                     int32_t continuation_num,
                                                     int32_t timeout,
                                                     GattStatus status) {
  for (auto& observer : gatt_server_observers_) {
    observer.GattServerSubrateChange(address, subrate_factor, latency,
                                     continuation_num, timeout, status);
  }
}

}  // namespace floss
