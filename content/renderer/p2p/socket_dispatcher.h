// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// P2PSocketDispatcher is a per-renderer object that dispatchers all
// P2P messages received from the browser and relays all P2P messages
// sent to the browser. P2PSocketClient instances register themselves
// with the dispatcher using RegisterClient() and UnregisterClient().
//
// Relationship of classes.
//
//       P2PSocketHost                     P2PSocketClient
//            ^                                   ^
//            |                                   |
//            v                  IPC              v
//  P2PSocketDispatcherHost  <--------->  P2PSocketDispatcher
//
// P2PSocketDispatcher receives and dispatches messages on the
// IO thread.

#ifndef CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
#define CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/renderer/p2p/network_list_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

class NetworkListObserver;

// This class is created on the main thread, but is used primarily on the
// WebRTC worker threads.
class CONTENT_EXPORT P2PSocketDispatcher
    : public base::RefCountedThreadSafe<P2PSocketDispatcher>,
      public NetworkListManager,
      public network::mojom::P2PNetworkNotificationClient {
 public:
  P2PSocketDispatcher();

  // NetworkListManager interface:
  void AddNetworkListObserver(
      NetworkListObserver* network_list_observer) override;
  void RemoveNetworkListObserver(
      NetworkListObserver* network_list_observer) override;

  scoped_refptr<network::mojom::ThreadSafeP2PSocketManagerPtr>
  GetP2PSocketManager();

 private:
  friend class base::RefCountedThreadSafe<P2PSocketDispatcher>;

  ~P2PSocketDispatcher() override;

  // network::mojom::P2PNetworkNotificationClient interface.
  void NetworkListChanged(
      const std::vector<net::NetworkInterface>& networks,
      const net::IPAddress& default_ipv4_local_address,
      const net::IPAddress& default_ipv6_local_address) override;

  void RequestInterfaceIfNecessary();
  void RequestNetworkEventsIfNecessary();

  void OnConnectionError();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<base::ObserverListThreadSafe<NetworkListObserver>>
      network_list_observers_;

  network::mojom::P2PSocketManagerRequest p2p_socket_manager_request_;
  scoped_refptr<network::mojom::ThreadSafeP2PSocketManagerPtr>
      thread_safe_p2p_socket_manager_;
  base::Lock p2p_socket_manager_lock_;

  // Cached from last |NetworkListChanged| call.
  std::vector<net::NetworkInterface> networks_;
  net::IPAddress default_ipv4_local_address_;
  net::IPAddress default_ipv6_local_address_;

  mojo::Binding<network::mojom::P2PNetworkNotificationClient>
      network_notification_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_SOCKET_DISPATCHER_H_
