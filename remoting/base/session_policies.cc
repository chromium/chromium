// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/session_policies.h"

#include <optional>
#include <ostream>

#include "base/notreached.h"

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

bool SessionPolicies::is_host_to_client_audio_allowed() const {
  switch (audio_mode.value_or(AudioMode::kBoth)) {
    case AudioMode::kUnspecified:
    case AudioMode::kHostToClient:
    case AudioMode::kBoth:
      return true;
    case AudioMode::kNone:
    case AudioMode::kClientToHost:
      return false;
  }
  NOTREACHED();
}

bool SessionPolicies::is_client_to_host_audio_allowed() const {
  switch (audio_mode.value_or(AudioMode::kBoth)) {
    case AudioMode::kUnspecified:
    case AudioMode::kClientToHost:
    case AudioMode::kBoth:
      return true;
    case AudioMode::kNone:
    case AudioMode::kHostToClient:
      return false;
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os,
                         SessionPolicies::AudioMode audio_mode) {
  switch (audio_mode) {
    case SessionPolicies::AudioMode::kUnspecified:
      return os << "kUnspecified";
    case SessionPolicies::AudioMode::kNone:
      return os << "kNone";
    case SessionPolicies::AudioMode::kClientToHost:
      return os << "kClientToHost";
    case SessionPolicies::AudioMode::kHostToClient:
      return os << "kHostToClient";
    case SessionPolicies::AudioMode::kBoth:
      return os << "kBoth";
  }
  NOTREACHED();
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
     << ", allow_remote_input: " << session_policies.allow_remote_input
     << ", allow_terminal_mode: " << session_policies.allow_terminal_mode
     << ", audio_mode: " << session_policies.audio_mode << " }";
  return os;
}

}  // namespace remoting
