// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_socket_manager.h"

#include "base/containers/contains.h"
#include "base/types/expected.h"

namespace floss {
namespace {
void HandleExported(const std::string& method_name,
                    const std::string& interface_name,
                    const std::string& object_path,
                    bool success) {
  DVLOG(1) << (success ? "Successfully exported " : "Failed to export ")
           << method_name << " on interface = " << interface_name
           << ", object = " << object_path;
}

constexpr char kListeningPropId[] = "id";
constexpr char kListeningPropSockType[] = "sock_type";
constexpr char kListeningPropFlags[] = "flags";
constexpr char kListeningPropPsm[] = "psm";
constexpr char kListeningPropChannel[] = "channel";
constexpr char kListeningPropName[] = "name";
constexpr char kListeningPropUuid[] = "uuid";

constexpr char kConnectingPropId[] = "id";
constexpr char kConnectingPropRemoteDevice[] = "remote_device";
constexpr char kConnectingPropSockType[] = "sock_type";
constexpr char kConnectingPropFlags[] = "flags";
constexpr char kConnectingPropFd[] = "fd";
constexpr char kConnectingPropPort[] = "port";
constexpr char kConnectingPropUuid[] = "uuid";
constexpr char kConnectingPropMaxRxSize[] = "max_rx_size";
constexpr char kConnectingPropMaxTxSize[] = "max_tx_size";

constexpr char kResultPropStatus[] = "status";
constexpr char kResultPropId[] = "id";
}  // namespace

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossSocketManager::SocketType* type) {
  uint32_t raw_type = 0;
  bool read = FlossDBusClient::ReadDBusParam(reader, &raw_type);

  if (read) {
    *type = static_cast<FlossSocketManager::SocketType>(raw_type);
  }

  return read;
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossSocketManager::SocketType& type) {
  WriteDBusParam(writer, static_cast<uint32_t>(type));
}

template <>
bool FlossDBusClient::ReadDBusParam(
    dbus::MessageReader* reader,
    FlossSocketManager::FlossListeningSocket* socket) {
  dbus::MessageReader array(nullptr);
  dbus::MessageReader dict(nullptr);

  std::map<std::string, bool> required_keys = {
      {kListeningPropId, false},      {kListeningPropSockType, false},
      {kListeningPropFlags, false},   {kListeningPropPsm, false},
      {kListeningPropChannel, false}, {kListeningPropName, false},
      {kListeningPropUuid, false},
  };

  if (!reader->PopArray(&array)) {
    return false;
  }

  while (array.PopDictEntry(&dict)) {
    std::string key;
    dict.PopString(&key);

    if (base::Contains(required_keys, key)) {
      if (key == kListeningPropId) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->id);
      } else if (key == kListeningPropSockType) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->type);
      } else if (key == kListeningPropFlags) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->flags);
      } else if (key == kListeningPropPsm) {
        required_keys[key] =
            ReadDBusParamFromVariant<std::optional<int>>(&dict, &socket->psm);
      } else if (key == kListeningPropChannel) {
        required_keys[key] = ReadDBusParamFromVariant<std::optional<int>>(
            &dict, &socket->channel);
      } else if (key == kListeningPropName) {
        required_keys[key] =
            ReadDBusParamFromVariant<std::optional<std::string>>(&dict,
                                                                 &socket->name);
      } else if (key == kListeningPropUuid) {
        required_keys[key] =
            ReadDBusParamFromVariant<std::optional<device::BluetoothUUID>>(
                &dict, &socket->uuid);
      }
    }
  }

  // Make sure all required keys were correctly parsed.
  bool result = true;
  for (auto [key, found] : required_keys) {
    result = result && found;
  }

  return result;
}

