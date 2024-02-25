// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"
#include "extensions/common/extension_id.h"

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
class TCPSocketEventDispatcher : public BrowserContextKeyedAPI {
 public:
  explicit TCPSocketEventDispatcher(content::BrowserContext* context);
  ~TCPSocketEventDispatcher() override;

  // Socket is active, start receving from it.
  void OnSocketConnect(const ExtensionId& extension_id, int socket_id);

  // Socket is active again, start receiving data from it.
  void OnSocketResume(const ExtensionId& extension_id, int socket_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TCPSocketEventDispatcher>*
      GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static TCPSocketEventDispatcher* Get(content::BrowserContext* context);

 private:
  using SocketData = ApiResourceManager<ResumableTCPSocket>::ApiResourceData;
  friend class BrowserContextKeyedAPIFactory<TCPSocketEventDispatcher>;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "TCPSocketEventDispatcher"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. ReadParams is used
  // as a workaround that limitation for invoking StartReceive.
  struct ReadParams {
    ReadParams();
    ReadParams(const ReadParams& other);
    ~ReadParams();

    content::BrowserThread::ID thread_id;
    raw_ptr<void> browser_context_id;
    ExtensionId extension_id;
    scoped_refptr<SocketData> sockets;
    int socket_id;
  };

  // Start a receive and register a callback.
  void StartSocketRead(const ExtensionId& extension_id, int socket_id);

  // Start a receive and register a callback.
  static void StartRead(const ReadParams& params);

  // Called when socket receive data.
  static void ReadCallback(const ReadParams& params,
                           int bytes_read,
                           scoped_refptr<net::IOBuffer> io_buffer,
                           bool socket_destroying);

  // Post an extension event from IO to UI thread
  static void PostEvent(const ReadParams& params, std::unique_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* browser_context_id,
                            const ExtensionId& extension_id,
                            std::unique_ptr<Event> event);

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  const raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<SocketData> sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_
