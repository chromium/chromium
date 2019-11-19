// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gin_port.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/content_features.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"

namespace extensions {

namespace {

const int kDefaultRoutingId = 42;
const char kDefaultPortName[] = "port name";

// Stub delegate for testing.
class TestPortDelegate : public GinPort::Delegate {
 public:
  TestPortDelegate() {}
  ~TestPortDelegate() override {}

  void PostMessageToPort(v8::Local<v8::Context> context,
                         const PortId& port_id,
                         int routing_id,
                         std::unique_ptr<Message> message) override {
    last_port_id_ = port_id;
    last_message_ = std::move(message);
  }
  MOCK_METHOD3(ClosePort,
               void(v8::Local<v8::Context> context,
                    const PortId&,
                    int routing_id));

  void ResetLastMessage() {
    last_port_id_.reset();
    last_message_.reset();
  }

  const base::Optional<PortId>& last_port_id() const { return last_port_id_; }
  const Message* last_message() const { return last_message_.get(); }

 private:
  base::Optional<PortId> last_port_id_;
  std::unique_ptr<Message> last_message_;

  DISALLOW_COPY_AND_ASSIGN(TestPortDelegate);
};

class GinPortTest : public APIBindingTest {
 public:
  GinPortTest() {}
  ~GinPortTest() override {}

  void SetUp() override {
    APIBindingTest::SetUp();
    auto get_context_owner = [](v8::Local<v8::Context> context) {
      return std::string();
    };
    event_handler_ = std::make_unique<APIEventHandler>(
        base::DoNothing(), base::BindRepeating(get_context_owner), nullptr);
    delegate_ = std::make_unique<testing::StrictMock<TestPortDelegate>>();
  }

  void TearDown() override {
    APIBindingTest::TearDown();
    event_handler_.reset();
  }

  void OnWillDisposeContext(v8::Local<v8::Context> context) override {
    event_handler_->InvalidateContext(context);
    binding::InvalidateContext(context);
  }

  gin::Handle<GinPort> CreatePort(v8::Local<v8::Context> context,
                                  const PortId& port_id,
                                  const char* name = kDefaultPortName) {
    EXPECT_EQ(context, context->GetIsolate()->GetCurrentContext());
    return gin::CreateHandle(
        isolate(), new GinPort(context, port_id, kDefaultRoutingId, name,
                               event_handler(), delegate()));
  }

  APIEventHandler* event_handler() { return event_handler_.get(); }
  TestPortDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<APIEventHandler> event_handler_;
  std::unique_ptr<testing::StrictMock<TestPortDelegate>> delegate_;

  DISALLOW_COPY_AND_ASSIGN(GinPortTest);
};

}  // namespace

// Tests getting the port's name.
TEST_F(GinPortTest, TestGetName) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  EXPECT_EQ(R"("port name")",
            GetStringPropertyFromObject(port_obj, context, "name"));
}

// Tests dispatching a message through the port to JS listeners.
TEST_F(GinPortTest, TestDispatchMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  const char kTestFunction[] =
      R"((function(port) {
           this.onMessagePortValid = false;
           this.messageValid = false;
           port.onMessage.addListener((message, listenerPort) => {
             this.onMessagePortValid = listenerPort === port;
             let stringifiedMessage = JSON.stringify(message);
             this.messageValid =
                 stringifiedMessage === '{"foo":42}' || stringifiedMessage;
           });
      }))";
  v8::Local<v8::Function> test_function =
      FunctionFromString(context, kTestFunction);
  v8::Local<v8::Value> args[] = {port_obj};
  RunFunctionOnGlobal(test_function, context, base::size(args), args);

  port->DispatchOnMessage(context, Message(R"({"foo":42})", false));

  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "messageValid"));
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "onMessagePortValid"));
}

