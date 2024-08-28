// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/network_settings.h"

namespace remoting::protocol {

NetworkSettings::NetworkSettings() = default;
NetworkSettings::~NetworkSettings() = default;

NetworkSettings::NetworkSettings(uint32_t flags) : flags(flags) {}

NetworkSettings::NetworkSettings(const SessionPolicies& policies) {
  port_range = policies.host_udp_port_range;
  if (policies.allow_stun_connections.value_or(true) ||
      policies.allow_relayed_connections.value_or(true)) {
    flags |= NetworkSettings::NAT_TRAVERSAL_OUTGOING;
  } else {
    if (port_range.is_null()) {
      // For legacy reasons we have to restrict the port range to a set of
      // default values when nat traversal is disabled, even if the port range
      // was not set in policy.
      port_range.min_port = kDefaultMinPort;
      port_range.max_port = kDefaultMaxPort;
    }
  }
  if (policies.allow_stun_connections.value_or(true)) {
    flags |= NetworkSettings::NAT_TRAVERSAL_STUN;
  }
  if (policies.allow_relayed_connections.value_or(true)) {
    flags |= NetworkSettings::NAT_TRAVERSAL_RELAY;
  }
}

}  // namespace remoting::protocol
