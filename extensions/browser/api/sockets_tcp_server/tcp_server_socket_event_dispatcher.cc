// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_id.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    TCPServerSocketEventDispatcher>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TCPServerSocketEventDispatcher>*
TCPServerSocketEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
TCPServerSocketEventDispatcher* TCPServerSocketEventDispatcher::Get(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return BrowserContextKeyedAPIFactory<TCPServerSocketEventDispatcher>::Get(
      context);
}

TCPServerSocketEventDispatcher::TCPServerSocketEventDispatcher(
    content::BrowserContext* context)
    : thread_id_(Socket::kThreadId), browser_context_(context) {
  ApiResourceManager<ResumableTCPServerSocket>* server_manager =
      ApiResourceManager<ResumableTCPServerSocket>::Get(browser_context_);
  DCHECK(server_manager)
      << "There is no server socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<ResumableTCPServerSocket>.";
  server_sockets_ = server_manager->data_;

  ApiResourceManager<ResumableTCPSocket>* client_manager =
      ApiResourceManager<ResumableTCPSocket>::Get(browser_context_);
  DCHECK(client_manager)
      << "There is no client socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<ResumableTCPSocket>.";
  client_sockets_ = client_manager->data_;
}

TCPServerSocketEventDispatcher::~TCPServerSocketEventDispatcher() = default;

TCPServerSocketEventDispatcher::AcceptParams::AcceptParams() = default;

TCPServerSocketEventDispatcher::AcceptParams::AcceptParams(
    const AcceptParams& other) = default;

TCPServerSocketEventDispatcher::AcceptParams::~AcceptParams() = default;

void TCPServerSocketEventDispatcher::OnServerSocketListen(
    const ExtensionId& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  StartSocketAccept(extension_id, socket_id);
}

void TCPServerSocketEventDispatcher::OnServerSocketResume(
    const ExtensionId& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  StartSocketAccept(extension_id, socket_id);
}

void TCPServerSocketEventDispatcher::StartSocketAccept(
    const ExtensionId& extension_id,
    int socket_id) {
  DCHECK_CURRENTLY_ON(thread_id_);

  AcceptParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.server_sockets = server_sockets_;
  params.client_sockets = client_sockets_;
  params.socket_id = socket_id;

  StartAccept(params);
}

// static
void TCPServerSocketEventDispatcher::StartAccept(const AcceptParams& params) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  ResumableTCPServerSocket* socket =
      params.server_sockets->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
      << "Socket has wrong owner.";

  // Don't start another accept if the socket has been paused.
  if (socket->paused()) {
    return;
  }

  socket->Accept(
      base::BindOnce(&TCPServerSocketEventDispatcher::AcceptCallback, params));
}

// static
void TCPServerSocketEventDispatcher::AcceptCallback(
    const AcceptParams& params,
    int result_code,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
    const std::optional<net::IPEndPoint>& remote_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  DCHECK_CURRENTLY_ON(params.thread_id);
  DCHECK_GE(net::OK, result_code);

  if (result_code == net::OK) {
    ResumableTCPSocket* client_socket = new ResumableTCPSocket(
        std::move(socket), std::move(receive_pipe_handle),
        std::move(send_pipe_handle), remote_addr, params.extension_id);
    client_socket->set_paused(true);
    int client_socket_id = params.client_sockets->Add(client_socket);

    // Dispatch "onAccept" event.
    sockets_tcp_server::AcceptInfo accept_info;
    accept_info.socket_id = params.socket_id;
    accept_info.client_socket_id = client_socket_id;
    auto args = sockets_tcp_server::OnAccept::Create(accept_info);
    std::unique_ptr<Event> event(
        new Event(events::SOCKETS_TCP_SERVER_ON_ACCEPT,
                  sockets_tcp_server::OnAccept::kEventName, std::move(args)));
    PostEvent(params, std::move(event));

    // Post a task to delay the "accept" until the socket is available, as
    // calling StartAccept at this point would error with ERR_IO_PENDING.
    content::BrowserThread::GetTaskRunnerForThread(params.thread_id)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&TCPServerSocketEventDispatcher::StartAccept,
                                  params));
  } else {
    // Dispatch "onAcceptError" event but don't start another accept to avoid
    // potential infinite "accepts" if we have a persistent network error.
    sockets_tcp_server::AcceptErrorInfo accept_error_info;
    accept_error_info.socket_id = params.socket_id;
    accept_error_info.result_code = result_code;
    auto args = sockets_tcp_server::OnAcceptError::Create(accept_error_info);
    std::unique_ptr<Event> event(new Event(
        events::SOCKETS_TCP_SERVER_ON_ACCEPT_ERROR,
        sockets_tcp_server::OnAcceptError::kEventName, std::move(args)));
    PostEvent(params, std::move(event));

    // Since we got an error, the socket is now "paused" until the application
    // "resumes" it.
    ResumableTCPServerSocket* server_socket =
        params.server_sockets->Get(params.extension_id, params.socket_id);
    if (server_socket) {
      server_socket->set_paused(true);
    }
  }
}

// static
void TCPServerSocketEventDispatcher::PostEvent(const AcceptParams& params,
                                               std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(params.thread_id);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DispatchEvent, params.browser_context_id,
                                params.extension_id, std::move(event)));
}

// static
void TCPServerSocketEventDispatcher::DispatchEvent(
    void* browser_context_id,
    const ExtensionId& extension_id,
    std::unique_ptr<Event> event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context_id)) {
    return;
  }
  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  EventRouter* router = EventRouter::Get(context);
  if (router) {
#if BUILDFLAG(IS_CHROMEOS)
    // Terminal app is the only non-extension to use sockets
    // (crbug.com/1350479).
    if (extension_id == kCrOSTerminal) {
      router->DispatchEventToURL(GURL(extension_id), std::move(event));
      return;
    }
#endif
    router->DispatchEventToExtension(extension_id, std::move(event));
  }
}

}  // namespace api
}  // namespace extensions
