// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"

#include <stdint.h>

#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"
#include "extensions/common/api/bluetooth/bluetooth_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;
using extensions::BluetoothApiSocket;
using extensions::api::bluetooth_socket::ListenOptions;
using extensions::api::bluetooth_socket::SocketInfo;
using extensions::api::bluetooth_socket::SocketProperties;

namespace extensions {
namespace api {

namespace {

const char kDeviceNotFoundError[] = "Device not found";
const char kInvalidPsmError[] = "Invalid PSM";
const char kInvalidUuidError[] = "Invalid UUID";
const char kPermissionDeniedError[] = "Permission denied";
const char kSocketNotFoundError[] = "Socket not found";

SocketInfo CreateSocketInfo(int socket_id, BluetoothApiSocket* socket) {
  DCHECK_CURRENTLY_ON(BluetoothApiSocket::kThreadId);
  SocketInfo socket_info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info.socket_id = socket_id;
  if (socket->name()) {
    socket_info.name = *socket->name();
  }
  socket_info.persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info.buffer_size = socket->buffer_size();
  }
  socket_info.paused = socket->paused();
  socket_info.connected = socket->IsConnected();

  if (socket->IsConnected()) {
    socket_info.address = socket->device_address();
  }
  socket_info.uuid = socket->uuid().canonical_value();

  return socket_info;
}

void SetSocketProperties(BluetoothApiSocket* socket,
                         SocketProperties* properties) {
  if (properties->name) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent) {
    socket->set_persistent(*properties->persistent);
  }
  if (properties->buffer_size) {
    // buffer size is validated when issuing the actual Recv operation
    // on the socket.
    socket->set_buffer_size(*properties->buffer_size);
  }
}

BluetoothSocketEventDispatcher* GetSocketEventDispatcher(
    content::BrowserContext* browser_context) {
  BluetoothSocketEventDispatcher* socket_event_dispatcher =
      BluetoothSocketEventDispatcher::Get(browser_context);
  DCHECK(socket_event_dispatcher)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "BluetoothSocketEventDispatcher.";
  return socket_event_dispatcher;
}

// Returns |true| if |psm| is a valid PSM.
// Per the Bluetooth specification, the PSM field must be at least two octets in
// length, with least significant bit of the least significant octet equal to
// '1' and the least significant bit of the most significant octet equal to '0'.
bool IsValidPsm(int psm) {
  if (psm <= 0)
    return false;

  std::vector<int16_t> octets;
  while (psm > 0) {
     octets.push_back(psm & 0xFF);
     psm = psm >> 8;
  }

  if (octets.size() < 2U)
    return false;

  // The least significant bit of the least significant octet must be '1'.
  if ((octets.front() & 0x01) != 1)
    return false;

  // The least significant bit of the most significant octet must be '0'.
  if ((octets.back() & 0x01) != 0)
    return false;

  return true;
}

}  // namespace

BluetoothSocketAsyncApiFunction::BluetoothSocketAsyncApiFunction() = default;

BluetoothSocketAsyncApiFunction::~BluetoothSocketAsyncApiFunction() = default;

bool BluetoothSocketAsyncApiFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  if (!BluetoothManifestData::CheckSocketPermitted(extension())) {
    *error = kPermissionDeniedError;
    return false;
  }

  manager_ = ApiResourceManager<BluetoothApiSocket>::Get(browser_context());
  DCHECK(manager_)
      << "There is no socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothApiSocket>.";

  if (!manager_) {
    *error = "There is no socket manager.";
    return false;
  }
  return true;
}

int BluetoothSocketAsyncApiFunction::AddSocket(BluetoothApiSocket* socket) {
  return manager_->Add(socket);
}

content::BrowserThread::ID
BluetoothSocketAsyncApiFunction::work_thread_id() const {
  return BluetoothApiSocket::kThreadId;
}

BluetoothApiSocket* BluetoothSocketAsyncApiFunction::GetSocket(
    int api_resource_id) {
  return manager_->Get(extension_id(), api_resource_id);
}

void BluetoothSocketAsyncApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_id(), api_resource_id);
}

std::unordered_set<int>* BluetoothSocketAsyncApiFunction::GetSocketIds() {
  return manager_->GetResourceIds(extension_id());
}

BluetoothSocketCreateFunction::BluetoothSocketCreateFunction() = default;