template bool
FlossDBusClient::ReadDBusParam<FlossSocketManager::FlossListeningSocket>(
    dbus::MessageReader* reader,
    std::optional<FlossSocketManager::FlossListeningSocket>* socket);

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossSocketManager::FlossListeningSocket& socket) {
  dbus::MessageWriter array(nullptr);
  dbus::MessageWriter dict(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kListeningPropId, socket.id);
  WriteDictEntry(&array, kListeningPropSockType, socket.type);
  WriteDictEntry(&array, kListeningPropFlags, socket.flags);
  WriteDictEntry(&array, kListeningPropPsm, socket.psm);
  WriteDictEntry(&array, kListeningPropChannel, socket.channel);
  WriteDictEntry(&array, kListeningPropName, socket.name);
  WriteDictEntry(&array, kListeningPropUuid, socket.uuid);

  writer->CloseContainer(&array);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    FlossSocketManager::FlossSocket* socket) {
  dbus::MessageReader array(nullptr);
  dbus::MessageReader dict(nullptr);
  std::map<std::string, bool> required_keys = {
      {kConnectingPropId, false},        {kConnectingPropRemoteDevice, false},
      {kConnectingPropSockType, false},  {kConnectingPropFlags, false},
      {kConnectingPropFd, false},        {kConnectingPropPort, false},
      {kConnectingPropUuid, false},      {kConnectingPropMaxRxSize, false},
      {kConnectingPropMaxTxSize, false},
  };
  if (!reader->PopArray(&array)) {
    return false;
  }

  while (array.PopDictEntry(&dict)) {
    std::string key;
    dict.PopString(&key);

    if (base::Contains(required_keys, key)) {
      if (key == kConnectingPropId) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->id);
      } else if (key == kConnectingPropRemoteDevice) {
        required_keys[key] =
            ReadDBusParamFromVariant(&dict, &socket->remote_device);
      } else if (key == kConnectingPropSockType) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->type);
      } else if (key == kConnectingPropFlags) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->flags);
      } else if (key == kConnectingPropFd) {
        required_keys[key] =
            ReadDBusParamFromVariant<std::optional<base::ScopedFD>>(
                &dict, &socket->fd);
      } else if (key == kConnectingPropPort) {
        required_keys[key] = ReadDBusParamFromVariant(&dict, &socket->port);
      } else if (key == kConnectingPropUuid) {
        required_keys[key] =
            ReadDBusParamFromVariant<std::optional<device::BluetoothUUID>>(
                &dict, &socket->uuid);
      } else if (key == kConnectingPropMaxRxSize) {
        required_keys[key] =
            ReadDBusParamFromVariant(&dict, &socket->max_rx_size);
      } else if (key == kConnectingPropMaxTxSize) {
        required_keys[key] =
            ReadDBusParamFromVariant(&dict, &socket->max_tx_size);
      }
    }
  }

  // Make sure all required keys were correctly parsed.
  bool result = true;
  for (auto [key, found] : required_keys) {
    result = result && found;
  }

  return result;
}

template bool FlossDBusClient::ReadDBusParam<FlossSocketManager::FlossSocket>(
    dbus::MessageReader* reader,
    std::optional<FlossSocketManager::FlossSocket>* socket);

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossSocketManager::FlossSocket& socket) {
  dbus::MessageWriter array(nullptr);
  dbus::MessageWriter dict(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kConnectingPropId, socket.id);
  WriteDictEntry(&array, kConnectingPropRemoteDevice, socket.remote_device);
  WriteDictEntry(&array, kConnectingPropSockType, socket.type);
  WriteDictEntry(&array, kConnectingPropFlags, socket.flags);
  WriteDictEntry(&array, kConnectingPropFd, socket.fd);
  WriteDictEntry(&array, kConnectingPropPort, socket.port);
  WriteDictEntry(&array, kConnectingPropUuid, socket.uuid);
  WriteDictEntry(&array, kConnectingPropMaxRxSize, socket.max_rx_size);
  WriteDictEntry(&array, kConnectingPropMaxTxSize, socket.max_tx_size);

  writer->CloseContainer(&array);
}

