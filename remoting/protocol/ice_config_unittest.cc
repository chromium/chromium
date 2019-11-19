// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/proto/remoting/v1/network_traversal_messages.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

TEST(IceConfigTest, ParseValid) {
  const char kTestConfigJson[] =
      "{"
      "  \"lifetimeDuration\": \"43200.000s\","
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"turn:8.8.8.8:19234\","
      "        \"turn:[2001:4860:4860::8888]:333\","
      "        \"turn:[2001:4860:4860::8888]\","
      "        \"turn:[2001:4860:4860::8888]:333?transport=tcp\","
      "        \"turns:the_server.com\","
      "        \"turns:the_server.com?transport=udp\""
      "      ],"
      "      \"username\": \"123\","
      "      \"credential\": \"abc\","
      "      \"maxRateKbps\": 8000.0"
      "    },"
      "    {"
      "      \"urls\": ["
      "        \"stun:stun_server.com:18344\","
      "        \"stun:1.2.3.4\""
      "      ]"
      "    }"
      "  ]"
      "}";

  IceConfig config = IceConfig::Parse(kTestConfigJson);

  // lifetimeDuration in the config is set to 12 hours. Verify that the
  // resulting expiration time is within 20 seconds before 12 hours after now.
  EXPECT_TRUE(base::Time::Now() + base::TimeDelta::FromHours(12) -
                  base::TimeDelta::FromSeconds(20) <
              config.expiration_time);
  EXPECT_TRUE(config.expiration_time <
              base::Time::Now() + base::TimeDelta::FromHours(12));

  EXPECT_EQ(6U, config.turn_servers.size());
  EXPECT_TRUE(cricket::RelayServerConfig("8.8.8.8", 19234, "123", "abc",
                                         cricket::PROTO_UDP,
                                         false) == config.turn_servers[0]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_UDP,
                                         false) == config.turn_servers[1]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 3478, "123",
                                         "abc", cricket::PROTO_UDP,
                                         false) == config.turn_servers[2]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_TCP,
                                         false) == config.turn_servers[3]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_TCP,
                                         true) == config.turn_servers[4]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_UDP,
                                         true) == config.turn_servers[5]);

  EXPECT_EQ(2U, config.stun_servers.size());
  EXPECT_EQ(rtc::SocketAddress("stun_server.com", 18344),
            config.stun_servers[0]);
  EXPECT_EQ(rtc::SocketAddress("1.2.3.4", 3478), config.stun_servers[1]);
  EXPECT_EQ(8000.0, config.max_bitrate_kbps);
}

TEST(IceConfigTest, ParseGetIceConfigResponse) {
  apis::v1::GetIceConfigResponse response;
  response.mutable_lifetime_duration()->set_seconds(43200);
  apis::v1::IceServer* turn_server = response.add_servers();
  turn_server->add_urls("turn:8.8.8.8:19234");
  turn_server->add_urls("turn:[2001:4860:4860::8888]:333");
  turn_server->add_urls("turn:[2001:4860:4860::8888]");
  turn_server->add_urls("turn:[2001:4860:4860::8888]:333?transport=tcp");
  turn_server->add_urls("turns:the_server.com");
  turn_server->add_urls("turns:the_server.com?transport=udp");
  turn_server->set_username("123");
  turn_server->set_credential("abc");
  turn_server->set_max_rate_kbps(8000);
  apis::v1::IceServer* stun_server = response.add_servers();
  stun_server->add_urls("stun:stun_server.com:18344");
  stun_server->add_urls("stun:1.2.3.4");
  IceConfig config = IceConfig::Parse(response);

  // lifetimeDuration in the config is set to 12 hours. Verify that the
  // resulting expiration time is within 20 seconds before 12 hours after now.
  EXPECT_TRUE(base::Time::Now() + base::TimeDelta::FromHours(12) -
                  base::TimeDelta::FromSeconds(20) <
              config.expiration_time);
  EXPECT_TRUE(config.expiration_time <
              base::Time::Now() + base::TimeDelta::FromHours(12));

  EXPECT_EQ(6U, config.turn_servers.size());
  EXPECT_TRUE(cricket::RelayServerConfig("8.8.8.8", 19234, "123", "abc",
                                         cricket::PROTO_UDP,
                                         false) == config.turn_servers[0]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_UDP,
                                         false) == config.turn_servers[1]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 3478, "123",
                                         "abc", cricket::PROTO_UDP,
                                         false) == config.turn_servers[2]);
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_TCP,
                                         false) == config.turn_servers[3]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_TCP,
                                         true) == config.turn_servers[4]);
  EXPECT_TRUE(cricket::RelayServerConfig("the_server.com", 5349, "123", "abc",
                                         cricket::PROTO_UDP,
                                         true) == config.turn_servers[5]);

  EXPECT_EQ(2U, config.stun_servers.size());
  EXPECT_EQ(rtc::SocketAddress("stun_server.com", 18344),
            config.stun_servers[0]);
  EXPECT_EQ(rtc::SocketAddress("1.2.3.4", 3478), config.stun_servers[1]);
  EXPECT_EQ(8000.0, config.max_bitrate_kbps);
}

