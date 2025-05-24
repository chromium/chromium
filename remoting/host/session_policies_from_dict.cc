// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_policies_from_dict.h"

#include <optional>

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "remoting/base/port_range.h"
#include "remoting/base/session_policies.h"

namespace remoting {

std::optional<SessionPolicies> SessionPoliciesFromDict(
    const base::Value::Dict& dict) {
#if !BUILDFLAG(IS_CHROMEOS)
  std::optional<base::TimeDelta> maximum_session_duration;
  std::optional<int> max_session_duration_mins =
      dict.FindInt(policy::key::kRemoteAccessHostMaximumSessionDurationMinutes);
  // The default policy dict sets RemoteAccessHostMaximumSessionDurationMinutes
  // to 0, so we need to treat 0 as an unset value.
  if (max_session_duration_mins.has_value() &&
      *max_session_duration_mins != 0) {
    maximum_session_duration = base::Minutes(*max_session_duration_mins);
    if (*maximum_session_duration <
        SessionPolicies::kMinMaximumSessionDuration) {
      LOG(WARNING) << "Invalid session duration: " << *maximum_session_duration;
      return std::nullopt;
    }
  }
#endif

  PortRange host_udp_port_range;
  const std::string* udp_port_range_string =
      dict.FindString(policy::key::kRemoteAccessHostUdpPortRange);
  if (udp_port_range_string &&
      !PortRange::Parse(*udp_port_range_string, &host_udp_port_range)) {
    LOG(WARNING) << "Invalid port range: " << udp_port_range_string;
    return std::nullopt;
  }

  std::optional<bool> allow_firewall_traversal =
      dict.FindBool(policy::key::kRemoteAccessHostFirewallTraversal);

  std::optional<int> clipboard_size_bytes_value =
      dict.FindInt(policy::key::kRemoteAccessHostClipboardSizeBytes);
  std::optional<size_t> clipboard_size_bytes;
  // The default policy dict sets RemoteAccessHostClipboardSizeBytes
  // to -1, so we need to treat negative values as unset values.
  if (clipboard_size_bytes_value.has_value() &&
      *clipboard_size_bytes_value >= 0) {
    clipboard_size_bytes = *clipboard_size_bytes_value;
  }
  SessionPolicies session_policies;
  session_policies.clipboard_size_bytes = clipboard_size_bytes;
  session_policies.allow_stun_connections = allow_firewall_traversal;
  // Relayed connection is not allowed if RemoteAccessHostFirewallTraversal
  // is false. See:
  // https://chromeenterprise.google/policies/#RemoteAccessHostAllowRelayedConnection
  session_policies.allow_relayed_connections =
      allow_firewall_traversal.value_or(true)
          ? dict.FindBool(policy::key::kRemoteAccessHostAllowRelayedConnection)
          : false;
  session_policies.host_udp_port_range = host_udp_port_range;
#if !BUILDFLAG(IS_CHROMEOS)
  session_policies.allow_file_transfer =
      dict.FindBool(policy::key::kRemoteAccessHostAllowFileTransfer);
  session_policies.allow_uri_forwarding =
      dict.FindBool(policy::key::kRemoteAccessHostAllowUrlForwarding);
  session_policies.maximum_session_duration = maximum_session_duration;
  session_policies.curtain_required =
      dict.FindBool(policy::key::kRemoteAccessHostRequireCurtain);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  session_policies.host_username_match_required =
      dict.FindBool(policy::key::kRemoteAccessHostMatchUsername);
#endif
  return session_policies;
}

}  // namespace remoting
