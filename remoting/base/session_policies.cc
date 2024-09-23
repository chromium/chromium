// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_policies.h"

#include <optional>

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

bool SessionPolicies::operator==(const SessionPolicies&) const = default;

std::ostream& operator<<(std::ostream& os,
                         const SessionPolicies& session_policies) {
  os << "{ clipboard_size_bytes: " << session_policies.clipboard_size_bytes
     << ", allow_stun_connections: " << session_policies.allow_stun_connections
     << ", allow_relayed_connections: "
     << session_policies.allow_relayed_connections
     << ", host_udp_port_range: " << session_policies.host_udp_port_range
     << ", allow_file_transfer: " << session_policies.allow_file_transfer
     << ", allow_uri_forwarding: " << session_policies.allow_uri_forwarding
     << ", maximum_session_duration: "
     << session_policies.maximum_session_duration
     << ", curtain_required: " << session_policies.curtain_required << " }";
  return os;
}

}  // namespace remoting
