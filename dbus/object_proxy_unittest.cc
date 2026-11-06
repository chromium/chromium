// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_proxy.h"

#include <dbus/dbus.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/bus.h"
#include "dbus/test_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {
namespace {

class ObjectProxyTest : public testing::Test {
 protected:
  ObjectProxyTest() {}

  void SetUp() override {
    Bus::Options bus_options;
    bus_options.bus_type = Bus::SESSION;
    bus_options.connection_type = Bus::PRIVATE;
    bus_ = new Bus(std::move(bus_options));
  }

  void TearDown() override { bus_->ShutdownAndBlock(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  scoped_refptr<Bus> bus_;
};

// Used as a WaitForServiceToBeAvailableCallback.
void OnServiceIsAvailable(bool* dest_service_is_available,
                          int* num_calls,
                          bool src_service_is_available) {
  *dest_service_is_available = src_service_is_available;
  (*num_calls)++;
}

// Used as a callback for TestService::RequestOwnership().
void OnOwnershipRequestDone(bool success) {
  ASSERT_TRUE(success);
}

// Used as a callback for TestService::ReleaseOwnership().
void OnOwnershipReleased() {}

// Used as a Bus filter function. Records "<path>:<member>" for every
// org.chromium.TestInterface signal that reaches the connection, whether or
// not an ObjectProxy handles it. `user_data` is a std::vector<std::string>.
DBusHandlerResult RecordTestInterfaceSignal(DBusConnection* connection,
                                            DBusMessage* raw_message,
                                            void* user_data) {
  if (dbus_message_get_type(raw_message) == DBUS_MESSAGE_TYPE_SIGNAL &&
      dbus_message_has_interface(raw_message, "org.chromium.TestInterface")) {
    static_cast<std::vector<std::string>*>(user_data)->push_back(
        std::string(dbus_message_get_path(raw_message)) + ":" +
        dbus_message_get_member(raw_message));
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

TEST_F(ObjectProxyTest, WaitForServiceToBeAvailableRunOnce) {
  TestService::Options options;
  TestService test_service(options);
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));

  // The callback is not yet called because the service is not available.
  int num_calls = 0;
  bool service_is_available = false;
  object_proxy->WaitForServiceToBeAvailable(
      base::BindOnce(&OnServiceIsAvailable, &service_is_available, &num_calls));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_calls);

  // Start the service. The callback should be called asynchronously.
  ASSERT_TRUE(test_service.StartService());
  test_service.WaitUntilServiceIsStarted();
  ASSERT_TRUE(test_service.has_ownership());
  num_calls = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_calls);
  EXPECT_TRUE(service_is_available);

  // Release the service's ownership of its name. The callback should not be
  // invoked again.
  test_service.ReleaseOwnership(base::BindOnce(&OnOwnershipReleased));
  num_calls = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_calls);

  // Take ownership of the name and check that the callback is not called.
  test_service.RequestOwnership(base::BindOnce(&OnOwnershipRequestDone));
  num_calls = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_calls);
}

TEST_F(ObjectProxyTest, WaitForServiceToBeAvailableAlreadyRunning) {
  TestService::Options options;
  TestService test_service(options);
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));

  ASSERT_TRUE(test_service.StartService());
  test_service.WaitUntilServiceIsStarted();
  ASSERT_TRUE(test_service.has_ownership());

  // Since the service is already running, the callback should be invoked
  // immediately (but asynchronously, rather than the callback being invoked
  // directly within WaitForServiceToBeAvailable()).
  int num_calls = 0;
  bool service_is_available = false;
  object_proxy->WaitForServiceToBeAvailable(
      base::BindOnce(&OnServiceIsAvailable, &service_is_available, &num_calls));
  EXPECT_EQ(0, num_calls);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_calls);
  EXPECT_TRUE(service_is_available);
}

TEST_F(ObjectProxyTest, WaitForServiceToBeAvailableMultipleCallbacks) {
  TestService::Options options;
  TestService test_service(options);
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));

  // Register two callbacks.
  int num_calls_1 = 0, num_calls_2 = 0;
  bool service_is_available_1 = false, service_is_available_2 = false;
  object_proxy->WaitForServiceToBeAvailable(base::BindOnce(
      &OnServiceIsAvailable, &service_is_available_1, &num_calls_1));
  object_proxy->WaitForServiceToBeAvailable(base::BindOnce(
      &OnServiceIsAvailable, &service_is_available_2, &num_calls_2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_calls_1);
  EXPECT_EQ(0, num_calls_2);

  // Start the service and confirm that both callbacks are invoked.
  ASSERT_TRUE(test_service.StartService());
  test_service.WaitUntilServiceIsStarted();
  ASSERT_TRUE(test_service.has_ownership());
  num_calls_1 = 0;
  num_calls_2 = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_calls_1);
  EXPECT_EQ(1, num_calls_2);
  EXPECT_TRUE(service_is_available_1);
  EXPECT_TRUE(service_is_available_2);
}

// ConnectToSignal() should ask the bus only for the requested signal, so
// other signals of the same interface are not routed to the connection.
TEST_F(ObjectProxyTest, ConnectToSignalReceivesOnlyRequestedSignal) {
  TestService::Options options;
  TestService test_service(options);
  ASSERT_TRUE(test_service.StartService());
  test_service.WaitUntilServiceIsStarted();
  ASSERT_TRUE(test_service.has_ownership());

  // Connect to "Test2" (never sent) on the test object.
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));
  base::test::TestFuture<const std::string&, const std::string&, bool>
      test2_connected;
  object_proxy->ConnectToSignal("org.chromium.TestInterface", "Test2",
                                base::DoNothing(),
                                test2_connected.GetCallback());
  ASSERT_TRUE(test2_connected.Get<2>());

  // Connect to "Test" on the root object. Its delivery marks the end of the
  // test.
  ObjectProxy* root_object_proxy =
      bus_->GetObjectProxy(test_service.service_name(), ObjectPath("/"));
  base::RunLoop root_signal_loop;
  base::test::TestFuture<const std::string&, const std::string&, bool>
      root_connected;
  root_object_proxy->ConnectToSignal(
      "org.chromium.TestInterface", "Test",
      base::IgnoreArgs<Signal*>(root_signal_loop.QuitClosure()),
      root_connected.GetCallback());
  ASSERT_TRUE(root_connected.Get<2>());

  std::vector<std::string> received_signals;
  bus_->AddFilterFunction(&RecordTestInterfaceSignal, &received_signals);

  // Send "Test" from the test object, which nobody connected to, then "Test"
  // from the root object. The service sends both on one connection, so the
  // first signal would arrive before the second if the bus routed it.
  test_service.SendTestSignal("not requested");
  test_service.SendTestSignalFromRoot("requested");
  root_signal_loop.Run();

  bus_->RemoveFilterFunction(&RecordTestInterfaceSignal, &received_signals);
  EXPECT_THAT(received_signals, testing::ElementsAre("/:Test"));
}

}  // namespace
}  // namespace dbus
