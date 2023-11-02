// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ACTIVITY_MONITOR_H_
#define NET_BASE_NETWORK_ACTIVITY_MONITOR_H_

#include <cstdint>

#include "net/base/net_export.h"

namespace net::activity_monitor {

// These functions are used to track bytes received from the network across all
// sockets. They are thread-safe.
//
// There are a few caveats:
//  * Bytes received includes only bytes actually received from the network, and
//    does not include any bytes read from the the cache.
//  * Network activity not initiated directly using chromium sockets won't be
//    reflected here (for instance DNS queries issued by getaddrinfo()).
//
// Free functions are used instead of a singleton, to avoid memory barriers
// associated with singleton initialization.
void NET_EXPORT_PRIVATE IncrementBytesReceived(uint64_t bytes_received);
uint64_t NET_EXPORT_PRIVATE GetBytesReceived();
void NET_EXPORT_PRIVATE ResetBytesReceivedForTesting();

}  // namespace net::activity_monitor

#endif  // NET_BASE_NETWORK_ACTIVITY_MONITOR_H_
