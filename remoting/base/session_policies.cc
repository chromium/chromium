// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_policies.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

namespace remoting {

namespace {

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::optional<T>& opt) {
  if (!opt.has_value()) {
    os << "<unspecified>";
  } else {
    os << *opt;
  }
  return os;
}

}  // namespace

SessionPolicies::SessionPolicies() = default;
SessionPolicies::~SessionPolicies() = default;

SessionPolicies::SessionPolicies(const SessionPolicies&) = default;
SessionPolicies& SessionPolicies::operator=(const SessionPolicies&) = default;
SessionPolicies::SessionPolicies(SessionPolicies&&) = default;
SessionPolicies& SessionPolicies::operator=(SessionPolicies&&) = default;

bool SessionPolicies::operator==(const SessionPolicies&) const = default;

base::expected<void, Loggable> SessionPolicies::Validate() const {
  if (!host_udp_port_range.is_valid()) {
    return base::unexpected(
        Loggable(FROM_HERE, "Invalid host UDP port range policy"));
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (maximum_session_duration.has_value() &&
      *maximum_session_duration < kMinMaximumSessionDuration) {
    return base::unexpected(Loggable(
        FROM_HERE,
        "maximum_session_duration (" +
            base::NumberToString(maximum_session_duration->InMinutes()) +
            " mins) is shorter than minimum required (" +
            base::NumberToString(kMinMaximumSessionDuration.InMinutes()) +
            " mins)"));
  }
#endif
  return base::ok();
}

std::ostream& operator<<(std::ostream& os,
                         const SessionPolicies& session_policies) {
  os << "{ clipboard_size_bytes: " << session_policies.clipboard_size_bytes
     << ", allow_stun_connections: " << session_policies.allow_stun_connections
     << ", allow_relayed_connections: "
     << session_policies.allow_relayed_connections
     << ", host_udp_port_range: " << session_policies.host_udp_port_range
     << ", allow_file_transfer: " << session_policies.allow_file_transfer
     << ", allow_uri_forwarding: " << session_policies.allow_uri_forwarding
     << ", allow_webauthn_forwarding: "
     << session_policies.allow_webauthn_forwarding
     << ", allow_gnubby_forwarding: "
     << session_policies.allow_gnubby_forwarding
     << ", maximum_session_duration: "
     << session_policies.maximum_session_duration
     << ", curtain_required: " << session_policies.curtain_required
     << ", host_username_match_required: "
     << session_policies.host_username_match_required
     << ", allow_remote_input: " << session_policies.allow_remote_input << " }";
  return os;
}

}  // namespace remoting