BluetoothSocketCreateFunction::~BluetoothSocketCreateFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketCreateFunction::Run() {
  DCHECK_CURRENTLY_ON(work_thread_id());

  auto params = bluetooth_socket::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  BluetoothApiSocket* socket = new BluetoothApiSocket(extension_id());

  bluetooth_socket::SocketProperties* properties =
      base::OptionalToPtr(params->properties);
  if (properties)
    SetSocketProperties(socket, properties);

  bluetooth_socket::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  return RespondNow(
      ArgumentList(bluetooth_socket::Create::Results::Create(create_info)));
}

BluetoothSocketUpdateFunction::BluetoothSocketUpdateFunction() = default;

BluetoothSocketUpdateFunction::~BluetoothSocketUpdateFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketUpdateFunction::Run() {
  auto params = bluetooth_socket::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  BluetoothApiSocket* socket = GetSocket(params->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  SetSocketProperties(socket, &params->properties);
  return RespondNow(ArgumentList(bluetooth_socket::Update::Results::Create()));
}

BluetoothSocketSetPausedFunction::BluetoothSocketSetPausedFunction() = default;

BluetoothSocketSetPausedFunction::~BluetoothSocketSetPausedFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketSetPausedFunction::Run() {
  auto params = bluetooth_socket::SetPaused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  BluetoothSocketEventDispatcher* socket_event_dispatcher =
      GetSocketEventDispatcher(browser_context());
  if (!socket_event_dispatcher)
    return RespondNow(Error("Could not get socket event dispatcher."));

  BluetoothApiSocket* socket = GetSocket(params->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  if (socket->paused() != params->paused) {
    socket->set_paused(params->paused);
    if (!params->paused) {
      socket_event_dispatcher->OnSocketResume(extension_id(),
                                              params->socket_id);
    }
  }

  return RespondNow(
      ArgumentList(bluetooth_socket::SetPaused::Results::Create()));
}

BluetoothSocketListenFunction::BluetoothSocketListenFunction() = default;

BluetoothSocketListenFunction::~BluetoothSocketListenFunction() = default;

bool BluetoothSocketListenFunction::PreRunValidation(std::string* error) {
  if (!BluetoothSocketAsyncApiFunction::PreRunValidation(error))
    return false;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  EXTENSION_FUNCTION_PRERUN_VALIDATE(CreateParams());
  socket_event_dispatcher_ = GetSocketEventDispatcher(browser_context());
  return socket_event_dispatcher_ != nullptr;
}

ExtensionFunction::ResponseAction BluetoothSocketListenFunction::Run() {
  DCHECK_CURRENTLY_ON(work_thread_id());
  device::BluetoothAdapterFactory::Get()->GetClassicAdapter(
      base::BindOnce(&BluetoothSocketListenFunction::OnGetAdapter, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void BluetoothSocketListenFunction::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  BluetoothApiSocket* socket = GetSocket(socket_id());
  if (!socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }

  device::BluetoothUUID bluetooth_uuid(uuid());
  if (!bluetooth_uuid.IsValid()) {
    Respond(Error(kInvalidUuidError));
    return;
  }

  BluetoothPermissionRequest param(uuid());
  if (!BluetoothManifestData::CheckRequest(extension(), param)) {
    Respond(Error(kPermissionDeniedError));
    return;
  }

  std::optional<std::string> name;
  if (socket->name())
    name = *socket->name();

  CreateService(
      adapter, bluetooth_uuid, name,
      base::BindOnce(&BluetoothSocketListenFunction::OnCreateService, this),
      base::BindOnce(&BluetoothSocketListenFunction::OnCreateServiceError,
                     this));
}

void BluetoothSocketListenFunction::OnCreateService(
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK_CURRENTLY_ON(work_thread_id());

  // Fetch the socket again since this is not a reference-counted object, and
  // it may have gone away in the meantime (we check earlier to avoid making
  // a connection in the case of an obvious programming error).
  BluetoothApiSocket* api_socket = GetSocket(socket_id());
  if (!api_socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }

  api_socket->AdoptListeningSocket(socket,
                                   device::BluetoothUUID(uuid()));
  socket_event_dispatcher_->OnSocketListen(extension_id(), socket_id());
  Respond(ArgumentList(CreateResults()));
}

void BluetoothSocketListenFunction::OnCreateServiceError(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  Respond(Error(message));
}

BluetoothSocketListenUsingRfcommFunction::
    BluetoothSocketListenUsingRfcommFunction() {}

BluetoothSocketListenUsingRfcommFunction::
    ~BluetoothSocketListenUsingRfcommFunction() {}

int BluetoothSocketListenUsingRfcommFunction::socket_id() const {
  return params_->socket_id;
}

const std::string& BluetoothSocketListenUsingRfcommFunction::uuid() const {
  return params_->uuid;
}

bool BluetoothSocketListenUsingRfcommFunction::CreateParams() {
  params_ = bluetooth_socket::ListenUsingRfcomm::Params::Create(args());
  return params_.has_value();
}

void BluetoothSocketListenUsingRfcommFunction::CreateService(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID& uuid,
    const std::optional<std::string>& name,
    device::BluetoothAdapter::CreateServiceCallback callback,
    device::BluetoothAdapter::CreateServiceErrorCallback error_callback) {
  device::BluetoothAdapter::ServiceOptions service_options;
  service_options.name = std::move(name);

  const std::optional<ListenOptions>& options = params_->options;
  if (options && options->channel)
    service_options.channel = *options->channel;

  adapter->CreateRfcommService(uuid, service_options, std::move(callback),
                               std::move(error_callback));
}

base::Value::List BluetoothSocketListenUsingRfcommFunction::CreateResults() {
  return bluetooth_socket::ListenUsingRfcomm::Results::Create();
}

BluetoothSocketListenUsingL2capFunction::
    BluetoothSocketListenUsingL2capFunction() {}

BluetoothSocketListenUsingL2capFunction::
    ~BluetoothSocketListenUsingL2capFunction() {}

int BluetoothSocketListenUsingL2capFunction::socket_id() const {
  return params_->socket_id;
}

const std::string& BluetoothSocketListenUsingL2capFunction::uuid() const {
  return params_->uuid;
}

bool BluetoothSocketListenUsingL2capFunction::CreateParams() {
  params_ = bluetooth_socket::ListenUsingL2cap::Params::Create(args());
  return params_.has_value();
}

void BluetoothSocketListenUsingL2capFunction::CreateService(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const device::BluetoothUUID& uuid,
    const std::optional<std::string>& name,
    device::BluetoothAdapter::CreateServiceCallback callback,
    device::BluetoothAdapter::CreateServiceErrorCallback error_callback) {
  device::BluetoothAdapter::ServiceOptions service_options;
  service_options.name = std::move(name);

  const std::optional<ListenOptions>& options = params_->options;
  if (options && options->psm) {
    int psm = *options->psm;
    if (!IsValidPsm(psm)) {
      std::move(error_callback).Run(kInvalidPsmError);
      return;
    }

    service_options.psm = psm;
  }

  adapter->CreateL2capService(uuid, service_options, std::move(callback),
                              std::move(error_callback));
}

base::Value::List BluetoothSocketListenUsingL2capFunction::CreateResults() {
  return bluetooth_socket::ListenUsingL2cap::Results::Create();
}

BluetoothSocketAbstractConnectFunction::
    BluetoothSocketAbstractConnectFunction() {}

BluetoothSocketAbstractConnectFunction::
    ~BluetoothSocketAbstractConnectFunction() {}

bool BluetoothSocketAbstractConnectFunction::PreRunValidation(
    std::string* error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BluetoothSocketAsyncApiFunction::PreRunValidation(error))
    return false;

  params_ = bluetooth_socket::Connect::Params::Create(args());
  EXTENSION_FUNCTION_PRERUN_VALIDATE(params_);

  socket_event_dispatcher_ = GetSocketEventDispatcher(browser_context());
  return socket_event_dispatcher_ != nullptr;
}

ExtensionFunction::ResponseAction
BluetoothSocketAbstractConnectFunction::Run() {
  DCHECK_CURRENTLY_ON(work_thread_id());
  device::BluetoothAdapterFactory::Get()->GetClassicAdapter(base::BindOnce(
      &BluetoothSocketAbstractConnectFunction::OnGetAdapter, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void BluetoothSocketAbstractConnectFunction::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }

  device::BluetoothDevice* device = adapter->GetDevice(params_->address);
  if (!device) {
    Respond(Error(kDeviceNotFoundError));
    return;
  }

  device::BluetoothUUID uuid(params_->uuid);
  if (!uuid.IsValid()) {
    Respond(Error(kInvalidUuidError));
    return;
  }

  BluetoothPermissionRequest param(params_->uuid);
  if (!BluetoothManifestData::CheckRequest(extension(), param)) {
    Respond(Error(kPermissionDeniedError));
    return;
  }

  ConnectToService(device, uuid);
}

void BluetoothSocketAbstractConnectFunction::OnConnect(
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK_CURRENTLY_ON(work_thread_id());

  // Fetch the socket again since this is not a reference-counted object, and
  // it may have gone away in the meantime (we check earlier to avoid making
  // a connection in the case of an obvious programming error).
  BluetoothApiSocket* api_socket = GetSocket(params_->socket_id);
  if (!api_socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }

  api_socket->AdoptConnectedSocket(socket,
                                   params_->address,
                                   device::BluetoothUUID(params_->uuid));
  socket_event_dispatcher_->OnSocketConnect(extension_id(),
                                            params_->socket_id);

  Respond(ArgumentList(bluetooth_socket::Connect::Results::Create()));
}

void BluetoothSocketAbstractConnectFunction::OnConnectError(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  Respond(Error(message));
}

BluetoothSocketConnectFunction::BluetoothSocketConnectFunction() = default;

BluetoothSocketConnectFunction::~BluetoothSocketConnectFunction() = default;

void BluetoothSocketConnectFunction::ConnectToService(
    device::BluetoothDevice* device,
    const device::BluetoothUUID& uuid) {
  device->ConnectToService(
      uuid, base::BindOnce(&BluetoothSocketConnectFunction::OnConnect, this),
      base::BindOnce(&BluetoothSocketConnectFunction::OnConnectError, this));
}

BluetoothSocketDisconnectFunction::BluetoothSocketDisconnectFunction() =
    default;

BluetoothSocketDisconnectFunction::~BluetoothSocketDisconnectFunction() =
    default;

ExtensionFunction::ResponseAction BluetoothSocketDisconnectFunction::Run() {
  DCHECK_CURRENTLY_ON(work_thread_id());

  auto params = bluetooth_socket::Disconnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  BluetoothApiSocket* socket = GetSocket(params->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  socket->Disconnect(
      base::BindOnce(&BluetoothSocketDisconnectFunction::OnSuccess, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void BluetoothSocketDisconnectFunction::OnSuccess() {
  DCHECK_CURRENTLY_ON(work_thread_id());
  Respond(ArgumentList(bluetooth_socket::Disconnect::Results::Create()));
}

BluetoothSocketCloseFunction::BluetoothSocketCloseFunction() = default;

BluetoothSocketCloseFunction::~BluetoothSocketCloseFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketCloseFunction::Run() {
  auto params = bluetooth_socket::Close::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  BluetoothApiSocket* socket = GetSocket(params->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  RemoveSocket(params->socket_id);
  return RespondNow(ArgumentList(bluetooth_socket::Close::Results::Create()));
}

BluetoothSocketSendFunction::BluetoothSocketSendFunction()
    : io_buffer_size_(0) {}

BluetoothSocketSendFunction::~BluetoothSocketSendFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketSendFunction::Run() {
  DCHECK_CURRENTLY_ON(work_thread_id());

  params_ = bluetooth_socket::Send::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  io_buffer_size_ = params_->data.size();
  io_buffer_ = base::MakeRefCounted<net::WrappedIOBuffer>(params_->data);

  BluetoothApiSocket* socket = GetSocket(params_->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  socket->Send(io_buffer_, io_buffer_size_,
               base::BindOnce(&BluetoothSocketSendFunction::OnSuccess, this),
               base::BindOnce(&BluetoothSocketSendFunction::OnError, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void BluetoothSocketSendFunction::OnSuccess(int bytes_sent) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  Respond(ArgumentList(bluetooth_socket::Send::Results::Create(bytes_sent)));
}

void BluetoothSocketSendFunction::OnError(
    BluetoothApiSocket::ErrorReason reason,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(work_thread_id());
  Respond(Error(message));
}

BluetoothSocketGetInfoFunction::BluetoothSocketGetInfoFunction() = default;

BluetoothSocketGetInfoFunction::~BluetoothSocketGetInfoFunction() = default;

ExtensionFunction::ResponseAction BluetoothSocketGetInfoFunction::Run() {
  auto params = bluetooth_socket::GetInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  BluetoothApiSocket* socket = GetSocket(params->socket_id);
  if (!socket)
    return RespondNow(Error(kSocketNotFoundError));

  return RespondNow(ArgumentList(bluetooth_socket::GetInfo::Results::Create(
      CreateSocketInfo(params->socket_id, socket))));
}

BluetoothSocketGetSocketsFunction::BluetoothSocketGetSocketsFunction() =
    default;

BluetoothSocketGetSocketsFunction::~BluetoothSocketGetSocketsFunction() =
    default;

ExtensionFunction::ResponseAction BluetoothSocketGetSocketsFunction::Run() {
  std::vector<bluetooth_socket::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids) {
    for (int socket_id : *resource_ids) {
      BluetoothApiSocket* socket = GetSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  return RespondNow(ArgumentList(
      bluetooth_socket::GetSockets::Results::Create(socket_infos)));
}

}  // namespace api
}  // namespace extensions