template <>
bool FlossDBusClient::ReadDBusParam(
    dbus::MessageReader* reader,
    FlossSocketManager::SocketResult* socket_result) {
  dbus::MessageReader array(nullptr);
  dbus::MessageReader dict(nullptr);

  std::map<std::string, bool> required_keys = {
      {kResultPropStatus, false},
      {kResultPropId, false},
  };

  if (!reader->PopArray(&array)) {
    return false;
  }

  while (array.PopDictEntry(&dict)) {
    std::string key;
    dict.PopString(&key);

    if (base::Contains(required_keys, key)) {
      if (key == kResultPropStatus) {
        required_keys[key] =
            ReadDBusParamFromVariant(&dict, &socket_result->status);
      } else if (key == kResultPropId) {
        required_keys[key] =
            ReadDBusParamFromVariant(&dict, &socket_result->id);
      }
    }
  }

  // Make sure all required keys were correctly parsed.
  bool result = true;
  for (auto [key, found] : required_keys) {
    result = result && found;
  }

  return result;
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const FlossSocketManager::SocketResult& socket_result) {
  dbus::MessageWriter array(nullptr);
  dbus::MessageWriter dict(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kResultPropStatus,
                 static_cast<uint32_t>(socket_result.status));
  WriteDictEntry(&array, kResultPropId, socket_result.id);

  writer->CloseContainer(&array);
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const FlossSocketManager::SocketType*) {
  static DBusTypeInfo info{"u", "SocketType"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const FlossSocketManager::FlossSocket*) {
  static DBusTypeInfo info{"a{sv}", "FlossSocket"};
  return info;
}

int FlossSocketManager::GetRawFlossFlagsFromBluetoothFlags(bool encrypt,
                                                           bool auth,
                                                           bool auth_mitm,
                                                           bool auth_16_digit,
                                                           bool no_sdp) {
  int flags = 0;
  return flags |
         (encrypt ? static_cast<int>(SocketFlags::kSocketFlagsEncrypt) : 0) |
         (auth ? static_cast<int>(SocketFlags::kSocketFlagsAuth) : 0) |
         (auth_mitm ? static_cast<int>(SocketFlags::kSocketFlagsAuthMitm) : 0) |
         (auth_16_digit ? static_cast<int>(SocketFlags::kSocketFlagsAuth16Digit)
                        : 0) |
         (no_sdp ? static_cast<int>(SocketFlags::kSocketFlagsNoSdp) : 0);
}

FlossSocketManager::FlossListeningSocket::FlossListeningSocket() = default;
FlossSocketManager::FlossListeningSocket::FlossListeningSocket(
    const FlossListeningSocket&) = default;
FlossSocketManager::FlossListeningSocket::~FlossListeningSocket() = default;

FlossSocketManager::FlossSocket::FlossSocket() = default;
FlossSocketManager::FlossSocket::FlossSocket(FlossSocket&&) = default;
FlossSocketManager::FlossSocket::~FlossSocket() = default;

// static
const char FlossSocketManager::kErrorInvalidCallback[] =
    "org.chromium.Error.InvalidCallbackId";

// static
const char FlossSocketManager::kExportedCallbacksPath[] =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    "/org/chromium/bluetooth/socket_manager/callback/lacros";
#else
    "/org/chromium/bluetooth/socket_manager/callback";
#endif

// static
std::unique_ptr<FlossSocketManager> FlossSocketManager::Create() {
  return std::make_unique<FlossSocketManager>();
}

FlossSocketManager::FlossSocketManager() = default;

FlossSocketManager::~FlossSocketManager() {
  if (callback_id_ != kInvalidCallbackId) {
    CallSocketMethod(
        base::BindOnce(&FlossSocketManager::CompleteUnregisterCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        socket_manager::kUnregisterCallback, callback_id_);
  }
  if (bus_) {
    bus_->UnregisterExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  }
}

void FlossSocketManager::ListenUsingL2cap(
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(base::unexpected(Error(kErrorInvalidCallback, "")));
    return;
  }

  const char* method = security_level == Security::kInsecure
                           ? socket_manager::kListenUsingInsecureL2capChannel
                           : socket_manager::kListenUsingL2capChannel;

  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteListen,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ready_cb), std::move(new_connection_cb)),
      method, callback_id_);
}

void FlossSocketManager::ListenUsingL2capLe(
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(base::unexpected(Error(kErrorInvalidCallback, "")));
    return;
  }

  const char* method = security_level == Security::kInsecure
                           ? socket_manager::kListenUsingInsecureL2capLeChannel
                           : socket_manager::kListenUsingL2capLeChannel;

  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteListen,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ready_cb), std::move(new_connection_cb)),
      method, callback_id_);
}

void FlossSocketManager::ListenUsingRfcommAlt(
    const std::optional<std::string> name,
    const std::optional<device::BluetoothUUID> application_uuid,
    const std::optional<int> channel,
    const std::optional<int> flags,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(
        base::unexpected(Error(kErrorInvalidCallback, /*message=*/"")));
    return;
  }
  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteListen,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ready_cb), std::move(new_connection_cb)),
      socket_manager::kListenUsingRfcomm, callback_id_, channel,
      application_uuid, name, flags);
}

void FlossSocketManager::ListenUsingRfcomm(
    const std::string& name,
    const device::BluetoothUUID& uuid,
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(
        base::unexpected(Error(kErrorInvalidCallback, /*message=*/"")));
    return;
  }

  const char* method =
      security_level == Security::kInsecure
          ? socket_manager::kListenUsingInsecureRfcommWithServiceRecord
          : socket_manager::kListenUsingRfcommWithServiceRecord;
  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteListen,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(ready_cb), std::move(new_connection_cb)),
      method, callback_id_, name, uuid);
}

