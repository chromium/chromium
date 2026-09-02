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

// LINT.IfChange(SessionPolicies)
// Policies to be applied to the CRD host.
struct SessionPolicies {
  SessionPolicies();
  ~SessionPolicies();

  SessionPolicies(const SessionPolicies&);
  SessionPolicies& operator=(const SessionPolicies&);
  SessionPolicies(SessionPolicies&&);
  SessionPolicies& operator=(SessionPolicies&&);

  bool operator==(const SessionPolicies&) const;

  // The maximum size, in bytes, that can be transferred between client and host
  // via clipboard synchronization. A nullopt value means to use the default
  // value, which is no restrictions (unlimited size). Setting it to 0
  // disables clipboard sync.
  // Corresponding Chrome policy: RemoteAccessHostClipboardSizeBytes
  std::optional<size_t> clipboard_size_bytes;

  // Allow connections over STUN. A nullopt value means to use the default
  // value, which is true (allowed).
  // Corresponding Chrome policy: RemoteAccessHostFirewallTraversal
  std::optional<bool> allow_stun_connections;

  // Allow connections over a relay server. A nullopt value means to use the
  // default value, which is true (allowed).
  // Corresponding Chrome policies:
  // RemoteAccessHostFirewallTraversal && RemoteAccessHostAllowRelayedConnection
  std::optional<bool> allow_relayed_connections;

  // Restrict the UDP port range used by the remote access host.
  // A null port range (`min_port == 0 && max_port == 0`) means to use the
  // default value, which is no restrictions.
  // Corresponding Chrome policy: RemoteAccessHostUdpPortRange
  PortRange host_udp_port_range;

  // Allow transferring files between the host and the client.
  // A nullopt value means to use the default value, which is true (allowed).
  // Corresponding Chrome policy: RemoteAccessHostAllowFileTransfer
  std::optional<bool> allow_file_transfer;

  // Allow opening a host-side URI on the client browser.
  // A nullopt value means to use the default value, which is true (allowed).
  // Corresponding Chrome policy: RemoteAccessHostAllowUrlForwarding
  std::optional<bool> allow_uri_forwarding;

  // Maximum session duration allowed for remote access connections.
  // A nullopt value means to use the default value, which is no restrictions
  // (unlimited duration). Values outside the range of supported session
  // durations will be clamped to match it.
  // Corresponding Chrome policy: RemoteAccessHostMaximumSessionDurationMinutes
  std::optional<base::TimeDelta> maximum_session_duration;

  // Enable curtaining of remote access hosts.
  // A nullopt value means to use the default value, which is false
  // (not required).
  // Corresponding Chrome policy: RemoteAccessHostRequireCurtain
  std::optional<bool> curtain_required;

  // Require that the name of the local user and the remote access host owner
  // match. For example, if the host owner's email address is foo@gmail.com,
  // then the local user of the OS must be foo.
  // A nullopt value means to use the default value, which is false
  // (not required).
  // Corresponding Chrome policy: RemoteAccessHostMatchUsername
  std::optional<bool> host_username_match_required;

  // Allow the client to remotely control the host. When disabled the host will
  // be in a view-only session.
  // A nullopt value means to use the default value, which is true (allowed).
  std::optional<bool> allow_remote_input;

  // Allow the client to service WebAuthn request generated on the host machine.
  // A nullopt value means to use the default value, which is true (allowed).
  // No Corresponding Chrome Policy as the admin can block installation of the
  // WebAuthn forwarding extension if needed.
  std::optional<bool> allow_webauthn_forwarding;

  // Allow the client to service security key (gnubby) requests generated on the
  // host machine.
  // A nullopt value means to use the default value, which is true (allowed).
  // Corresponding Chrome policy: RemoteAccessHostAllowGnubbyAuth
  std::optional<bool> allow_gnubby_forwarding;

  // Allow the client to establish terminal sessions on supported platforms.
  // A nullopt value means to use the default value, which is true (allowed).
  std::optional<bool> allow_terminal_mode;
};
// LINT.ThenChange(//remoting/host/mojom/common.mojom:SessionPolicies)

std::ostream& operator<<(std::ostream& os,
                         const SessionPolicies& session_policies);

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_POLICIES_H_