// Tests posting a message from JS.
TEST_F(GinPortTest, TestPostMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  auto test_post_message = [this, port_obj, context](
                               base::StringPiece function,
                               base::Optional<PortId> expected_port_id,
                               base::Optional<Message> expected_message) {
    SCOPED_TRACE(function);
    ASSERT_EQ(!!expected_port_id, !!expected_message)
        << "Cannot expect a port id with no message";
    v8::Local<v8::Function> v8_function = FunctionFromString(context, function);
    v8::Local<v8::Value> args[] = {port_obj};

    if (expected_port_id) {
      RunFunction(v8_function, context, base::size(args), args);
      ASSERT_TRUE(delegate()->last_port_id());
      EXPECT_EQ(*expected_port_id, delegate()->last_port_id());
      ASSERT_TRUE(delegate()->last_message());
      EXPECT_EQ(expected_message->data, delegate()->last_message()->data);
      EXPECT_EQ(expected_message->user_gesture,
                delegate()->last_message()->user_gesture);
    } else {
      RunFunctionAndExpectError(v8_function, context, base::size(args), args,
                                "Uncaught Error: Could not serialize message.");
      EXPECT_FALSE(delegate()->last_port_id());
      EXPECT_FALSE(delegate()->last_message())
          << delegate()->last_message()->data;
    }
    delegate()->ResetLastMessage();
  };

  {
    // Simple message; should succeed.
    const char kFunction[] =
        "(function(port) { port.postMessage({data: [42]}); })";
    test_post_message(kFunction, port_id, Message(R"({"data":[42]})", false));

    // TODO(mustaq): We need a test with Message.user_gesture == true.
  }

  {
    // Simple non-object message; should succeed.
    const char kFunction[] = "(function(port) { port.postMessage('hello'); })";
    test_post_message(kFunction, port_id, Message(R"("hello")", false));
  }

  {
    // Undefined string (interesting because of our comparison to the JSON
    // stringify result "undefined"); should succeed.
    const char kFunction[] =
        "(function(port) { port.postMessage('undefined'); })";
    test_post_message(kFunction, port_id, Message(R"("undefined")", false));
  }

  {
    // We change undefined to null; see comment in gin_port.cc.
    const char kFunction[] =
        "(function(port) { port.postMessage(undefined); })";
    test_post_message(kFunction, port_id, Message("null", false));
  }

  {
    // Un-JSON-able object (self-referential). Should fail.
    const char kFunction[] =
        R"((function(port) {
             let message = {foo: 42};
             message.bar = message;
             port.postMessage(message);
           }))";
    test_post_message(kFunction, base::nullopt, base::nullopt);
  }

  {
    // Disconnect the port and send a message. Should fail.
    port->DispatchOnDisconnect(context);
    EXPECT_TRUE(port->is_closed_for_testing());
    const char kFunction[] =
        "(function(port) { port.postMessage({data: [42]}); })";
    v8::Local<v8::Function> function = FunctionFromString(context, kFunction);
    v8::Local<v8::Value> args[] = {port_obj};
    RunFunctionAndExpectError(
        function, context, base::size(args), args,
        "Uncaught Error: Attempting to use a disconnected port object");

    EXPECT_FALSE(delegate()->last_port_id());
    EXPECT_FALSE(delegate()->last_message());
    delegate()->ResetLastMessage();
  }
}

// Tests that calling Disconnect() notifies any listeners of the onDisconnect
// event and then closes the port.
TEST_F(GinPortTest, TestNativeDisconnect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  const char kTestFunction[] =
      R"((function(port) {
           this.onDisconnectPortValid = false;
           port.onDisconnect.addListener(listenerPort => {
             this.onDisconnectPortValid = listenerPort === port;
           });
      }))";
  v8::Local<v8::Function> test_function =
      FunctionFromString(context, kTestFunction);
  v8::Local<v8::Value> args[] = {port_obj};
  RunFunctionOnGlobal(test_function, context, base::size(args), args);

  port->DispatchOnDisconnect(context);
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "onDisconnectPortValid"));
  EXPECT_TRUE(port->is_closed_for_testing());
}

// Tests calling disconnect() from JS.
TEST_F(GinPortTest, TestJSDisconnect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  EXPECT_CALL(*delegate(), ClosePort(context, port_id, kDefaultRoutingId))
      .Times(1);
  const char kFunction[] = "(function(port) { port.disconnect(); })";
  v8::Local<v8::Function> function = FunctionFromString(context, kFunction);
  v8::Local<v8::Value> args[] = {port_obj};
  RunFunction(function, context, base::size(args), args);
  ::testing::Mock::VerifyAndClearExpectations(delegate());
  EXPECT_TRUE(port->is_closed_for_testing());
}

