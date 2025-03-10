// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/exported_object.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {
namespace {

class ExportedObjectTest : public testing::Test {
 protected:
  ExportedObjectTest() = default;

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

// Tests that calling a method that doesn't send a response crashes.
// TODO(crbug.com/401584852): Reenable this test.
TEST_F(ExportedObjectTest, DISABLED_NotSendingResponseCrash) {
  TestService::Options options;
  TestService test_service(options);
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));

  base::RunLoop run_loop;
  object_proxy->WaitForServiceToBeAvailable(
      base::BindLambdaForTesting([&](bool service_available) {
        ASSERT_TRUE(service_available);
        run_loop.Quit();
      }));

  ASSERT_TRUE(test_service.StartService());
  test_service.WaitUntilServiceIsStarted();
  ASSERT_TRUE(test_service.has_ownership());

  // Spin a loop and wait for `TestService` to be available.
  run_loop.Run();

  // Call the bad method and expect a CHECK crash.
  MethodCall method_call("org.chromium.TestInterface",
                         "NotSendingResponseCrash");
  base::expected<std::unique_ptr<Response>, Error> result;
  EXPECT_DEATH_IF_SUPPORTED(
      result = object_proxy->CallMethodAndBlock(
          &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT),
      "ResponseSender did not run for "
      "org.chromium.TestInterface.NotSendingResponseCrash");
}

}  // namespace
}  // namespace dbus
