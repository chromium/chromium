// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_H_
#define NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_H_

#include "net/base/net_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace net {

// SocketPerformanceWatcher is the base class for recording and aggregating
// per-socket statistics. SocketPerformanceWatcher must be used on a single
// thread.
class NET_EXPORT_PRIVATE SocketPerformanceWatcher {
 public:
  virtual ~SocketPerformanceWatcher() = default;

  // Returns true if |this| SocketPerformanceWatcher is interested in receiving
  // an updated RTT estimate (via OnUpdatedRTTAvailable).
  virtual bool ShouldNotifyUpdatedRTT() const = 0;

  // Notifies |this| SocketPerformanceWatcher of updated transport layer RTT
  // from this device to the remote transport layer endpoint. This method is
  // called immediately after the observation is made, hence no timestamp.
  // There is no guarantee that OnUpdatedRTTAvailable will be called every time
  // an updated RTT is available as the socket may throttle the
  // OnUpdatedRTTAvailable call for various reasons, including performance.
  virtual void OnUpdatedRTTAvailable(const base::TimeDelta& rtt) = 0;

  // Notifies that |this| watcher will be reused to watch a socket that belongs
  // to a different transport layer connection. Note: The new connection shares
  // the same protocol as the previously watched socket.
  virtual void OnConnectionChanged() = 0;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_H_
