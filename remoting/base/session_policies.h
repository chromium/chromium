// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_POLICIES_H_
#define REMOTING_BASE_SESSION_POLICIES_H_

#include <stddef.h>

#include <optional>

#include "base/time/time.h"
#include "remoting/base/port_range.h"

namespace remoting {

// Policies to be applied to the CRD host.
struct SessionPolicies {
  // Minimum value of `maximum_session_duration`, when set.
  static constexpr base::TimeDelta kMinMaximumSessionDuration =
      base::Minutes(30);

  bool operator==(const SessionPolicies&) const;

  // The maximum size, in bytes, that can be transferred between client and host
  // via clipboard synchronization. Defaults to no restrictions. Setting it to 0
  // disables clipboard sync.
  // Corresponding Chrome policy: RemoteAccessHostClipboardSizeBytes
  std::optional<size_t> clipboard_size_bytes;

  // Allow connections over STUN. Defaults to true.
  // Corresponding Chrome policy: RemoteAccessHostFirewallTraversal
  std::optional<bool> allow_stun_connections;

  // Allow connections over a relay server. Defaults to true.
  // Corresponding Chrome policies:
  // RemoteAccessHostFirewallTraversal && RemoteAccessHostAllowRelayedConnection
  std::optional<bool> allow_relayed_connections;

  // Restrict the UDP port range used by the remote access host. No
  // restrictions if the port range is null.
  // Corresponding Chrome policy: RemoteAccessHostUdpPortRange
  PortRange host_udp_port_range;

  // Allow transferring files between the host and the client. Defaults to true.
  // Corresponding Chrome policy: RemoteAccessHostAllowFileTransfer
  std::optional<bool> allow_file_transfer;

  // Allow opening a host-side URI on the client browser. Defaults to true.
  // Corresponding Chrome policy: RemoteAccessHostAllowUrlForwarding
  std::optional<bool> allow_uri_forwarding;

  // Maximum session duration allowed for remote access connections. Defaults to
  // no restrictions. When set, minimum value is 30 minutes
  // Corresponding Chrome policy: RemoteAccessHostMaximumSessionDurationMinutes
  std::optional<base::TimeDelta> maximum_session_duration;

  // Enable curtaining of remote access hosts. Defaults to false.
  // Corresponding Chrome policy: RemoteAccessHostRequireCurtain
  std::optional<bool> curtain_required;
};

std::ostream& operator<<(std::ostream& os,
                         const SessionPolicies& session_policies);

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_POLICIES_H_
