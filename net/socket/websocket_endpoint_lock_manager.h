// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_
#define NET_SOCKET_WEBSOCKET_ENDPOINT_LOCK_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/containers/linked_list.h"
#include "base/functional/callback.h"
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
 private:
  // Needs to be declared here for use in LockEndpoint.
  struct LockInfo;

 public:
  // Single-use class that is be used to wait until an endpoint is available
  // without blocking, at which point it will obtain the lock to the endpoint
  // and inform the caller. On destruction, EndpointLock will call
  // UnlockEndpoint(), but only if it has both obtained a lock and that lock has
  // not already been released. Only one EndpointLock object may have a lock for
  // any endpoint at a time. It is safe to destroy an EndpointLock at any time,
  // including before it has a lock.
  class NET_EXPORT_PRIVATE EndpointLock final
      : public base::LinkNode<EndpointLock> {
   public:
    EndpointLock(WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
                 const IPEndPoint& endpoint);

    EndpointLock(const EndpointLock&) = delete;
    EndpointLock& operator=(const EndpointLock&) = delete;

    ~EndpointLock();

    int LockEndpoint(base::OnceClosure lock_callback);

   private:
    friend class WebSocketEndpointLockManager;

    void GotEndpointLock();

    raw_ptr<WebSocketEndpointLockManager> websocket_endpoint_lock_manager_;
    const IPEndPoint endpoint_;

    base::OnceClosure lock_callback_;
    // The LockInfo when this class holds the lock. If non-null,
    // `lock_info_->endpoint_lock` must be `this`.
    //
    // May only be modified SetLock() / ClearLock().
    raw_ptr<LockInfo> lock_info_;
  };

  WebSocketEndpointLockManager();

  WebSocketEndpointLockManager(const WebSocketEndpointLockManager&) = delete;
  WebSocketEndpointLockManager& operator=(const WebSocketEndpointLockManager&) =
      delete;

  ~WebSocketEndpointLockManager();

  // Asynchronously releases the lock on |endpoint| after a delay. Does nothing
  // if |endpoint| is not locked. If an EndpointLock object has been created for
  // this endpoint, it will be unregistered.
  void UnlockEndpoint(const IPEndPoint& endpoint);

  // Checks that |lock_info_map_| is empty. For tests.
  bool IsEmpty() const;

  // Changes the value of the unlock delay. Returns the previous value of the
  // delay.
  base::TimeDelta SetUnlockDelayForTesting(base::TimeDelta new_delay);

 private:
  struct LockInfo {
    typedef base::LinkedList<EndpointLock> WaiterQueue;

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

    // This pointer is non-NULL if an EndpointLock object has been constructed
    // since the last call to UnlockEndpoint().  If non-null,
    // `endpoint_lock->lock_info_` must be `this`.
    //
    // May only be modified SetLock() / ClearLock().
    raw_ptr<EndpointLock> endpoint_lock;
  };

  // SocketLockInfoMap requires std::map iterator semantics for LockInfoMap
  // (ie. that the iterator will remain valid as long as the entry is not
  // deleted).
  typedef std::map<IPEndPoint, LockInfo> LockInfoMap;

  // Returns OK if lock was acquired immediately, ERR_IO_PENDING if not. If the
  // lock was not acquired, then `endpoint_lock->GotEndpointLock()` will be
  // called when it is. An EndpointLock automatically removes itself from the
  // list of waiters when its destructor is called.
  int LockEndpoint(const IPEndPoint& endpoint, EndpointLock* endpoint_lock);

  // Asynchronously releases the lock represented by `lock_info` after a delay.
  // If an EndpointLock object has been created for this endpoint, it will be
  // unregistered.
  //
  // Separate function from UnlockEndpoint so ~EndpointLock() can unlock an
  // endpoint without a search.
  void UnlockEndpointInternal(const IPEndPoint& endpoint, LockInfo& lock_info);

  // Records the association of an EndpointLock with a particular endpoint.
  void UnlockEndpointAfterDelay(const IPEndPoint& endpoint);
  void DelayedUnlockEndpoint(const IPEndPoint& endpoint);

  // Set/Clear the pointers in `lock_info` and `endpoint_lock` to point at
  // each other, when a lock is held/released. These are bookkeeping helper
  // functions. Once SetLock() is called, ClearLock() must be called before
  // SetLock() is called again on the same LockInfo. SetLock() take pointers
  // rather than refs because SetLock() sets both objects to point at each
  // other.
  void SetLock(LockInfo* lock_info, EndpointLock* endpoint_lock);
  void ClearLock(LockInfo& lock_info);

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
