// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_RECONNECT_NOTIFIER_H_
#define NET_BASE_RECONNECT_NOTIFIER_H_

#include <cstdint>
#include <optional>

#include "net/base/net_export.h"

namespace net {

// Keeps track of the relevant information to conduct connection keep-alive.
struct NET_EXPORT ConnectionKeepAliveConfig {
  ConnectionKeepAliveConfig() = default;
  ~ConnectionKeepAliveConfig() = default;

  ConnectionKeepAliveConfig(const ConnectionKeepAliveConfig& other) = default;
  ConnectionKeepAliveConfig& operator=(const ConnectionKeepAliveConfig& other) =
      default;
  ConnectionKeepAliveConfig(ConnectionKeepAliveConfig&& other) = default;
  ConnectionKeepAliveConfig& operator=(ConnectionKeepAliveConfig&& other) =
      default;

  // Timeout for the session to be closed. Counted from the last successful
  // PING.
  uint32_t idle_timeout_in_seconds = 0;

  // Interval between two pings. Counted from the last ping. This should be
  // reasonably shorter than `idle_timeout` so that a PING frame can be
  // exchanged before the idle timeout.
  uint32_t ping_interval_in_seconds = 0;
};

// Keeps track of the connection management relevant information (e.g.
// connection keep alive configs, reconnect notification configs) to be passed
// on to the underlying connection.
struct NET_EXPORT ConnectionManagementConfig {
  ConnectionManagementConfig() = default;
  ~ConnectionManagementConfig() = default;

  // connection keep alive related information.
  std::optional<ConnectionKeepAliveConfig> keep_alive_config;
};

}  // namespace net

#endif  // NET_BASE_RECONNECT_NOTIFIER_H_