void FlossSocketManager::ConnectUsingL2cap(const FlossDeviceId& remote_device,
                                           const int psm,
                                           const Security security_level,
                                           ConnectionCompleted callback) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
    return;
  }

  const char* method = security_level == Security::kInsecure
                           ? socket_manager::kCreateInsecureL2capChannel
                           : socket_manager::kCreateL2capChannel;

  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteConnect,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      method, callback_id_, remote_device, psm);
}

void FlossSocketManager::ConnectUsingL2capLe(const FlossDeviceId& remote_device,
                                             const int psm,
                                             const Security security_level,
                                             ConnectionCompleted callback) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
    return;
  }

  const char* method = security_level == Security::kInsecure
                           ? socket_manager::kCreateInsecureL2capLeChannel
                           : socket_manager::kCreateL2capLeChannel;

  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteConnect,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      method, callback_id_, remote_device, psm);
}

void FlossSocketManager::ConnectUsingRfcomm(const FlossDeviceId& remote_device,
                                            const device::BluetoothUUID& uuid,
                                            const Security security_level,
                                            ConnectionCompleted callback) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
    return;
  }

  const char* method =
      security_level == Security::kInsecure
          ? socket_manager::kCreateInsecureRfcommSocketToServiceRecord
          : socket_manager::kCreateRfcommSocketToServiceRecord;

  CallSocketMethod(
      base::BindOnce(&FlossSocketManager::CompleteConnect,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      method, callback_id_, remote_device, uuid);
}

void FlossSocketManager::Accept(const SocketId id,
                                std::optional<uint32_t> timeout_ms,
                                ResponseCallback<BtifStatus> callback) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(base::unexpected(Error(kErrorInvalidCallback, "")));
    return;
  }

  CallSocketMethod(std::move(callback), socket_manager::kAccept, callback_id_,
                   id, timeout_ms);
}

void FlossSocketManager::Close(const SocketId id,
                               ResponseCallback<BtifStatus> callback) {
  if (callback_id_ == kInvalidCallbackId) {
    std::move(callback).Run(base::unexpected(Error(kErrorInvalidCallback, "")));
    return;
  }

  CallSocketMethod(std::move(callback), socket_manager::kClose, callback_id_,
                   id);
}

