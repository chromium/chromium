// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_
#define NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"

namespace net {

class IPAddress;
class SocketPerformanceWatcher;

// SocketPerformanceWatcherFactory creates socket performance watcher for
// different type of sockets.
class NET_EXPORT_PRIVATE SocketPerformanceWatcherFactory {
 public:
  // Transport layer protocol used by the socket that are supported by
  // |SocketPerformanceWatcherFactory|.
  enum Protocol { PROTOCOL_TCP, PROTOCOL_QUIC };

  SocketPerformanceWatcherFactory(const SocketPerformanceWatcherFactory&) =
      delete;
  SocketPerformanceWatcherFactory& operator=(
      const SocketPerformanceWatcherFactory&) = delete;

  virtual ~SocketPerformanceWatcherFactory() = default;

  // Creates a socket performance watcher that will record statistics for a
  // single socket that uses |protocol| as the transport layer protocol.
  // |ip_address| is the IP address that the socket is going to connect to.
  // Implementations must return a valid, unique SocketRecorder for every call;
  // recorders must not be shared across calls or objects, nor is nullptr valid.
  virtual std::unique_ptr<SocketPerformanceWatcher>
  CreateSocketPerformanceWatcher(const Protocol protocol,
                                 const IPAddress& ip_address) = 0;

 protected:
  SocketPerformanceWatcherFactory() = default;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_PERFORMANCE_WATCHER_FACTORY_H_
