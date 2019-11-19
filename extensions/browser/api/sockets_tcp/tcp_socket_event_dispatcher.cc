// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "net/base/net_errors.h"

namespace {
int kDefaultBufferSize = 4096;
}

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<TCPSocketEventDispatcher>>::DestructorAtExit
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TCPSocketEventDispatcher>*
TCPSocketEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
TCPSocketEventDispatcher* TCPSocketEventDispatcher::Get(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return BrowserContextKeyedAPIFactory<TCPSocketEventDispatcher>::Get(context);
}

TCPSocketEventDispatcher::TCPSocketEventDispatcher(
    content::BrowserContext* context)
    : thread_id_(Socket::kThreadId), browser_context_(context) {
  ApiResourceManager<ResumableTCPSocket>* manager =
      ApiResourceManager<ResumableTCPSocket>::Get(browser_context_);
  DCHECK(manager)
      << "There is no socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<ResumableTCPSocket>.";
  sockets_ = manager->data_;
}

TCPSocketEventDispatcher::~TCPSocketEventDispatcher() {}

TCPSocketEventDispatcher::ReadParams::ReadParams() {}

TCPSocketEventDispatcher::ReadParams::ReadParams(const ReadParams& other) =
    default;

TCPSocketEventDispatcher::ReadParams::~ReadParams() {}

void TCPSocketEventDispatcher::OnSocketConnect(const std::string& extension_id,
                                               int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  StartSocketRead(extension_id, socket_id);
}

void TCPSocketEventDispatcher::OnSocketResume(const std::string& extension_id,
                                              int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  StartSocketRead(extension_id, socket_id);
}

void TCPSocketEventDispatcher::StartSocketRead(const std::string& extension_id,
                                               int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  ReadParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartRead(params);
}

// static
void TCPSocketEventDispatcher::StartRead(const ReadParams& params) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  ResumableTCPSocket* socket =
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
  socket->Read(buffer_size,
               base::BindOnce(&TCPSocketEventDispatcher::ReadCallback, params));
}

// static
void TCPSocketEventDispatcher::ReadCallback(
    const ReadParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer,
    bool socket_destroying) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  // If |bytes_read| == 0, the connection has been closed by the peer.
  // If |bytes_read| < 0, there was a network error, and |bytes_read| is a value
  // from "net::ERR_".

  if (bytes_read == 0) {
    bytes_read = net::ERR_CONNECTION_CLOSED;
  }

  if (bytes_read > 0) {
    // Dispatch "onReceive" event.
    sockets_tcp::ReceiveInfo receive_info;
    receive_info.socket_id = params.socket_id;
    receive_info.data.assign(io_buffer->data(), io_buffer->data() + bytes_read);
    std::unique_ptr<base::ListValue> args =
        sockets_tcp::OnReceive::Create(receive_info);
    std::unique_ptr<Event> event(new Event(events::SOCKETS_TCP_ON_RECEIVE,
                                           sockets_tcp::OnReceive::kEventName,
                                           std::move(args)));
    PostEvent(params, std::move(event));

    // Post a task to delay the read until the socket is available, as
    // calling StartReceive at this point would error with ERR_IO_PENDING.
    base::PostTask(
        FROM_HERE, {params.thread_id},
        base::BindOnce(&TCPSocketEventDispatcher::StartRead, params));
  } else if (bytes_read == net::ERR_IO_PENDING) {
    // This happens when resuming a socket which already had an
    // active "read" callback.
  } else {
    // Dispatch "onReceiveError" event but don't start another read to avoid
    // potential infinite reads if we have a persistent network error.
    sockets_tcp::ReceiveErrorInfo receive_error_info;
    receive_error_info.socket_id = params.socket_id;
    receive_error_info.result_code = bytes_read;
    std::unique_ptr<base::ListValue> args =
        sockets_tcp::OnReceiveError::Create(receive_error_info);
    std::unique_ptr<Event> event(
        new Event(events::SOCKETS_TCP_ON_RECEIVE_ERROR,
                  sockets_tcp::OnReceiveError::kEventName, std::move(args)));
    PostEvent(params, std::move(event));

    // Do not try to access |socket| when we are destroying it.
    if (!socket_destroying) {
      // Since we got an error, the socket is now "paused" until the application
      // "resumes" it.
      ResumableTCPSocket* socket =
          params.sockets->Get(params.extension_id, params.socket_id);
      if (socket)
        socket->set_paused(true);
    }
  }
}

// static
void TCPSocketEventDispatcher::PostEvent(const ReadParams& params,
                                         std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&DispatchEvent, params.browser_context_id,
                                params.extension_id, std::move(event)));
}

// static
void TCPSocketEventDispatcher::DispatchEvent(void* browser_context_id,
                                             const std::string& extension_id,
                                             std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  EventRouter* event_router = EventRouter::Get(context);
  if (event_router)
    event_router->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace api
}  // namespace extensions