// Tests that a call of disconnect() from the listener of the onDisconnect event
// is rejected. Regression test for crbug.com/932347.
TEST_F(GinPortTest, JSDisconnectFromOnDisconnect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  const char kTestFunction[] =
      R"((function(port) {
           port.onDisconnect.addListener(() => {
             port.disconnect();
           });
      }))";
  v8::Local<v8::Function> test_function =
      FunctionFromString(context, kTestFunction);
  v8::Local<v8::Value> args[] = {port_obj};
  RunFunctionOnGlobal(test_function, context, base::size(args), args);

  port->DispatchOnDisconnect(context);
  EXPECT_TRUE(port->is_closed_for_testing());
}

// Tests that a call of postMessage() from the listener of the onDisconnect
// event is rejected. Regression test for crbug.com/932347.
TEST_F(GinPortTest, JSPostMessageFromOnDisconnect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  const char kTestFunction[] =
      R"((function(port) {
           port.onDisconnect.addListener(() => {
             try {
               port.postMessage({data: [42]});
             } catch (e) {
               this.lastError = e.message;
             }
           });
      }))";
  v8::Local<v8::Function> test_function =
      FunctionFromString(context, kTestFunction);
  v8::Local<v8::Value> args[] = {port_obj};
  RunFunctionOnGlobal(test_function, context, base::size(args), args);

  port->DispatchOnDisconnect(context);
  EXPECT_EQ(
      "\"Attempting to use a disconnected port object\"",
      GetStringPropertyFromObject(context->Global(), context, "lastError"));
  EXPECT_TRUE(port->is_closed_for_testing());
}

// Tests setting and getting the 'sender' property.
TEST_F(GinPortTest, TestSenderProperty) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);

  {
    gin::Handle<GinPort> port = CreatePort(context, port_id);
    v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();
    EXPECT_EQ("undefined",
              GetStringPropertyFromObject(port_obj, context, "sender"));
  }

  {
    // SetSender() can only be called before the `sender` property is accessed,
    // so we need to create a new port here.
    gin::Handle<GinPort> port = CreatePort(context, port_id);
    port->SetSender(context,
                    gin::DataObjectBuilder(isolate()).Set("prop", 42).Build());
    v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();
    EXPECT_EQ(R"({"prop":42})",
              GetStringPropertyFromObject(port_obj, context, "sender"));
  }
}

TEST_F(GinPortTest, TryUsingPortAfterInvalidation) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  constexpr char kTrySendMessage[] =
      "(function(port) { port.postMessage('hi'); })";
  v8::Local<v8::Function> send_message_function =
      FunctionFromString(context, kTrySendMessage);

  constexpr char kTryDisconnect[] = "(function(port) { port.disconnect(); })";
  v8::Local<v8::Function> disconnect_function =
      FunctionFromString(context, kTryDisconnect);

  constexpr char kTryGetOnMessage[] =
      "(function(port) { return port.onMessage; })";
  v8::Local<v8::Function> get_on_message_function =
      FunctionFromString(context, kTryGetOnMessage);

  constexpr char kTryGetOnDisconnect[] =
      "(function(port) { return port.onDisconnect; })";
  v8::Local<v8::Function> get_on_disconnect_function =
      FunctionFromString(context, kTryGetOnDisconnect);

  DisposeContext(context);

  v8::Local<v8::Value> function_args[] = {port_obj};
  for (const auto& function :
       {send_message_function, disconnect_function, get_on_message_function,
        get_on_disconnect_function}) {
    SCOPED_TRACE(gin::V8ToString(isolate(),
                                 function->ToString(context).ToLocalChecked()));
    RunFunctionAndExpectError(function, context, base::size(function_args),
                              function_args,
                              "Uncaught Error: Extension context invalidated.");
  }
}

TEST_F(GinPortTest, AlteringPortName) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  PortId port_id(base::UnguessableToken::Create(), 0, true);
  gin::Handle<GinPort> port = CreatePort(context, port_id);

  v8::Local<v8::Object> port_obj = port.ToV8().As<v8::Object>();

  v8::Local<v8::Function> change_port_name = FunctionFromString(
      context, "(function(port) { port.name = 'foo'; return port.name; })");

  v8::Local<v8::Value> args[] = {port_obj};
  v8::Local<v8::Value> result =
      RunFunction(change_port_name, context, base::size(args), args);
  EXPECT_EQ(R"("foo")", V8ToString(result, context));
}

}  // namespace extensions