TEST(IceConfigTest, ParseDataEnvelope) {
  const char kTestConfigJson[] =
      "{\"data\":{"
      "  \"lifetimeDuration\": \"43200.000s\","
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ]"
      "    }"
      "  ]"
      "}}";

  IceConfig config = IceConfig::Parse(kTestConfigJson);

  EXPECT_EQ(1U, config.stun_servers.size());
  EXPECT_EQ(rtc::SocketAddress("1.2.3.4", 3478), config.stun_servers[0]);
}

// Verify that we can still proceed if some servers cannot be parsed.
TEST(IceConfigTest, ParsePartiallyInvalid) {
  const char kTestConfigJson[] =
      "{"
      "  \"lifetimeDuration\": \"43200.000s\","
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"InvalidURL\","
      "        \"turn:[2001:4860:4860::8888]:333\""
      "      ],"
      "      \"username\": \"123\","
      "      \"credential\": \"abc\""
      "    },"
      "    \"42\""
      "  ]"
      "}";

  IceConfig config = IceConfig::Parse(kTestConfigJson);

  // Config should be already expired because it couldn't be parsed.
  EXPECT_TRUE(config.expiration_time <= base::Time::Now());

  EXPECT_EQ(1U, config.turn_servers.size());
  EXPECT_TRUE(cricket::RelayServerConfig("2001:4860:4860::8888", 333, "123",
                                         "abc", cricket::PROTO_UDP,
                                         false) == config.turn_servers[0]);
}

TEST(IceConfigTest, NotParseable) {
  IceConfig config = IceConfig::Parse("Invalid Ice Config");
  EXPECT_TRUE(config.is_null());
}

TEST(IceConfigTest, UnspecifiedMaxRate_IsZero) {
  const char kTestConfigJson[] =
      "{"
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ]"
      "    }"
      "  ]"
      "}";

  IceConfig config = IceConfig::Parse(kTestConfigJson);
  EXPECT_EQ(0, config.max_bitrate_kbps);
}

TEST(IceConfigTest, OneSpecifiedMaxRate_IsUsed) {
  const char kTestConfigJson1[] =
      "{"
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ],"
      "      \"maxRateKbps\": 1000.0"
      "    },"
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ]"
      "    }"
      "  ]"
      "}";

  IceConfig config1 = IceConfig::Parse(kTestConfigJson1);
  EXPECT_EQ(1000, config1.max_bitrate_kbps);

  const char kTestConfigJson2[] =
      "{"
      "  \"iceServers\": ["
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ]"
      "    },"
      "    {"
      "      \"urls\": ["
      "        \"stun:1.2.3.4\""
      "      ],"
      "      \"maxRateKbps\": 2000.0"
      "    }"
      "  ]"
      "}";

  IceConfig config2 = IceConfig::Parse(kTestConfigJson2);
  EXPECT_EQ(2000, config2.max_bitrate_kbps);
}

}  // namespace protocol
}  // namespace remoting
