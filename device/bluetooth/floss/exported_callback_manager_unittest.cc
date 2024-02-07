// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/exported_callback_manager.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

namespace {

using testing::DoAll;

const char kExportedCallbackPath[] = "/org/chromium/some/callback";

const char kTestSender[] = ":0.1";

const int kTestSerial = 1;

void FakeExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const dbus::ExportedObject::MethodCallCallback& method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  std::move(on_exported_callback)
      .Run(interface_name, method_name, /*success=*/true);
}

class ISomeCallback {
 public:
  virtual ~ISomeCallback() = default;
  virtual base::WeakPtr<ISomeCallback> GetWeakPtr() = 0;
  virtual void OnSomethingHappened(std::string name,
                                   uint32_t number,
                                   bool status) = 0;
  virtual void SomeMethod() = 0;
};

class SomeCallback : public ISomeCallback {
 public:
  base::WeakPtr<ISomeCallback> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnSomethingHappened(std::string name,
                           uint32_t number,
                           bool status) override {
    last_something_happened = {name, number, status};
  }

  void SomeMethod() override { some_method_called = true; }

  // For test inspections.
  std::optional<std::tuple<std::string, uint32_t, bool>>
      last_something_happened;
  bool some_method_called = false;

 private:
  base::WeakPtrFactory<SomeCallback> weak_ptr_factory_{this};
};

}  // namespace

class ExportedCallbackManagerTest : public testing::Test {
 public:
  ExportedCallbackManagerTest() = default;

  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
  }

 protected:
  void TestSomethingHappenedWrongParameters(
      dbus::ExportedObject::MethodCallCallback method_handler,
      SomeCallback& some_callback) {
    dbus::MethodCall method_call("some.interface", "OnSomethingHappened");
    method_call.SetPath(dbus::ObjectPath(kExportedCallbackPath));
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("foo");
    // Wrong data type, uint32 expected.
    writer.AppendBool(true);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ(FlossDBusClient::kErrorInvalidParameters,
              saved_response->GetErrorName());

    std::string error_message;
    dbus::MessageReader reader(saved_response.get());
    reader.PopString(&error_message);
    EXPECT_EQ(
        "Cannot parse the 2th parameter, expected type signature 'u' (uint32), "
        "got 'b'",
        error_message);
  }

  void TestSomethingHappenedRightParameters(
      dbus::ExportedObject::MethodCallCallback method_handler,
      SomeCallback& some_callback) {
    dbus::MethodCall method_call("some.interface", "OnSomethingHappened");
    method_call.SetPath(dbus::ObjectPath(kExportedCallbackPath));
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("foo");
    writer.AppendUint32(10);
    writer.AppendBool(true);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    EXPECT_EQ(std::make_tuple("foo", 10, true),
              some_callback.last_something_happened);
  }

  void TestSomeMethod(dbus::ExportedObject::MethodCallCallback method_handler,
                      SomeCallback& some_callback) {
    dbus::MethodCall method_call("some.interface", "SomeMethod");
    method_call.SetPath(dbus::ObjectPath(kExportedCallbackPath));
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ("", saved_response->GetErrorName());

    ASSERT_TRUE(some_callback.some_method_called);
  }

  void TestCallbackDoesNotExist(
      dbus::ExportedObject::MethodCallCallback method_handler) {
    dbus::MethodCall method_call("some.interface", "SomeMethod");
    method_call.SetPath(dbus::ObjectPath(kExportedCallbackPath));
    method_call.SetSender(kTestSender);
    method_call.SetSerial(kTestSerial);
    dbus::MessageWriter writer(&method_call);

    std::unique_ptr<dbus::Response> saved_response;
    method_handler.Run(&method_call,
                       base::BindOnce(
                           [](std::unique_ptr<dbus::Response>* saved_response,
                              std::unique_ptr<dbus::Response> response) {
                             *saved_response = std::move(response);
                           },
                           &saved_response));

    ASSERT_TRUE(!!saved_response);
    EXPECT_EQ(FlossDBusClient::kErrorDoesNotExist,
              saved_response->GetErrorName());
  }

  scoped_refptr<dbus::MockBus> bus_;
  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<ExportedCallbackManagerTest> weak_ptr_factory_{this};
};

TEST_F(ExportedCallbackManagerTest, TestMethodHandler) {
  ExportedCallbackManager<ISomeCallback> manager("org.example.interface");
  manager.Init(bus_);

  manager.AddMethod("OnSomethingHappened", &ISomeCallback::OnSomethingHappened);
  manager.AddMethod("SomeMethod", &ISomeCallback::SomeMethod);

  scoped_refptr<::dbus::MockExportedObject> exported_callback =
      base::MakeRefCounted<::dbus::MockExportedObject>(
          bus_.get(), dbus::ObjectPath(kExportedCallbackPath));

  dbus::ExportedObject::MethodCallCallback method_handler_something_happened;
  EXPECT_CALL(*exported_callback.get(),
              ExportMethod("org.example.interface", "OnSomethingHappened",
                           testing::_, testing::_))
      .WillOnce(DoAll(testing::SaveArg<2>(&method_handler_something_happened),
                      &FakeExportMethod));

  dbus::ExportedObject::MethodCallCallback method_handler_some_method;
  EXPECT_CALL(*exported_callback.get(),
              ExportMethod("org.example.interface", "SomeMethod", testing::_,
                           testing::_))
      .WillOnce(DoAll(testing::SaveArg<2>(&method_handler_some_method),
                      &FakeExportMethod));

  auto some_callback = std::make_unique<SomeCallback>();

  EXPECT_CALL(*bus_.get(),
              GetExportedObject(dbus::ObjectPath(kExportedCallbackPath)))
      .WillRepeatedly(testing::Return(exported_callback.get()));

  manager.ExportCallback(dbus::ObjectPath(kExportedCallbackPath),
                         some_callback->GetWeakPtr(), base::DoNothing());

  ASSERT_TRUE(!!method_handler_something_happened);
  ASSERT_TRUE(!!method_handler_some_method);

  TestSomethingHappenedWrongParameters(method_handler_something_happened,
                                       *some_callback);
  TestSomethingHappenedRightParameters(method_handler_something_happened,
                                       *some_callback);
  TestSomeMethod(method_handler_some_method, *some_callback);

  some_callback.reset();
  TestCallbackDoesNotExist(method_handler_some_method);
}

}  // namespace floss
