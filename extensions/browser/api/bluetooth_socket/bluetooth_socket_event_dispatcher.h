// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace device {
class BluetoothDevice;
class BluetoothSocket;
}

namespace extensions {
struct Event;
class BluetoothApiSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "bluetooth" sockets from callback on native socket
// instances. There is one instance per browser context.
class BluetoothSocketEventDispatcher : public BrowserContextKeyedAPI {
 public:
  explicit BluetoothSocketEventDispatcher(content::BrowserContext* context);
  ~BluetoothSocketEventDispatcher() override;

  // Socket is active, start receiving data from it.
  void OnSocketConnect(const ExtensionId& extension_id, int socket_id);

  // Socket is active again, start accepting connections from it.
  void OnSocketListen(const ExtensionId& extension_id, int socket_id);

  // Socket is active again, start receiving data from it.
  void OnSocketResume(const ExtensionId& extension_id, int socket_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>*
      GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static BluetoothSocketEventDispatcher* Get(content::BrowserContext* context);

 private:
  using SocketData = ApiResourceManager<BluetoothApiSocket>::ApiResourceData;
  friend class BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothSocketEventDispatcher"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. SocketParams is used
  // as a workaround that limitation for invoking StartReceive() and
  // StartAccept().
  struct SocketParams {
    SocketParams();
    SocketParams(const SocketParams& other);
    ~SocketParams();

    content::BrowserThread::ID thread_id;
    raw_ptr<void> browser_context_id;
    ExtensionId extension_id;
    scoped_refptr<SocketData> sockets;
    int socket_id;
  };

  // Start a receive and register a callback.
  static void StartReceive(const SocketParams& params);

  // Called when socket receive data.
  static void ReceiveCallback(const SocketParams& params,
                              int bytes_read,
                              scoped_refptr<net::IOBuffer> io_buffer);

  // Called when socket receive data.
  static void ReceiveErrorCallback(const SocketParams& params,
                                   BluetoothApiSocket::ErrorReason error_reason,
                                   const std::string& error);

  // Start an accept and register a callback.
  static void StartAccept(const SocketParams& params);

  // Called when socket accepts a client connection.
  static void AcceptCallback(const SocketParams& params,
                             const device::BluetoothDevice* device,
                             scoped_refptr<device::BluetoothSocket> socket);

  // Called when socket encounters an error while accepting a client connection.
  static void AcceptErrorCallback(const SocketParams& params,
                                  BluetoothApiSocket::ErrorReason error_reason,
                                  const std::string& error);

  // Post an extension event from IO to UI thread
  static void PostEvent(const SocketParams& params,
                        std::unique_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* browser_context_id,
                            const ExtensionId& extension_id,
                            std::unique_ptr<Event> event);

  // Usually FILE thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  const raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<SocketData> sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_
