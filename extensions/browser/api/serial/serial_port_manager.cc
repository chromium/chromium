// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_port_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace extensions {

namespace api {

namespace {

bool ShouldPauseOnReceiveError(serial::ReceiveError error) {
  return error == serial::ReceiveError::kDeviceLost ||
         error == serial::ReceiveError::kSystemError ||
         error == serial::ReceiveError::kDisconnected ||
         error == serial::ReceiveError::kBreak ||
         error == serial::ReceiveError::kFrameError ||
         error == serial::ReceiveError::kOverrun ||
         error == serial::ReceiveError::kBufferOverflow ||
         error == serial::ReceiveError::kParityError;
}

SerialPortManager::Binder& GetBinderOverride() {
  static base::NoDestructor<SerialPortManager::Binder> binder;
  return *binder;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<SerialPortManager>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SerialPortManager>*
SerialPortManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
SerialPortManager* SerialPortManager::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<SerialPortManager>::Get(context);
}

SerialPortManager::SerialPortManager(content::BrowserContext* context)
    : context_(context) {
  ApiResourceManager<SerialConnection>* manager =
      ApiResourceManager<SerialConnection>::Get(context_);
  DCHECK(manager) << "No serial connection manager.";
  connections_ = manager->data_;
}

SerialPortManager::~SerialPortManager() = default;

SerialPortManager::ReceiveParams::ReceiveParams() = default;

SerialPortManager::ReceiveParams::ReceiveParams(const ReceiveParams& other) =
    default;

SerialPortManager::ReceiveParams::~ReceiveParams() = default;

void SerialPortManager::GetDevices(
    device::mojom::SerialPortManager::GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureConnection();
  port_manager_->GetDevices(std::move(callback));
}

void SerialPortManager::OpenPort(
    const std::string& path,
    device::mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<device::mojom::SerialPortClient> client,
    OpenPortCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureConnection();
  port_manager_->GetDevices(base::BindOnce(
      &SerialPortManager::OnGotDevicesToGetPort, weak_factory_.GetWeakPtr(),
      path, std::move(options), std::move(client), std::move(callback)));
}

void SerialPortManager::StartConnectionPolling(const ExtensionId& extension_id,
                                               int connection_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* connection = connections_->Get(extension_id, connection_id);
  if (!connection)
    return;

  DCHECK_EQ(extension_id, connection->owner_extension_id());

  ReceiveParams params;
  params.browser_context_id = context_;
  params.extension_id = extension_id;
  params.connections = connections_;
  params.connection_id = connection_id;

  connection->StartPolling(base::BindRepeating(&DispatchReceiveEvent, params));
}

// static
void SerialPortManager::OverrideBinderForTesting(Binder binder) {
  GetBinderOverride() = std::move(binder);
}

// static
void SerialPortManager::DispatchReceiveEvent(const ReceiveParams& params,
                                             std::vector<uint8_t> data,
                                             serial::ReceiveError error) {
  // Note that an error (e.g. timeout) does not necessarily mean that no data
  // was read, so we may fire an onReceive regardless of any error code.
  if (data.size() > 0) {
    serial::ReceiveInfo receive_info;
    receive_info.connection_id = params.connection_id;
    receive_info.data = std::move(data);
    auto args = serial::OnReceive::Create(receive_info);
    std::unique_ptr<extensions::Event> event(
        new extensions::Event(extensions::events::SERIAL_ON_RECEIVE,
                              serial::OnReceive::kEventName, std::move(args)));
    DispatchEvent(params, std::move(event));
  }

  if (error != serial::ReceiveError::kNone) {
    if (ShouldPauseOnReceiveError(error)) {
      SerialConnection* connection =
          params.connections->Get(params.extension_id, params.connection_id);
      if (connection)
        connection->SetPaused(true);
    }
    serial::ReceiveErrorInfo error_info;
    error_info.connection_id = params.connection_id;
    error_info.error = error;
    auto args = serial::OnReceiveError::Create(error_info);
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::SERIAL_ON_RECEIVE_ERROR,
        serial::OnReceiveError::kEventName, std::move(args)));
    DispatchEvent(params, std::move(event));
  }
}

// static
void SerialPortManager::DispatchEvent(
    const ReceiveParams& params,
    std::unique_ptr<extensions::Event> event) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(
          params.browser_context_id.get())) {
    return;
  }

  content::BrowserContext* context = reinterpret_cast<content::BrowserContext*>(
      params.browser_context_id.get());

  EventRouter* router = EventRouter::Get(context);
  if (router)
    router->DispatchEventToExtension(params.extension_id, std::move(event));
}

void SerialPortManager::EnsureConnection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (port_manager_)
    return;

  auto receiver = port_manager_.BindNewPipeAndPassReceiver();
  port_manager_.set_disconnect_handler(
      base::BindOnce(&SerialPortManager::OnPortManagerConnectionError,
                     weak_factory_.GetWeakPtr()));

  const auto& binder = GetBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    content::GetDeviceService().BindSerialPortManager(std::move(receiver));
}

void SerialPortManager::OnGotDevicesToGetPort(
    const std::string& path,
    device::mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<device::mojom::SerialPortClient> client,
    OpenPortCallback callback,
    std::vector<device::mojom::SerialPortInfoPtr> devices) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& device : devices) {
    if (device->path.AsUTF8Unsafe() == path) {
      port_manager_->OpenPort(device->token, /*use_alternate_path=*/false,
                              std::move(options), std::move(client),
                              /*watcher=*/mojo::NullRemote(),
                              std::move(callback));
      return;
    }

#if BUILDFLAG(IS_MAC)
    if (device->alternate_path &&
        device->alternate_path->AsUTF8Unsafe() == path) {
      port_manager_->OpenPort(device->token, /*use_alternate_path=*/true,
                              std::move(options), std::move(client),
                              /*watcher=*/mojo::NullRemote(),
                              std::move(callback));
      return;
    }
#endif  // BUILDFLAG(IS_MAC)
  }
}

void SerialPortManager::OnPortManagerConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  port_manager_.reset();
}

}  // namespace api

}  // namespace extensions
