// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_

#include <stdint.h>

#include <string>

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"

namespace content {
class BrowserContext;
}

namespace extensions {
struct Event;
class ResumableUDPSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "sockets.udp" sockets from callback on native
// socket instances. There is one instance per profile.
class UDPSocketEventDispatcher
    : public BrowserContextKeyedAPI,
      public base::SupportsWeakPtr<UDPSocketEventDispatcher> {
 public:
  explicit UDPSocketEventDispatcher(content::BrowserContext* context);
  ~UDPSocketEventDispatcher() override;

  // Socket is active, start receving from it.
  void OnSocketBind(const std::string& extension_id, int socket_id);

  // Socket is active again, start receiving data from it.
  void OnSocketResume(const std::string& extension_id, int socket_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<UDPSocketEventDispatcher>*
      GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static UDPSocketEventDispatcher* Get(content::BrowserContext* context);

 private:
  typedef ApiResourceManager<ResumableUDPSocket>::ApiResourceData SocketData;
  friend class BrowserContextKeyedAPIFactory<UDPSocketEventDispatcher>;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UDPSocketEventDispatcher"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. ReceiveParams is used
  // as a workaround that limitation for invoking StartReceive.
  struct ReceiveParams {
    ReceiveParams();
    ReceiveParams(const ReceiveParams& other);
    ~ReceiveParams();

    content::BrowserThread::ID thread_id;
    void* browser_context_id;
    std::string extension_id;
    scoped_refptr<SocketData> sockets;
    int socket_id;
  };

  // Start a receive and register a callback.
  static void StartReceive(const ReceiveParams& params);

  // Called when socket receive data.
  static void ReceiveCallback(const ReceiveParams& params,
                              int bytes_read,
                              scoped_refptr<net::IOBuffer> io_buffer,
                              bool socket_destroying,
                              const std::string& address,
                              uint16_t port);

  // Post an extension event from IO to UI thread
  static void PostEvent(const ReceiveParams& params,
                        std::unique_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* browser_context_id,
                            const std::string& extension_id,
                            std::unique_ptr<Event> event);

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  content::BrowserContext* const browser_context_;
  scoped_refptr<SocketData> sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
