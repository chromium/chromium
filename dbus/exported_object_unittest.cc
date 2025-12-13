// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/exported_object.h"

#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {
namespace {

inline constexpr char kObjectPath[] = "/org/chromium/TestObject";
inline constexpr char kInterface[] = "org.chromium.TestObject";

inline constexpr char kMethodNeverRun[] = "NeverRun";
inline constexpr char kMethodNotSendingResponse[] = "NotSendingResponse";

class TestObject {
 public:
  explicit TestObject(Bus* bus) : bus_(bus) {}
  ~TestObject() = default;

  ExportedObject* ExportMethods() {
    exported_object_ = bus_->GetExportedObject(ObjectPath(kObjectPath));

    ExportOneMethod(kMethodNotSendingResponse,
                    base::BindRepeating(&TestObject::NotSendingResponse,
                                        weak_ptr_factory_.GetWeakPtr()));
    ExportOneMethod(kMethodNeverRun,
                    base::BindRepeating(&TestObject::NeverRun,
                                        weak_ptr_factory_.GetWeakPtr()));

    return exported_object_.get();
  }

 private:
  void ExportOneMethod(
      const std::string& method_name,
      const ExportedObject::MethodCallCallback& method_callback) {
    ASSERT_TRUE(exported_object_);

    base::RunLoop run_loop;
    exported_object_->ExportMethod(
        kInterface, method_name, method_callback,
        base::BindLambdaForTesting([&](const std::string& interface_name,
                                       const std::string& method_name,
                                       bool success) {
          ASSERT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void NotSendingResponse(MethodCall* method_call,
                          ExportedObject::ResponseSender response_sender) {
    // Intentionally not invoking `response_sender`. `ExportedObject` would
    // catch the case and crash.
  }

  void NeverRun(MethodCall* method_call,
                ExportedObject::ResponseSender response_sender) {
    ADD_FAILURE() << "NeverRun should never run.";
  }

  raw_ptr<Bus> bus_;
  scoped_refptr<ExportedObject> exported_object_;

  base::WeakPtrFactory<TestObject> weak_ptr_factory_{this};
};

class ExportedObjectTest : public testing::Test {
 protected:
  ExportedObjectTest() = default;

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

// Tests that calling a method that doesn't send a response crashes.
TEST_F(ExportedObjectTest, NotSendingResponseCrash) {
  const std::string service_name =
      "org.chromium.NotSendingResponse" +
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  ASSERT_TRUE(bus_->Connect());
  bus_->RequestOwnershipAndBlock(service_name,
                                 Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT);

  TestObject test_object(bus_.get());
  test_object.ExportMethods();

  // Call the bad method and expect a CHECK crash.
  auto call_bad_method = [&]() {
    ObjectProxy* object_proxy =
        bus_->GetObjectProxy(service_name, ObjectPath(kObjectPath));
    MethodCall method_call(kInterface, kMethodNotSendingResponse);
    base::RunLoop run_loop;
    object_proxy->CallMethod(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::BindLambdaForTesting(
                                 [&](Response* response) { run_loop.Quit(); }));
    run_loop.Run();
  };

  EXPECT_CHECK_DEATH_WITH(call_bad_method(),
                          "ResponseSender did not run for "
                          "org.chromium.TestObject.NotSendingResponse");
}

// Tests that an error response is sent when calling a method after a short
// lived object destruction but before its `ExportedObject` gone.
TEST_F(ExportedObjectTest, SendFailureForShortLivedObject) {
  const std::string service_name =
      "org.chromium.ShortLived" +
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  ASSERT_TRUE(bus_->Connect());
  bus_->RequestOwnershipAndBlock(service_name,
                                 Bus::REQUIRE_PRIMARY_ALLOW_REPLACEMENT);

  auto short_lived = std::make_unique<TestObject>(bus_.get());

  // Hold on to `ExportedObject` and destroy the short lived.
  scoped_refptr<ExportedObject> expored_object = short_lived->ExportMethods();
  short_lived.reset();

  // Call `NeverRun`. It should not run and an error response should be sent.
  ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name, ObjectPath(kObjectPath));
  MethodCall method_call(kInterface, kMethodNeverRun);
  base::RunLoop run_loop;
  object_proxy->CallMethodWithErrorResponse(
      &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindLambdaForTesting([&](Response* response, ErrorResponse* error) {
        ASSERT_FALSE(response);
        ASSERT_TRUE(error);
        EXPECT_EQ(DBUS_ERROR_UNKNOWN_METHOD, error->GetErrorName());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace dbus
