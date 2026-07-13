// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DELAYED_SOCKET_CONFIG_H_
#define NET_SOCKET_DELAYED_SOCKET_CONFIG_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// Configuration for simulated network delay applied by the Delayed socket
// wrappers (DelayedStreamSocket / DelayedDatagramSocket). Lives in its own
// header so TCP and UDP wrappers depend on this small shared definition
// rather than on each other.
struct NET_EXPORT DelayedSocketConfig {
  // Round-trip time. Stream sockets apply half of this on each direction;
  // datagram sockets apply half-RTT per packet.
  base::TimeDelta rtt;

  // Download throughput in bytes per second. std::nullopt means the download
  // direction is unconstrained (no bandwidth shaping); a default-constructed
  // config is therefore unconstrained.
  std::optional<uint64_t> download_throughput_bytes_per_sec;

  // Upload throughput in bytes per second. std::nullopt means the upload
  // direction is unconstrained (no bandwidth shaping).
  std::optional<uint64_t> upload_throughput_bytes_per_sec;
};

}  // namespace net

#endif  // NET_SOCKET_DELAYED_SOCKET_CONFIG_H_
