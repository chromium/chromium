// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "dbus/error.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Unused;

namespace dbus {

class MockTest : public testing::Test {
 public:
  MockTest() = default;

  void SetUp() override {
    // Create a mock bus.
    Bus::Options options;
    options.bus_type = Bus::SYSTEM;
    mock_bus_ = new MockBus(options);

    // Create a mock proxy.
    mock_proxy_ = new MockObjectProxy(
        mock_bus_.get(),
        "org.chromium.TestService",
        ObjectPath("/org/chromium/TestObject"));

    // Set an expectation so mock_proxy's CallMethodAndBlock() will use
    // CreateMockProxyResponse() to return responses.
    EXPECT_CALL(*mock_proxy_.get(), CallMethodAndBlock(_, _))
        .WillRepeatedly(Invoke(this, &MockTest::CreateMockProxyResponse));

    // Set an expectation so mock_proxy's CallMethod() will use
    // HandleMockProxyResponseWithMessageLoop() to return responses.
    EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &MockTest::HandleMockProxyResponseWithMessageLoop));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy("org.chromium.TestService",
                               ObjectPath("/org/chromium/TestObject")))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());
  }

  void TearDown() override { mock_bus_->ShutdownAndBlock(); }

  // Called when the response is received.
  void OnResponse(Response* response) {
    // |response| will be deleted on exit of the function. Copy the
    // payload to |response_string_|.
    if (response) {
      MessageReader reader(response);
      ASSERT_TRUE(reader.PopString(&response_string_));
    }
    run_loop_->Quit();
  }

 protected:
  std::string response_string_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<MockBus> mock_bus_;
  scoped_refptr<MockObjectProxy> mock_proxy_;

 private:
  // Returns a response for the given method call. Used to implement
  // CallMethodAndBlock() for |mock_proxy_|.
  base::expected<std::unique_ptr<Response>, Error> CreateMockProxyResponse(
      MethodCall* method_call,
      int timeout_ms) {
    if (method_call->GetInterface() == "org.chromium.TestInterface" &&
        method_call->GetMember() == "Echo") {
      MessageReader reader(method_call);
      std::string text_message;
      if (reader.PopString(&text_message)) {
        std::unique_ptr<Response> response = Response::CreateEmpty();
        MessageWriter writer(response.get());
        writer.AppendString(text_message);
        return base::ok(std::move(response));
      }

      LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
      return base::unexpected(Error());
    }

    return base::unexpected(Error(DBUS_ERROR_NOT_SUPPORTED, "Not implemented"));
  }

  // Creates a response and posts the given response callback with the
  // response. Used to implement for |mock_proxy_|.
  void HandleMockProxyResponseWithMessageLoop(
      MethodCall* method_call,
      int timeout_ms,
      ObjectProxy::ResponseCallback* response_callback) {
    std::unique_ptr<Response> response =
        CreateMockProxyResponse(method_call, timeout_ms).value_or(nullptr);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockTest::RunResponseCallback, base::Unretained(this),
                       std::move(*response_callback), std::move(response)));
  }

  // Runs the given response callback with the given response.
  void RunResponseCallback(
      ObjectProxy::ResponseCallback response_callback,
      std::unique_ptr<Response> response) {
    std::move(response_callback).Run(response.get());
  }
};

// This test demonstrates how to mock a synchronous method call using the
// mock classes.
TEST_F(MockTest, CallMethodAndBlock) {
  const char kHello[] = "Hello";
  // Get an object proxy from the mock bus.
  ObjectProxy* proxy = mock_bus_->GetObjectProxy(
      "org.chromium.TestService",
      ObjectPath("/org/chromium/TestObject"));

  // Create a method call.
  MethodCall method_call("org.chromium.TestInterface", "Echo");
  MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method.
  auto result =
      proxy->CallMethodAndBlock(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT);

  // Check the response.
  ASSERT_TRUE(result.has_value());
  MessageReader reader(result->get());
  std::string text_message;
  ASSERT_TRUE(reader.PopString(&text_message));
  // The text message should be echo'ed back.
  EXPECT_EQ(kHello, text_message);
}

TEST_F(MockTest, CallMethodAndBlockOnError) {
  // Get an object proxy from the mock bus.
  ObjectProxy* proxy = mock_bus_->GetObjectProxy(
      "org.chromium.TestService",
      ObjectPath("/org/chromium/TestObject"));

  // Create a method call.
  MethodCall method_call("org.chromium.TestInterface", "MissingMethod");

  // Call the method.
  auto result =
      proxy->CallMethodAndBlock(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT);

  // Check the response.
  ASSERT_FALSE(result.has_value());
  const Error& error = result.error();
  EXPECT_EQ(DBUS_ERROR_NOT_SUPPORTED, error.name());
  EXPECT_EQ("Not implemented", error.message());
}

// This test demonstrates how to mock an asynchronous method call using the
// mock classes.
TEST_F(MockTest, CallMethod) {
  const char kHello[] = "hello";

  // Get an object proxy from the mock bus.
  ObjectProxy* proxy = mock_bus_->GetObjectProxy(
      "org.chromium.TestService",
      ObjectPath("/org/chromium/TestObject"));

  // Create a method call.
  MethodCall method_call("org.chromium.TestInterface", "Echo");
  MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method.
  run_loop_ = std::make_unique<base::RunLoop>();
  proxy->CallMethod(
      &method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&MockTest::OnResponse, base::Unretained(this)));
  // Run the message loop to let OnResponse be called.
  run_loop_->Run();

  EXPECT_EQ(kHello, response_string_);
}

}  // namespace dbus
