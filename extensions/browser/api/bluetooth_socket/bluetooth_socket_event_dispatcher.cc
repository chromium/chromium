// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace {

namespace bluetooth_socket = extensions::api::bluetooth_socket;
using extensions::BluetoothApiSocket;

int kDefaultBufferSize = 4096;

bluetooth_socket::ReceiveError MapReceiveErrorReason(
    BluetoothApiSocket::ErrorReason value) {
  switch (value) {
    case BluetoothApiSocket::kDisconnected:
      return bluetooth_socket::RECEIVE_ERROR_DISCONNECTED;
    case BluetoothApiSocket::kNotConnected:
    // kNotConnected is impossible since a socket has to be connected to be
    // able to call Receive() on it.
    // fallthrough
    case BluetoothApiSocket::kIOPending:
    // kIOPending is not relevant to apps, as BluetoothSocketEventDispatcher
    // handles this specific error.
    // fallthrough
    default:
      return bluetooth_socket::RECEIVE_ERROR_SYSTEM_ERROR;
  }
}

bluetooth_socket::AcceptError MapAcceptErrorReason(
    BluetoothApiSocket::ErrorReason value) {
  // TODO(keybuk): All values are system error, we may want to seperate these
  // out to more discrete reasons.
  switch (value) {
    case BluetoothApiSocket::kNotListening:
    // kNotListening is impossible since a socket has to be listening to be
    // able to call Accept() on it.
    // fallthrough
    default:
      return bluetooth_socket::ACCEPT_ERROR_SYSTEM_ERROR;
  }
}

}  // namespace

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    BluetoothSocketEventDispatcher>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>*
BluetoothSocketEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothSocketEventDispatcher* BluetoothSocketEventDispatcher::Get(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>::Get(
      context);
}

BluetoothSocketEventDispatcher::BluetoothSocketEventDispatcher(
    content::BrowserContext* context)
    : thread_id_(BluetoothApiSocket::kThreadId),
      browser_context_(context) {
  ApiResourceManager<BluetoothApiSocket>* manager =
      ApiResourceManager<BluetoothApiSocket>::Get(browser_context_);
  DCHECK(manager)
      << "There is no socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothApiSocket>.";
  sockets_ = manager->data_;
}

BluetoothSocketEventDispatcher::~BluetoothSocketEventDispatcher() {}

BluetoothSocketEventDispatcher::SocketParams::SocketParams() {}

BluetoothSocketEventDispatcher::SocketParams::SocketParams(
    const SocketParams& other) = default;

BluetoothSocketEventDispatcher::SocketParams::~SocketParams() {}

void BluetoothSocketEventDispatcher::OnSocketConnect(
    const std::string& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  SocketParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartReceive(params);
}

void BluetoothSocketEventDispatcher::OnSocketListen(
    const std::string& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  SocketParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartAccept(params);
}

void BluetoothSocketEventDispatcher::OnSocketResume(
    const std::string& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  SocketParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }

  if (socket->IsConnected()) {
    StartReceive(params);
  } else {
    StartAccept(params);
  }
}

// static
void BluetoothSocketEventDispatcher::StartReceive(const SocketParams& params) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
      << "Socket has wrong owner.";

  // Don't start another read if the socket has been paused.
  if (socket->paused())
    return;

  int buffer_size = socket->buffer_size();
  if (buffer_size <= 0)
    buffer_size = kDefaultBufferSize;
  socket->Receive(
      buffer_size,
      base::Bind(
          &BluetoothSocketEventDispatcher::ReceiveCallback, params),
      base::Bind(
          &BluetoothSocketEventDispatcher::ReceiveErrorCallback, params));
}

// static
void BluetoothSocketEventDispatcher::ReceiveCallback(
    const SocketParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  // Dispatch "onReceive" event.
  bluetooth_socket::ReceiveInfo receive_info;
  receive_info.socket_id = params.socket_id;
  receive_info.data.assign(io_buffer->data(), io_buffer->data() + bytes_read);
  std::unique_ptr<base::ListValue> args =
      bluetooth_socket::OnReceive::Create(receive_info);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_SOCKET_ON_RECEIVE,
                bluetooth_socket::OnReceive::kEventName, std::move(args)));
  PostEvent(params, std::move(event));

  // Post a task to delay the read until the socket is available, as
  // calling StartReceive at this point would error with ERR_IO_PENDING.
  base::PostTask(
      FROM_HERE, {params.thread_id},
      base::BindOnce(&BluetoothSocketEventDispatcher::StartReceive, params));
}

