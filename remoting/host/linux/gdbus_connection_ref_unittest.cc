// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdbus_connection_ref.h"

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "dbus/test_service.h"
#include "remoting/host/linux/dbus_interfaces/org_chromium_TestInterface.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace test_interface = org_chromium_TestInterface;

class GDBusConnectionRefTest : public testing::Test {
 public:
  void SetUp() override {
    // Start the test service (runs on its own thread).
    ASSERT_TRUE(test_service_.StartService());
    test_service_.WaitUntilServiceIsStarted();

    base::test::TestFuture<base::expected<GDBusConnectionRef, std::string>>
        connection;
    GDBusConnectionRef::CreateForSessionBus(connection.GetCallback());
    ASSERT_TRUE(connection.Get().has_value());
    connection_ = connection.Take().value();
  }

  void TearDown() override { test_service_.ShutdownAndBlock(); }

 protected:
  static constexpr char kObjectPath[] = "/org/chromium/TestObject";

  void PingBus() {
    base::test::TestFuture<base::expected<gvariant::Ignored, std::string>>
        response;
    connection_.Call("org.freedesktop.DBus", "/", "org.freedesktop.DBus.Peer",
                     "Ping", std::tuple(), response.GetCallback());
    EXPECT_TRUE(response.Get().has_value());
  }

  // Need a UI thread to get a GLib main loop.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  dbus::TestService test_service_{dbus::TestService::Options()};
  // Service name is randomly generated each run.
  std::string service_name_{test_service_.service_name()};
  GDBusConnectionRef connection_;
};

TEST_F(GDBusConnectionRefTest, MethodCall) {
  const char* kMessages[] = {"one", "two", "three"};
  base::test::TestFuture<base::expected<std::tuple<std::string>, std::string>>
      futures[3] = {};

  for (std::size_t i = 0; i < 3; ++i) {
    connection_.Call<test_interface::AsyncEcho>(
        service_name_.c_str(), kObjectPath, std::tuple(kMessages[i]),
        futures[i].GetCallback());
  }

  for (std::size_t i = 0; i < 3; ++i) {
    EXPECT_TRUE(futures[i].Get().has_value());
    EXPECT_EQ(kMessages[i], get<0>(futures[i].Get().value()));
  }
}

TEST_F(GDBusConnectionRefTest, MethodCallError) {
  base::test::TestFuture<base::expected<std::tuple<>, std::string>> result;
  connection_.Call<test_interface::BrokenMethod>(
      service_name_.c_str(), kObjectPath, std::tuple(), result.GetCallback());
  EXPECT_FALSE(result.Get().has_value());
}

TEST_F(GDBusConnectionRefTest, GetProperty) {
  base::test::TestFuture<base::expected<std::string, std::string>> name_value;
  base::test::TestFuture<base::expected<std::vector<std::string>, std::string>>
      methods_value;

  connection_.GetProperty<test_interface::Name>(
      service_name_.c_str(), kObjectPath, name_value.GetCallback());
  connection_.GetProperty<test_interface::Methods>(
      service_name_.c_str(), kObjectPath, methods_value.GetCallback());

  ASSERT_TRUE(name_value.Get().has_value());
  EXPECT_EQ("TestService", name_value.Get().value());

  ASSERT_TRUE(methods_value.Get().has_value());
  EXPECT_EQ((std::vector<std::string>{"Echo", "SlowEcho", "AsyncEcho",
                                      "BrokenMethod"}),
            methods_value.Get().value());
}

TEST_F(GDBusConnectionRefTest, SetProperty) {
  // TestService doesn't actually remember the set value of name, so instead one
  // must listen for the PropertiesChanged signal to make sure Set was called
  // properly.
  base::test::TestFuture<
      GVariantRef<org_freedesktop_DBus_Properties::PropertiesChanged::kType>>
      change_signal;
  auto signal_subscription =
      connection_
          .SignalSubscribe<org_freedesktop_DBus_Properties::PropertiesChanged>(
              service_name_.c_str(), kObjectPath,
              change_signal.GetRepeatingCallback());

  base::test::TestFuture<base::expected<void, std::string>> set_complete;

  const char* value = "new value";

  connection_.SetProperty<test_interface::Name>(
      service_name_.c_str(), kObjectPath, value, set_complete.GetCallback());

  EXPECT_TRUE(set_complete.Get().has_value());
  auto [interface_name, changed_properties, invalidated_properties] =
      change_signal.Take();
  EXPECT_EQ(test_interface::Name::kInterfaceName, interface_name.string_view());
  EXPECT_EQ(GVariantRef<"v">::From(gvariant::Boxed(value)),
            changed_properties.LookUp(test_interface::Name::kPropertyName));
}

