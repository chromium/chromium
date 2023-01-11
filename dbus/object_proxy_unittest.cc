// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_proxy.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/test_service.h"
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
    bus_ = new Bus(bus_options);
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

}  // namespace
}  // namespace dbus
