// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ACTIVITY_MONITOR_H_
#define NET_BASE_NETWORK_ACTIVITY_MONITOR_H_

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

namespace test {

class NetworkActivityMonitorPeer;

}  // namespace test

// NetworkActivityMonitor tracks network activity across all sockets and
// provides cumulative statistics about bytes sent to and received from
// the network. It uses a lock to ensure thread-safety.
//
// There are a few caveats:
//  * Bytes "sent" includes all send attempts and may include
//    some bytes which were actually never sent over the network.
//  * Bytes received includes only bytes actually received from the network,
//    and does not include any bytes read from the the cache.
//  * Network activity not initiated directly using chromium sockets won't
//    be reflected here (for instance DNS queries issued by getaddrinfo()).
class NET_EXPORT_PRIVATE NetworkActivityMonitor {
 public:
  // Returns the singleton instance of the monitor.
  static NetworkActivityMonitor* GetInstance();

  NetworkActivityMonitor(const NetworkActivityMonitor&) = delete;
  NetworkActivityMonitor& operator=(const NetworkActivityMonitor&) = delete;

  void IncrementBytesReceived(uint64_t bytes_received);
  void IncrementBytesSent(uint64_t bytes_sent);

  uint64_t GetBytesReceived() const;
  uint64_t GetBytesSent() const;

  base::TimeDelta GetTimeSinceLastReceived() const;
  base::TimeDelta GetTimeSinceLastSent() const;

 private:
  friend class test::NetworkActivityMonitorPeer;

  NetworkActivityMonitor();
  ~NetworkActivityMonitor();
  friend struct base::LazyInstanceTraitsBase<NetworkActivityMonitor>;

  // Protects all the following members.
  mutable base::Lock lock_;

  uint64_t bytes_received_;
  uint64_t bytes_sent_;

  base::TimeTicks last_received_ticks_;
  base::TimeTicks last_sent_ticks_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_ACTIVITY_MONITOR_H_
