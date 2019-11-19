// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/network_connection.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

constexpr auto CONNECTION_3G = NetworkChangeNotifier::CONNECTION_3G;
constexpr auto CONNECTION_2G = NetworkChangeNotifier::CONNECTION_2G;
constexpr auto CONNECTION_ETHERNET = NetworkChangeNotifier::CONNECTION_ETHERNET;
constexpr auto CONNECTION_WIFI = NetworkChangeNotifier::CONNECTION_WIFI;

class NetworkConnectionTest : public testing::Test {
 protected:
  NetworkConnectionTest()
      : notifier_(scoped_notifier_.mock_network_change_notifier()) {}

  ScopedMockNetworkChangeNotifier scoped_notifier_;
  MockNetworkChangeNotifier* notifier_;
};

TEST_F(NetworkConnectionTest, Connection2G) {
  notifier_->SetConnectionType(CONNECTION_2G);

  NetworkConnection network_connection;
  EXPECT_EQ(CONNECTION_2G, network_connection.connection_type());
  const char* description = network_connection.connection_description();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_2G),
            description);
}

TEST_F(NetworkConnectionTest, Connection3G) {
  notifier_->SetConnectionType(CONNECTION_3G);

  NetworkConnection network_connection;
  EXPECT_EQ(CONNECTION_3G, network_connection.connection_type());
  const char* description = network_connection.connection_description();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_3G),
            description);
}

TEST_F(NetworkConnectionTest, ConnectionEthnernet) {
  notifier_->SetConnectionType(CONNECTION_ETHERNET);

  NetworkConnection network_connection;
  EXPECT_EQ(CONNECTION_ETHERNET, network_connection.connection_type());
  const char* description = network_connection.connection_description();
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_ETHERNET),
            description);
}

TEST_F(NetworkConnectionTest, ConnectionWifi) {
  notifier_->SetConnectionType(CONNECTION_WIFI);

  NetworkConnection network_connection;
  EXPECT_EQ(CONNECTION_WIFI, network_connection.connection_type());
  const char* description = network_connection.connection_description();
  // On some platforms, the description for wifi will be more detailed
  // than what is returned by NetworkChangeNotifier::ConnectionTypeToString.
  EXPECT_NE(nullptr, description);
}

TEST_F(NetworkConnectionTest, ConnectionChange) {
  base::test::TaskEnvironment task_environment;

  notifier_->SetConnectionType(CONNECTION_2G);

  NetworkConnection network_connection;
  const char* description_2g = network_connection.connection_description();

  notifier_->SetConnectionType(CONNECTION_3G);
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONNECTION_3G, network_connection.connection_type());
  const char* description_3g = network_connection.connection_description();

  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      CONNECTION_ETHERNET);
  // Spin the message loop so the notification is delivered.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONNECTION_ETHERNET, network_connection.connection_type());
  const char* description_ethernet =
      network_connection.connection_description();

  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      CONNECTION_WIFI);
  EXPECT_NE(nullptr, network_connection.connection_description());
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_2G),
            description_2g);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_3G),
            description_3g);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeToString(CONNECTION_ETHERNET),
            description_ethernet);
}

}  // namespace test
}  // namespace net
