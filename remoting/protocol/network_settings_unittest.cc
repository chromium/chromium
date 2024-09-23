// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/network_settings.h"

#include "remoting/base/port_range.h"
#include "remoting/base/session_policies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

TEST(NetworkSettings, CreateFromSessionPolicies_ImplicitNoRestrictions) {
  NetworkSettings settings(SessionPolicies{});

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_FULL);
  EXPECT_TRUE(settings.port_range.is_null());
}

TEST(NetworkSettings, CreateFromSessionPolicies_ExplicitNoRestrictions) {
  NetworkSettings settings(SessionPolicies{
      .allow_stun_connections = true,
      .allow_relayed_connections = true,
  });

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_FULL);
  EXPECT_TRUE(settings.port_range.is_null());
}

TEST(NetworkSettings, CreateFromSessionPolicies_RestrictedPortRange) {
  PortRange port_range{.min_port = 100, .max_port = 200};
  NetworkSettings settings(SessionPolicies{
      .host_udp_port_range = port_range,
  });

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_FULL);
  EXPECT_EQ(settings.port_range, port_range);
}

TEST(NetworkSettings, CreateFromSessionPolicies_RelayDisabled) {
  NetworkSettings settings(SessionPolicies{
      .allow_relayed_connections = false,
  });

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_OUTGOING |
                                NetworkSettings::NAT_TRAVERSAL_STUN);
  EXPECT_TRUE(settings.port_range.is_null());
}

TEST(NetworkSettings, CreateFromSessionPolicies_StunDisabled) {
  NetworkSettings settings(SessionPolicies{
      .allow_stun_connections = false,
  });

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_OUTGOING |
                                NetworkSettings::NAT_TRAVERSAL_RELAY);
  EXPECT_TRUE(settings.port_range.is_null());
}

TEST(NetworkSettings,
     CreateFromSessionPolicies_NatTraversalDisabled_DefaultPortRange) {
  NetworkSettings settings(SessionPolicies{
      .allow_stun_connections = false,
      .allow_relayed_connections = false,
  });

  EXPECT_EQ(settings.flags, NetworkSettings::NAT_TRAVERSAL_DISABLED);
  EXPECT_EQ(settings.port_range.min_port, NetworkSettings::kDefaultMinPort);
  EXPECT_EQ(settings.port_range.max_port, NetworkSettings::kDefaultMaxPort);
}

}  // namespace remoting::protocol
