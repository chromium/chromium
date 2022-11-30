// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/dbus_statistics.h"

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

class DBusStatisticsTest : public testing::Test {
 public:
  DBusStatisticsTest() = default;

  DBusStatisticsTest(const DBusStatisticsTest&) = delete;
  DBusStatisticsTest& operator=(const DBusStatisticsTest&) = delete;

  void SetUp() override { statistics::Initialize(); }

  void TearDown() override { statistics::Shutdown(); }

 protected:
  void AddTestMethodCalls() {
    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method1");
    statistics::AddReceivedSignal(
        "service1", "service1.interface1", "method1");
    statistics::AddBlockingSentMethodCall(
        "service1", "service1.interface1", "method1");

    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method2");
    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method2");
    statistics::AddReceivedSignal(
        "service1", "service1.interface1", "method2");

    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method3");
    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method3");
    statistics::AddSentMethodCall(
        "service1", "service1.interface1", "method3");

    statistics::AddSentMethodCall(
        "service1", "service1.interface2", "method1");

    statistics::AddSentMethodCall(
        "service1", "service1.interface2", "method2");

    statistics::AddSentMethodCall(
        "service2", "service2.interface1", "method1");
  }
};

TEST_F(DBusStatisticsTest, TestDBusStatsBasic) {
  int sent = 0, received = 0, block = 0;

  // Add a sent call
  statistics::AddSentMethodCall("service1", "service1.interface1", "method1");
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(0, received);
  EXPECT_EQ(0, block);

  // Add a received call
  statistics::AddReceivedSignal("service1", "service1.interface1", "method1");
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(1, received);
  EXPECT_EQ(0, block);

  // Add a block call
  statistics::AddBlockingSentMethodCall(
      "service1", "service1.interface1", "method1");
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(1, received);
  EXPECT_EQ(1, block);
}

TEST_F(DBusStatisticsTest, TestDBusStatsMulti) {
  int sent = 0, received = 0, block = 0;

  // Add some more stats to exercise accessing multiple different stats.
  AddTestMethodCalls();

  // Make sure all entries can be found in the set and their counts were
  // incremented correctly.
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(1, received);
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method2", &sent, &received, &block));
  EXPECT_EQ(2, sent);
  EXPECT_EQ(1, received);
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface1", "method3", &sent, &received, &block));
  EXPECT_EQ(3, sent);
  EXPECT_EQ(0, received);
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface2", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(0, received);
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service1", "service1.interface2", "method2", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(0, received);
  ASSERT_TRUE(statistics::testing::GetCalls(
      "service2", "service2.interface1", "method1", &sent, &received, &block));
  EXPECT_EQ(1, sent);
  EXPECT_EQ(0, received);

  ASSERT_FALSE(statistics::testing::GetCalls(
      "service1", "service1.interface3", "method2", &sent, &received, &block));
}

TEST_F(DBusStatisticsTest, TestGetAsString) {
  std::string output_none = GetAsString(statistics::SHOW_SERVICE,
                                        statistics::FORMAT_TOTALS);
  EXPECT_EQ("No DBus calls.", output_none);

  AddTestMethodCalls();

  std::string output_service = GetAsString(statistics::SHOW_SERVICE,
                                           statistics::FORMAT_TOTALS);
  const std::string expected_output_service(
      "service1: Sent (BLOCKING): 1 Sent: 8 Received: 2\n"
      "service2: Sent: 1\n");
  EXPECT_EQ(expected_output_service, output_service);

  std::string output_interface = GetAsString(statistics::SHOW_INTERFACE,
                                             statistics::FORMAT_TOTALS);
  const std::string expected_output_interface(
      "service1.interface1: Sent (BLOCKING): 1 Sent: 6 Received: 2\n"
      "service1.interface2: Sent: 2\n"
      "service2.interface1: Sent: 1\n");
  EXPECT_EQ(expected_output_interface, output_interface);

  std::string output_per_minute = GetAsString(statistics::SHOW_INTERFACE,
                                              statistics::FORMAT_PER_MINUTE);
  const std::string expected_output_per_minute(
      "service1.interface1: Sent (BLOCKING): 1/min Sent: 6/min"
      " Received: 2/min\n"
      "service1.interface2: Sent: 2/min\n"
      "service2.interface1: Sent: 1/min\n");
  EXPECT_EQ(expected_output_per_minute, output_per_minute);

  std::string output_all = GetAsString(statistics::SHOW_INTERFACE,
                                       statistics::FORMAT_ALL);
  const std::string expected_output_all(
      "service1.interface1: Sent (BLOCKING): 1 (1/min) Sent: 6 (6/min)"
      " Received: 2 (2/min)\n"
      "service1.interface2: Sent: 2 (2/min)\n"
      "service2.interface1: Sent: 1 (1/min)\n");
  EXPECT_EQ(expected_output_all, output_all);


  std::string output_method = GetAsString(statistics::SHOW_METHOD,
                                          statistics::FORMAT_TOTALS);
  const std::string expected_output_method(
      "service1.interface1.method1: Sent (BLOCKING): 1 Sent: 1 Received: 1\n"
      "service1.interface1.method2: Sent: 2 Received: 1\n"
      "service1.interface1.method3: Sent: 3\n"
      "service1.interface2.method1: Sent: 1\n"
      "service1.interface2.method2: Sent: 1\n"
      "service2.interface1.method1: Sent: 1\n");
  EXPECT_EQ(expected_output_method, output_method);

}

}  // namespace dbus
