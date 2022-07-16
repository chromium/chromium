// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/login_settings.h"

#include <cstddef>

#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"

namespace notifier {

namespace {

const char kAuthMechanism[] = "auth_mechanism";

class LoginSettingsTest : public ::testing::Test {
 protected:
  LoginSettingsTest() {
    servers_.push_back(
        ServerInformation(
            net::HostPortPair("default.com", 100),
            DOES_NOT_SUPPORT_SSLTCP));
  }

  ServerList servers_;
};

TEST_F(LoginSettingsTest, Basic) {
  const LoginSettings login_settings(
      jingle_xmpp::XmppClientSettings(),
      jingle_glue::GetProxyResolvingSocketFactoryCallback(), servers_,
      false /* try_ssltcp_first */, kAuthMechanism,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_EQ(base::Time(), login_settings.GetRedirectExpirationForTest());
  const ServerList& servers = login_settings.GetServers();
  ASSERT_EQ(servers_.size(), servers.size());
  for (size_t i = 0; i < servers.size(); ++i) {
    EXPECT_TRUE(servers[i].Equals(servers_[i]));
  }
  EXPECT_EQ(TRAFFIC_ANNOTATION_FOR_TESTS, login_settings.traffic_annotation());
}

TEST_F(LoginSettingsTest, Redirect) {
  LoginSettings login_settings(
      jingle_xmpp::XmppClientSettings(),
      jingle_glue::GetProxyResolvingSocketFactoryCallback(), servers_,
      false /* try_ssltcp_first */, kAuthMechanism,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  const ServerInformation redirect_server(
      net::HostPortPair("redirect.com", 200),
      SUPPORTS_SSLTCP);
  login_settings.SetRedirectServer(redirect_server);

  {
    const ServerList& servers = login_settings.GetServersForTimeForTest(
        login_settings.GetRedirectExpirationForTest() - base::Milliseconds(1));
    ASSERT_EQ(servers_.size(), 1u);
    EXPECT_TRUE(servers[0].Equals(redirect_server));
  }

  {
    const ServerList& servers =
        login_settings.GetServersForTimeForTest(
            login_settings.GetRedirectExpirationForTest());
    ASSERT_EQ(servers_.size(), servers.size());
    for (size_t i = 0; i < servers.size(); ++i) {
      EXPECT_TRUE(servers[i].Equals(servers_[i]));
    }
  }
}

}  // namespace

}  // namespace notifier