void FlossSocketManager::Init(dbus::Bus* bus,
                              const std::string& service_name,
                              const int adapter_index,
                              base::Version version,
                              base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;
  adapter_path_ = GenerateAdapterPath(adapter_index);
  version_ = version;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossSocketManager couldn't init. Object proxy was null.";
    return;
  }

  dbus::ExportedObject* callbacks =
      bus_->GetExportedObject(dbus::ObjectPath(kExportedCallbacksPath));
  if (!callbacks) {
    LOG(ERROR) << "FlossSocketManager couldn't export client callbacks.";
    return;
  }

  // Export callbacks for socket manager.
  callbacks->ExportMethod(
      socket_manager::kCallbackInterface,
      socket_manager::kOnIncomingSocketReady,
      base::BindRepeating(&FlossSocketManager::OnIncomingSocketReady,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, socket_manager::kOnIncomingSocketReady));
  callbacks->ExportMethod(
      socket_manager::kCallbackInterface,
      socket_manager::kOnIncomingSocketClosed,
      base::BindRepeating(&FlossSocketManager::OnIncomingSocketClosed,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported, socket_manager::kOnIncomingSocketClosed));
  callbacks->ExportMethod(
      socket_manager::kCallbackInterface,
      socket_manager::kOnHandleIncomingConnection,
      base::BindRepeating(&FlossSocketManager::OnHandleIncomingConnection,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported,
                     socket_manager::kOnHandleIncomingConnection));
  callbacks->ExportMethod(
      socket_manager::kCallbackInterface,
      socket_manager::kOnOutgoingConnectionResult,
      base::BindRepeating(&FlossSocketManager::OnOutgoingConnectionResult,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HandleExported,
                     socket_manager::kOnOutgoingConnectionResult));

  // Register callbacks and store the callback id.
  dbus::MethodCall register_callback(kSocketManagerInterface,
                                     socket_manager::kRegisterCallback);
  dbus::MessageWriter writer(&register_callback);
  writer.AppendObjectPath(dbus::ObjectPath(kExportedCallbacksPath));

  object_proxy->CallMethodWithErrorResponse(
      &register_callback, kDBusTimeoutMs,
      base::BindOnce(&FlossSocketManager::CompleteRegisterCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  on_ready_ = std::move(on_ready);
}

void FlossSocketManager::CompleteRegisterCallback(
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (error_response) {
    FlossDBusClient::LogErrorResponse("SocketManager::RegisterCallback",
                                      error_response);
  } else {
    dbus::MessageReader reader(response);
    uint32_t result;
    if (!reader.PopUint32(&result)) {
      LOG(ERROR)
          << "No callback id provided for SocketManager::RegisterCallback";
      return;
    }

    callback_id_ = result;

    if (on_ready_) {
      std::move(on_ready_).Run();
    }
  }
}

void FlossSocketManager::CompleteUnregisterCallback(DBusResult<bool> result) {
  if (!result.has_value() || *result == false) {
    LOG(WARNING) << __func__ << ": Failed to unregister callback";
  }
}

void FlossSocketManager::CompleteListen(ResponseCallback<BtifStatus> callback,
                                        ConnectionStateChanged ready_cb,
                                        ConnectionAccepted new_connection_cb,
                                        DBusResult<SocketResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  // We got back a valid socket id from listen. Put into listening list.
  if (result->id != kInvalidSocketId) {
    listening_sockets_to_callbacks_.insert({
        result->id,
        {std::move(ready_cb), std::move(new_connection_cb)},
    });
  }

  // Complete callback with the BtifStatus.
  std::move(callback).Run(result->status);
}

void FlossSocketManager::CompleteConnect(ConnectionCompleted callback,
                                         DBusResult<SocketResult> result) {
  if (!result.has_value()) {
    std::move(callback).Run(BtifStatus::kFail, /*socket=*/std::nullopt);
    return;
  }

  // If we got back a valid socket id from connect, put into connecting list.
  // Otherwise, return the failed status back upwards.
  if (result->id != kInvalidSocketId) {
    connecting_sockets_to_callbacks_.insert({
        result->id,
        std::move(callback),
    });
  } else {
    std::move(callback).Run(result->status, /*socket=*/std::nullopt);
  }
}

void FlossSocketManager::OnIncomingSocketReady(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  FlossListeningSocket socket;
  BtifStatus status;

  if (!ReadAllDBusParams(&reader, &socket, &status)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  // If this is a known socket, run the |ConnectionStateChanged| callback.
  auto found = listening_sockets_to_callbacks_.find(socket.id);
  if (found != listening_sockets_to_callbacks_.end()) {
    auto& [key, callbacks] = *found;
    auto& [state_changed, accepted] = callbacks;

    state_changed.Run(ServerSocketState::kReady, socket, status);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossSocketManager::OnIncomingSocketClosed(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  SocketId id;
  BtifStatus status;

  if (!ReadAllDBusParams(&reader, &id, &status)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  // If this is a known socket, run the |ConnectionStateChanged| callback and
  // remove this from the list of listening sockets.
  auto found = listening_sockets_to_callbacks_.find(id);
  if (found != listening_sockets_to_callbacks_.end()) {
    FlossListeningSocket socket;
    socket.id = id;

    auto& [key, callbacks] = *found;
    auto& [state_changed, accepted] = callbacks;

    state_changed.Run(ServerSocketState::kClosed, socket, status);
    listening_sockets_to_callbacks_.erase(found);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossSocketManager::OnHandleIncomingConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  SocketId id;
  FlossSocket socket;

  if (!ReadAllDBusParams(&reader, &id, &socket)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  // New connection on a known listening socket. Call the |ConnectionAccepted|
  // callback.
  auto found = listening_sockets_to_callbacks_.find(id);
  if (found != listening_sockets_to_callbacks_.end()) {
    auto& [key, callbacks] = *found;
    auto& [state_changed, accepted] = callbacks;

    accepted.Run(std::move(socket));
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void FlossSocketManager::OnOutgoingConnectionResult(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  SocketId id;
  BtifStatus status;
  std::optional<FlossSocket> socket;

  if (!ReadAllDBusParams(&reader, &id, &status, &socket)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidParameters, std::string()));
    return;
  }

  // Connecting callback finished. Call the |ConnectionCompleted| callback and
  // remove this entry from the pending connections list.
  auto found = connecting_sockets_to_callbacks_.find(id);
  if (found != connecting_sockets_to_callbacks_.end()) {
    auto& [key, complete_callback] = *found;
    std::move(complete_callback).Run(status, std::move(socket));
    connecting_sockets_to_callbacks_.erase(found);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

// Specializations for default responses.
template void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback<FlossSocketManager::SocketResult> callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response);

}  // namespace floss
