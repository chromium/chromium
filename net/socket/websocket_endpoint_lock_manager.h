// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
#define NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/websocket_transport_client_socket_pool.h"

namespace net {

// Keep track of ongoing WebSocket connections in order to satisfy the WebSocket
// connection throttling requirements described in RFC6455 4.1.2:
//
//   2.  If the client already has a WebSocket connection to the remote
//       host (IP address) identified by /host/ and port /port/ pair, even
//       if the remote host is known by another name, the client MUST wait
//       until that connection has been established or for that connection
//       to have failed.  There MUST be no more than one connection in a
//       CONNECTING state.  If multiple connections to the same IP address
//       are attempted simultaneously, the client MUST serialize them so
//       that there is no more than one connection at a time running
//       through the following steps.
//
class NET_EXPORT_PRIVATE WebSocketEndpointLockManager {
 public:
  // Implement this interface to wait for an endpoint to be available.
  class NET_EXPORT_PRIVATE Waiter : public base::LinkNode<Waiter> {
   public:
    // If the node is in a list, removes it.
    virtual ~Waiter();

    virtual void GotEndpointLock() = 0;
  };

  // LockReleaser calls UnlockEndpoint() when it is destroyed, but only if it
  // has not already been called. Only one LockReleaser object may exist for
  // each endpoint at a time.
  class NET_EXPORT_PRIVATE LockReleaser final {
   public:
    LockReleaser(WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
                 IPEndPoint endpoint);

    LockReleaser(const LockReleaser&) = delete;
    LockReleaser& operator=(const LockReleaser&) = delete;

    ~LockReleaser();

   private:
    friend class WebSocketEndpointLockManager;

    // This is null if UnlockEndpoint() has been called before this object was
    // destroyed.
    raw_ptr<WebSocketEndpointLockManager> websocket_endpoint_lock_manager_;
    const IPEndPoint endpoint_;
  };

  WebSocketEndpointLockManager();

  WebSocketEndpointLockManager(const WebSocketEndpointLockManager&) = delete;
  WebSocketEndpointLockManager& operator=(const WebSocketEndpointLockManager&) =
      delete;

  ~WebSocketEndpointLockManager();

  // Returns OK if lock was acquired immediately, ERR_IO_PENDING if not. If the
  // lock was not acquired, then |waiter->GotEndpointLock()| will be called when
  // it is. A Waiter automatically removes itself from the list of waiters when
  // its destructor is called.
  int LockEndpoint(const IPEndPoint& endpoint, Waiter* waiter);

  // Asynchronously releases the lock on |endpoint| after a delay. Does nothing
  // if |endpoint| is not locked. If a LockReleaser object has been created for
  // this endpoint, it will be unregistered.
  void UnlockEndpoint(const IPEndPoint& endpoint);

  // Checks that |lock_info_map_| is empty. For tests.
  bool IsEmpty() const;

  // Changes the value of the unlock delay. Returns the previous value of the
  // delay.
  base::TimeDelta SetUnlockDelayForTesting(base::TimeDelta new_delay);

 private:
  struct LockInfo {
    typedef base::LinkedList<Waiter> WaiterQueue;

    LockInfo();
    ~LockInfo();

    // This object must be copyable to be placed in the map, but it cannot be
    // copied after |queue| has been assigned to.
    LockInfo(const LockInfo& rhs);

    // Not used.
    LockInfo& operator=(const LockInfo& rhs);

    // Must be NULL to copy this object into the map. Must be set to non-NULL
    // after the object is inserted into the map then point to the same list
    // until this object is deleted.
    std::unique_ptr<WaiterQueue> queue;

    // This pointer is non-NULL if a LockReleaser object has been constructed
    // since the last call to UnlockEndpoint().
    raw_ptr<LockReleaser> lock_releaser;
  };

  // SocketLockInfoMap requires std::map iterator semantics for LockInfoMap
  // (ie. that the iterator will remain valid as long as the entry is not
  // deleted).
  typedef std::map<IPEndPoint, LockInfo> LockInfoMap;

  // Records the association of a LockReleaser with a particular endpoint.
  void RegisterLockReleaser(LockReleaser* lock_releaser, IPEndPoint endpoint);
  void UnlockEndpointAfterDelay(const IPEndPoint& endpoint);
  void DelayedUnlockEndpoint(const IPEndPoint& endpoint);

  // If an entry is present in the map for a particular endpoint, then that
  // endpoint is locked. If LockInfo.queue is non-empty, then one or more
  // Waiters are waiting for the lock.
  LockInfoMap lock_info_map_;

  // Time to wait between a call to Unlock* and actually unlocking the socket.
  base::TimeDelta unlock_delay_;

  // Number of sockets currently pending unlock.
  size_t pending_unlock_count_ = 0;

  base::WeakPtrFactory<WebSocketEndpointLockManager> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
