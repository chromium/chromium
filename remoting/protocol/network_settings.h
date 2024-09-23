// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NETWORK_SETTINGS_H_
#define REMOTING_PROTOCOL_NETWORK_SETTINGS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "remoting/base/port_range.h"
#include "remoting/base/session_policies.h"

namespace remoting::protocol {

struct NetworkSettings {
  // When hosts are configured with NAT traversal disabled they will
  // typically also limit their P2P ports to this range, so that
  // sessions may be blocked or un-blocked via firewall rules.
  static constexpr uint16_t kDefaultMinPort = 12400;
  static constexpr uint16_t kDefaultMaxPort = 12409;

  enum Flags : uint32_t {
    // Don't use STUN or relay servers. Accept incoming P2P connection
    // attempts, but don't initiate any. This ensures that the peer is
    // on the same network. Note that connection will always fail if
    // both ends use this mode.
    NAT_TRAVERSAL_DISABLED = 0x0u,

    // Allow outgoing connections, even when STUN and RELAY are not enabled.
    NAT_TRAVERSAL_OUTGOING = 0x1u,

    // Active NAT traversal using STUN.
    NAT_TRAVERSAL_STUN = 0x2u,

    // Allow the use of relay servers when a direct connection is not available.
    NAT_TRAVERSAL_RELAY = 0x4u,

    // Active NAT traversal using STUN and relay servers.
    NAT_TRAVERSAL_FULL =
        NAT_TRAVERSAL_STUN | NAT_TRAVERSAL_RELAY | NAT_TRAVERSAL_OUTGOING
  };

  NetworkSettings();
  ~NetworkSettings();

  explicit NetworkSettings(uint32_t flags);
  explicit NetworkSettings(const SessionPolicies& policies);

  uint32_t flags = NAT_TRAVERSAL_DISABLED;

  // Range of ports used by P2P sessions.
  PortRange port_range;

  // ICE Timeout.
  base::TimeDelta ice_timeout = base::Seconds(15);

  // ICE reconnect attempts.
  int ice_reconnect_attempts = 2;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_NETWORK_SETTINGS_H_