TEST_F(GDBusConnectionRefTest, SignalSubscribe) {
  base::test::TestFuture<std::tuple<std::string>> signal_future;

  auto signal_subscription = connection_.SignalSubscribe<test_interface::Test>(
      service_name_.c_str(), kObjectPath, signal_future.GetRepeatingCallback());

  // Subscribing to a signal involves calling a bus method to send the match
  // rule to the bus. Unfortunately, Gio does not provide a way to inform the
  // caller of when the bus method has completed, as normally one is
  // communicating with a different process and the subscription creation only
  // has to be ordered with respect to other DBus messages (like in the
  // SetProperty test above). Here, since the test-service signal is triggered
  // directly, the test needs to wait for the match rule to be set before
  // triggering the signal. As a workaround, the test pings the bus and waits
  // for a response. Since the Ping message is sent after the match-rule message
  // we know the match-rule message has been received by the time we received
  // the ping response.
  PingBus();

  test_service_.SendTestSignal("message1");
  EXPECT_EQ("message1", get<0>(signal_future.Take()));

  test_service_.SendTestSignal("message2");
  EXPECT_EQ("message2", get<0>(signal_future.Take()));
}

TEST_F(GDBusConnectionRefTest, DropSubscription) {
  base::test::TestFuture<std::tuple<std::string>> signal_future;

  auto signal_subscription = connection_.SignalSubscribe<test_interface::Test>(
      service_name_.c_str(), kObjectPath, signal_future.GetRepeatingCallback());

  PingBus();

  test_service_.SendTestSignal("message1");
  EXPECT_EQ("message1", get<0>(signal_future.Take()));

  // Create a new subscription at root and drop original subscription.
  signal_subscription = connection_.SignalSubscribe<test_interface::Test>(
      service_name_.c_str(), "/", signal_future.GetRepeatingCallback());

  PingBus();

  // The next signal on TestObject should be ignored, but the following one on
  // the root object will match the new subscription.
  test_service_.SendTestSignal("message2");
  test_service_.SendTestSignalFromRoot("message3");
  EXPECT_EQ("message3", get<0>(signal_future.Take()));
}

TEST_F(GDBusConnectionRefTest, SubscribeAll) {
  std::string connection_name = test_service_.GetConnectionName();

  base::test::TestFuture<std::string, gvariant::ObjectPath, std::string,
                         std::string, GVariantRef<"r">>
      signal_future;

  auto signal_subscription = connection_.SignalSubscribe(
      service_name_.c_str(), nullptr, nullptr, nullptr,
      signal_future.GetRepeatingCallback());

  PingBus();

  test_service_.SendTestSignal("message1");

  {
    auto [sender, object_path, interface_name, signal_name, arguments] =
        signal_future.Take();
    EXPECT_EQ(connection_name, sender);
    EXPECT_EQ(kObjectPath, object_path.value());
    EXPECT_EQ(test_interface::Test::kInterfaceName, interface_name);
    EXPECT_EQ(test_interface::Test::kSignalName, signal_name);
    EXPECT_EQ(GVariantRef<>::From(std::tuple("message1")), arguments);
  }

  test_service_.SendTestSignalFromRoot("message2");

  {
    auto [sender, object_path, interface_name, signal_name, arguments] =
        signal_future.Take();
    EXPECT_EQ(connection_name, sender);
    EXPECT_EQ("/", object_path.value());
    EXPECT_EQ(test_interface::Test::kInterfaceName, interface_name);
    EXPECT_EQ(test_interface::Test::kSignalName, signal_name);
    EXPECT_EQ(GVariantRef<>::From(std::tuple("message2")), arguments);
  }

  const char* prop_value = "value3";
  connection_.SetProperty<test_interface::Name>(
      service_name_.c_str(), kObjectPath, prop_value, base::DoNothing());

  {
    auto [sender, object_path, interface_name, signal_name, arguments] =
        signal_future.Take();
    EXPECT_EQ(connection_name, sender);
    EXPECT_EQ(kObjectPath, object_path.value());
    EXPECT_EQ(
        org_freedesktop_DBus_Properties::PropertiesChanged::kInterfaceName,
        interface_name);
    EXPECT_EQ(org_freedesktop_DBus_Properties::PropertiesChanged::kSignalName,
              signal_name);
    auto typed_args =
        GVariantRef<org_freedesktop_DBus_Properties::PropertiesChanged::kType>::
            TryFrom(arguments);
    ASSERT_TRUE(typed_args.has_value());
    EXPECT_EQ(GVariantRef<"v">::From(gvariant::Boxed(prop_value)),
              typed_args->get<1>().LookUp(test_interface::Name::kPropertyName));
  }
}

}  // namespace remoting
