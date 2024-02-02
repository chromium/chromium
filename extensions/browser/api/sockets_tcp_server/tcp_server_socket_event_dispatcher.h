// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"
#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {
struct Event;
class ResumableTCPSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "sockets.tcp" sockets from callback on native
// socket instances. There is one instance per profile.
class TCPServerSocketEventDispatcher : public BrowserContextKeyedAPI {
 public:
  explicit TCPServerSocketEventDispatcher(content::BrowserContext* context);
  ~TCPServerSocketEventDispatcher() override;

  // Server socket is active, start accepting connections from it.
  void OnServerSocketListen(const ExtensionId& extension_id, int socket_id);

  // Server socket is active again, start accepting connections from it.
  void OnServerSocketResume(const ExtensionId& extension_id, int socket_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TCPServerSocketEventDispatcher>*
      GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static TCPServerSocketEventDispatcher* Get(content::BrowserContext* context);

 private:
  typedef ApiResourceManager<ResumableTCPServerSocket>::ApiResourceData
      ServerSocketData;
  typedef ApiResourceManager<ResumableTCPSocket>::ApiResourceData
      ClientSocketData;
  friend class BrowserContextKeyedAPIFactory<TCPServerSocketEventDispatcher>;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TCPServerSocketEventDispatcher"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. AcceptParams is used
  // as a workaround that limitation for invoking StartAccept.
  struct AcceptParams {
    AcceptParams();
    AcceptParams(const AcceptParams& other);
    ~AcceptParams();

    content::BrowserThread::ID thread_id;
    raw_ptr<void, LeakedDanglingUntriaged> browser_context_id;
    ExtensionId extension_id;
    scoped_refptr<ServerSocketData> server_sockets;
    scoped_refptr<ClientSocketData> client_sockets;
    int socket_id;
  };

  // Start an accept and register a callback.
  void StartSocketAccept(const ExtensionId& extension_id, int socket_id);

  // Start an accept and register a callback.
  static void StartAccept(const AcceptParams& params);

  // Called when socket accepts a new connection.
  static void AcceptCallback(
      const AcceptParams& params,
      int result,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
      const std::optional<net::IPEndPoint>& remote_addr,
      mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
      mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  // Post an extension event from |thread_id| to UI thread
  static void PostEvent(const AcceptParams& params,
                        std::unique_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* browser_context_id,
                            const ExtensionId& extension_id,
                            std::unique_ptr<Event> event);

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<ServerSocketData> server_sockets_;
  scoped_refptr<ClientSocketData> client_sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_