// static
void BluetoothSocketEventDispatcher::ReceiveErrorCallback(
    const SocketParams& params,
    BluetoothApiSocket::ErrorReason error_reason,
    const std::string& error) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  if (error_reason == BluetoothApiSocket::kIOPending) {
    // This happens when resuming a socket which already had an active "read"
    // callback. We can safely ignore this error, as the application should not
    // care.
    return;
  }

  // Dispatch "onReceiveError" event but don't start another read to avoid
  // potential infinite reads if we have a persistent network error.
  bluetooth_socket::ReceiveErrorInfo receive_error_info;
  receive_error_info.socket_id = params.socket_id;
  receive_error_info.error_message = error;
  receive_error_info.error = MapReceiveErrorReason(error_reason);
  std::unique_ptr<base::ListValue> args =
      bluetooth_socket::OnReceiveError::Create(receive_error_info);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_SOCKET_ON_RECEIVE_ERROR,
                bluetooth_socket::OnReceiveError::kEventName, std::move(args)));
  PostEvent(params, std::move(event));

  // Since we got an error, the socket is now "paused" until the application
  // "resumes" it.
  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (socket) {
    socket->set_paused(true);
  }
}

// static
void BluetoothSocketEventDispatcher::StartAccept(const SocketParams& params) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
      << "Socket has wrong owner.";

  // Don't start another accept if the socket has been paused.
  if (socket->paused())
    return;

  socket->Accept(
      base::Bind(
          &BluetoothSocketEventDispatcher::AcceptCallback, params),
      base::Bind(
          &BluetoothSocketEventDispatcher::AcceptErrorCallback, params));
}

// static
void BluetoothSocketEventDispatcher::AcceptCallback(
    const SocketParams& params,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  BluetoothApiSocket* server_api_socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  DCHECK(server_api_socket);

  BluetoothApiSocket* client_api_socket = new BluetoothApiSocket(
      params.extension_id,
      socket,
      device->GetAddress(),
      server_api_socket->uuid());
  int client_socket_id = params.sockets->Add(client_api_socket);

  // Dispatch "onAccept" event.
  bluetooth_socket::AcceptInfo accept_info;
  accept_info.socket_id = params.socket_id;
  accept_info.client_socket_id = client_socket_id;
  std::unique_ptr<base::ListValue> args =
      bluetooth_socket::OnAccept::Create(accept_info);
  std::unique_ptr<Event> event(new Event(events::BLUETOOTH_SOCKET_ON_ACCEPT,
                                         bluetooth_socket::OnAccept::kEventName,
                                         std::move(args)));
  PostEvent(params, std::move(event));

  // Post a task to delay the accept until the socket is available, as
  // calling StartAccept at this point would error with ERR_IO_PENDING.
  base::PostTask(
      FROM_HERE, {params.thread_id},
      base::BindOnce(&BluetoothSocketEventDispatcher::StartAccept, params));
}

// static
void BluetoothSocketEventDispatcher::AcceptErrorCallback(
    const SocketParams& params,
    BluetoothApiSocket::ErrorReason error_reason,
    const std::string& error) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  if (error_reason == BluetoothApiSocket::kIOPending) {
    // This happens when resuming a socket which already had an active "accept"
    // callback. We can safely ignore this error, as the application should not
    // care.
    return;
  }

  // Dispatch "onAcceptError" event but don't start another accept to avoid
  // potential infinite accepts if we have a persistent network error.
  bluetooth_socket::AcceptErrorInfo accept_error_info;
  accept_error_info.socket_id = params.socket_id;
  accept_error_info.error_message = error;
  accept_error_info.error = MapAcceptErrorReason(error_reason);
  std::unique_ptr<base::ListValue> args =
      bluetooth_socket::OnAcceptError::Create(accept_error_info);
  std::unique_ptr<Event> event(
      new Event(events::BLUETOOTH_SOCKET_ON_ACCEPT_ERROR,
                bluetooth_socket::OnAcceptError::kEventName, std::move(args)));
  PostEvent(params, std::move(event));

  // Since we got an error, the socket is now "paused" until the application
  // "resumes" it.
  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (socket) {
    socket->set_paused(true);
  }
}

// static
void BluetoothSocketEventDispatcher::PostEvent(const SocketParams& params,
                                               std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&DispatchEvent, params.browser_context_id,
                                params.extension_id, std::move(event)));
}

// static
void BluetoothSocketEventDispatcher::DispatchEvent(
    void* browser_context_id,
    const std::string& extension_id,
    std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  EventRouter* router = EventRouter::Get(context);
  if (router)
    router->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace api
}  // namespace extensions
