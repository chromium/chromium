// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/one_time_message_handler.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/mock_message_port_host.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "gin/data_object_builder.h"
#include "ipc/ipc_message.h"

namespace extensions {

namespace {

constexpr char kEchoArgsAndError[] =
    "(function() {\n"
    "  this.replyArgs = Array.from(arguments);\n"
    "  this.lastError =\n"
    "      chrome.runtime.lastError ?\n"
    "      chrome.runtime.lastError.message : undefined;\n"
    "})";

}  // namespace

class OneTimeMessageHandlerTest : public NativeExtensionBindingsSystemUnittest {
 public:
  OneTimeMessageHandlerTest() {}

  OneTimeMessageHandlerTest(const OneTimeMessageHandlerTest&) = delete;
  OneTimeMessageHandlerTest& operator=(const OneTimeMessageHandlerTest&) =
      delete;

  ~OneTimeMessageHandlerTest() override {}

  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();

    extension_ = ExtensionBuilder("foo").Build();
    RegisterExtension(extension_);

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    script_context_ = CreateScriptContext(
        context, extension_.get(), mojom::ContextType::kPrivilegedExtension);
    script_context_->set_url(extension_->url());
    bindings_system()->UpdateBindingsForContext(script_context_);
  }
  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  std::string GetGlobalProperty(v8::Local<v8::Context> context,
                                std::string_view property) {
    return GetStringPropertyFromObject(context->Global(), context, property);
  }

  OneTimeMessageHandler* message_handler() {
    return &bindings_system()->messaging_service()->one_time_message_handler_;
  }

  NativeRendererMessagingService* messaging_service() {
    return bindings_system()->messaging_service();
  }

  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  raw_ptr<ScriptContext> script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

// Tests sending a message without expecting a reply, as in
// chrome.runtime.sendMessage({foo: 'bar'});
TEST_F(OneTimeMessageHandlerTest, SendMessageAndDontExpectReply) {
  const PortId port_id(script_context()->context_id(), 0, true,
                       mojom::SerializationFormat::kJson);
  const Message message("\"Hello\"", mojom::SerializationFormat::kJson, false);

  v8::HandleScope handle_scope(isolate());

  // We should open a message port, send a message, and then close it
  // immediately.
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  MockMessagePortHost mock_message_port_host;
  base::RunLoop run_loop;
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&mock_message_port_host](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        mock_message_port_host.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(mock_message_port_host, PostMessage(message));
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  messaging_service()->BindPortForTesting(
      script_context(), port_id, message_port, message_port_host_receiver);

  message_handler()->SendMessage(
      script_context(), port_id, target, mojom::ChannelType::kSendMessage,
      message, binding::AsyncResponseType::kNone, v8::Local<v8::Function>(),
      &mock_message_port_host, std::move(message_port),
      std::move(message_port_host_receiver));
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Tests sending a message and expecting a callback reply, as in
// chrome.runtime.sendMessage({foo: 'bar'}, function(reply) { ... });
TEST_F(OneTimeMessageHandlerTest, SendMessageAndExpectCallbackReply) {
  const PortId port_id(script_context()->context_id(), 0, true,
                       mojom::SerializationFormat::kJson);
  const Message message("\"Hello\"", mojom::SerializationFormat::kJson, false);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Function> callback =
      FunctionFromString(context, kEchoArgsAndError);

  APIRequestHandler* request_handler =
      bindings_system()->api_system()->request_handler();
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  // We should open a message port and send a message, and the message port
  // should remain open (to allow for a reply).
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  MockMessagePortHost mock_message_port_host;
  auto run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&mock_message_port_host](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        mock_message_port_host.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(mock_message_port_host, PostMessage(message))
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  messaging_service()->BindPortForTesting(
      script_context(), port_id, message_port, message_port_host_receiver);

  message_handler()->SendMessage(
      script_context(), port_id, target, mojom::ChannelType::kSendMessage,
      message, binding::AsyncResponseType::kCallback, callback,
      &mock_message_port_host, std::move(message_port),
      std::move(message_port_host_receiver));
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);

  // We should have added a pending request to the APIRequestHandler, but
  // shouldn't yet have triggered the reply callback.
  EXPECT_FALSE(request_handler->GetPendingRequestIdsForTesting().empty());
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "lastError"));

  run_loop = std::make_unique<base::RunLoop>();
  // Deliver the reply; the message port should close.
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));
  const Message reply("\"Hi\"", mojom::SerializationFormat::kJson, false);
  message_handler()->DeliverMessage(script_context(), reply, port_id);
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // And the callback should have been triggered, completing the request.
  EXPECT_EQ("[\"Hi\"]", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "lastError"));
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Tests sending a message and expecting a promise reply, as in
// promise = chrome.runtime.sendMessage({foo: 'bar'});
TEST_F(OneTimeMessageHandlerTest, SendMessageAndExpectPromiseReply) {
  const PortId port_id(script_context()->context_id(), 0, true,
                       mojom::SerializationFormat::kJson);
  const Message message("\"Hello\"", mojom::SerializationFormat::kJson, false);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  APIRequestHandler* request_handler =
      bindings_system()->api_system()->request_handler();
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  // We should open a message port and send a message, and the message port
  // should remain open (to allow for a reply).
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  MockMessagePortHost mock_message_port_host;
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&mock_message_port_host](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        mock_message_port_host.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(mock_message_port_host, PostMessage(message));

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  messaging_service()->BindPortForTesting(
      script_context(), port_id, message_port, message_port_host_receiver);
  v8::Local<v8::Promise> promise = message_handler()->SendMessage(
      script_context(), port_id, target, mojom::ChannelType::kSendMessage,
      message, binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      &mock_message_port_host, std::move(message_port),
      std::move(message_port_host_receiver));
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  ASSERT_FALSE(promise.IsEmpty());

  // We should have added a pending request to the APIRequestHandler, but
  // shouldn't yet have fulfilled the related promise.
  EXPECT_FALSE(request_handler->GetPendingRequestIdsForTesting().empty());
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
  EXPECT_EQ(v8::Promise::kPending, promise->State());

  base::RunLoop run_loop;
  // Deliver the reply; the message port should close.
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  const Message reply("\"Hi\"", mojom::SerializationFormat::kJson, false);
  message_handler()->DeliverMessage(script_context(), reply, port_id);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // And the callback should have been triggered, completing the request.
  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ("\"Hi\"", V8ToString(promise->Result(), context));
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Tests disconnecting an opener (initiator of a sendMessage() call) when using
// callbacks. This can happen when no receiving end exists (i.e., no listener to
// runtime.onMessage).
TEST_F(OneTimeMessageHandlerTest, DisconnectOpenerCallback) {
  const PortId port_id(script_context()->context_id(), 0, true,
                       mojom::SerializationFormat::kJson);
  const Message message("\"Hello\"", mojom::SerializationFormat::kJson, false);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Function> callback =
      FunctionFromString(context, kEchoArgsAndError);

  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_));
  MockMessagePortHost mock_message_port_host;
  EXPECT_CALL(mock_message_port_host, PostMessage(message));
  message_handler()->SendMessage(script_context(), port_id, target,
                                 mojom::ChannelType::kSendMessage, message,
                                 binding::AsyncResponseType::kCallback,
                                 callback, &mock_message_port_host, {}, {});
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);

  EXPECT_EQ("undefined", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "lastError"));

  // Disconnect the opener with an error. The callback should be triggered, and
  // the port should be removed. chrome.runtime.lastError should have been
  // populated.
  message_handler()->Disconnect(script_context(), port_id, "No receiving end");
  EXPECT_EQ("[]", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("\"No receiving end\"", GetGlobalProperty(context, "lastError"));
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Tests disconnecting an opener (initiator of a sendMessage() call) when using
// promises. This can happen when no receiving end exists (i.e., no listener to
// runtime.onMessage).
TEST_F(OneTimeMessageHandlerTest, DisconnectOpenerPromise) {
  const PortId port_id(script_context()->context_id(), 0, true,
                       mojom::SerializationFormat::kJson);
  const Message message("\"Hello\"", mojom::SerializationFormat::kJson, false);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_));
  MockMessagePortHost mock_message_port_host;

  EXPECT_CALL(mock_message_port_host, PostMessage(message));
  v8::Local<v8::Promise> promise = message_handler()->SendMessage(
      script_context(), port_id, target, mojom::ChannelType::kSendMessage,
      message, binding::AsyncResponseType::kPromise, v8::Local<v8::Function>(),
      &mock_message_port_host, {}, {});
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  // Disconnect the opener with an error. The promise should be rejected with
  // the error and the port should be removed.
  message_handler()->Disconnect(script_context(), port_id, "No receiving end");
  EXPECT_EQ(v8::Promise::kRejected, promise->State());
  ASSERT_TRUE(promise->Result()->IsNativeError());
  EXPECT_EQ("\"No receiving end\"",
            GetStringPropertyFromObject(promise->Result().As<v8::Object>(),
                                        context, "message"));
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Tests delivering a message to a receiver and not replying, as in
// chrome.runtime.onMessage.addListener(function(message, sender, reply) {
//   ...
// });
TEST_F(OneTimeMessageHandlerTest, DeliverMessageToReceiverWithNoReply) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "    this.eventMessage = message;\n"
      "    this.eventSender = sender;\n"
      "    return true;  // Reply later\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventMessage"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventSender"));

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  MockMessagePortHost mock_message_port_host;

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
  v8::Local<v8::Object> sender =
      gin::DataObjectBuilder(isolate())
          .Set("origin", std::string("https://example.com"))
          .Build();
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote, message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  mock_message_port_host.BindReceiver(std::move(message_port_host_receiver));
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventMessage"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventSender"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_message_port_host, ResponsePending())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);
  message_handler()->DeliverMessage(script_context(), message, port_id);

  EXPECT_EQ("\"Hi\"", GetGlobalProperty(context, "eventMessage"));
  EXPECT_EQ(R"({"origin":"https://example.com"})",
            GetGlobalProperty(context, "eventSender"));

  run_loop.Run();

  // TODO(devlin): Right now, the port lives eternally. In JS bindings, we have
  // two ways of dealing with this:
  // - monitoring the lifetime of the reply object
  // - requiring the extension to return true from an onMessage handler
  // We should implement these and test lifetime.
}

// Tests delivering a message to a receiver and replying, as in
// chrome.runtime.onMessage.addListener(function(message, sender, reply) {
//   reply('foo');
// });
TEST_F(OneTimeMessageHandlerTest, DeliverMessageToReceiverAndReply) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "    reply({data: 'hey'});\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);
  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  MockMessagePortHost mock_message_port_host;

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote, message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  mock_message_port_host.BindReceiver(std::move(message_port_host_receiver));
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);

  base::RunLoop run_loop;
  // When the listener replies, we should post the reply to the message port and
  // close the channel.
  EXPECT_CALL(mock_message_port_host,
              PostMessage(Message(R"({"data":"hey"})",
                                  mojom::SerializationFormat::kJson, false)));
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Tests that nothing breaks when trying to call the reply callback multiple
// times.
TEST_F(OneTimeMessageHandlerTest, TryReplyingMultipleTimes) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "    this.sendReply = reply;\n"
      "    return true;  // Reply later\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);
  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  MockMessagePortHost mock_message_port_host;

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote, message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  mock_message_port_host.BindReceiver(std::move(message_port_host_receiver));
  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);

  EXPECT_CALL(mock_message_port_host, ResponsePending());
  message_handler()->DeliverMessage(script_context(), message, port_id);

  v8::Local<v8::Value> reply =
      GetPropertyFromObject(context->Global(), context, "sendReply");
  ASSERT_FALSE(reply.IsEmpty());
  ASSERT_TRUE(reply->IsFunction());

  v8::Local<v8::Value> reply_arg = V8ValueFromScriptSource(context, "'hi'");
  v8::Local<v8::Value> args[] = {reply_arg};

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_message_port_host,
      PostMessage(Message("\"hi\"", mojom::SerializationFormat::kJson, false)));
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  RunFunction(reply.As<v8::Function>(), context, std::size(args), args);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // Running the reply function a second time shouldn't do anything.
  // TODO(devlin): Add an error message.
  RunFunction(reply.As<v8::Function>(), context, std::size(args), args);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Test starting a new sendMessage call from a sendMessage listener.
TEST_F(OneTimeMessageHandlerTest, SendMessageInListener) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "    chrome.runtime.sendMessage('foo', function() {});\n"
      "  });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  base::UnguessableToken sender_context_id = base::UnguessableToken::Create();
  const PortId original_port_id(sender_context_id, 0, false,
                                mojom::SerializationFormat::kJson);
  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  MockMessagePortHost original_mock_message_port_host;
  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiverForTesting(
      script_context(), original_port_id, sender,
      messaging_util::kOnMessageEvent, message_port_remote,
      message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  original_mock_message_port_host.BindReceiver(
      std::move(message_port_host_receiver));

  // On delivering the message, we expect the listener to open a new message
  // channel by using sendMessage(). The original message channel will be
  // closed.
  const PortId listener_created_port_id(script_context()->context_id(), 0, true,
                                        mojom::SerializationFormat::kJson);
  const Message listener_sent_message("\"foo\"",
                                      mojom::SerializationFormat::kJson, false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  MockMessagePortHost listener_mock_message_port_host;
  base::RunLoop run_loop;
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), listener_created_port_id,
                                     target, mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&listener_mock_message_port_host](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        listener_mock_message_port_host.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(listener_mock_message_port_host,
              PostMessage(listener_sent_message));
  EXPECT_CALL(original_mock_message_port_host, ClosePort(false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);
  message_handler()->DeliverMessage(script_context(), message,
                                    original_port_id);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&listener_mock_message_port_host);
  ::testing::Mock::VerifyAndClearExpectations(&original_mock_message_port_host);
}

// Test using sendMessage from the reply to a sendMessage call.
TEST_F(OneTimeMessageHandlerTest, SendMessageInCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kSendMessage[] =
      "(function() {\n"
      "  chrome.runtime.sendMessage(\n"
      "      'foo',\n"
      "      function(reply) {\n"
      "        chrome.runtime.sendMessage('bar', function() {});\n"
      "      });\n"
      "})";
  v8::Local<v8::Function> send_message =
      FunctionFromString(context, kSendMessage);

  // Running the function should send one message ('foo'), which will wait for
  // a reply.
  const PortId original_port_id(script_context()->context_id(), 0, true,
                                mojom::SerializationFormat::kJson);
  const Message original_message("\"foo\"", mojom::SerializationFormat::kJson,
                                 false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  MockMessagePortHost mock_message_port_host;
  auto run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), original_port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&mock_message_port_host](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        mock_message_port_host.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(mock_message_port_host, PostMessage(original_message))
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));
  RunFunctionOnGlobal(send_message, context, 0, nullptr);
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);

  // Upon delivering the reply to the sender, it should send a second message
  // ('bar'). The original message channel should be closed.
  const PortId new_port_id(script_context()->context_id(), 1, true,
                           mojom::SerializationFormat::kJson);
  MockMessagePortHost mock_message_port_host1;
  run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), new_port_id, target,
                                     mojom::ChannelType::kSendMessage,
                                     messaging_util::kSendMessageChannel,
                                     testing::_, testing::_))
      .WillOnce([&mock_message_port_host1](
                    ScriptContext* script_context, const PortId& port_id,
                    const MessageTarget& target,
                    mojom::ChannelType channel_type,
                    const std::string& channel_name,
                    mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                        port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
        mock_message_port_host1.BindReceiver(std::move(port_host));
      });
  EXPECT_CALL(mock_message_port_host1,
              PostMessage(Message("\"bar\"", mojom::SerializationFormat::kJson,
                                  false)));
  EXPECT_CALL(mock_message_port_host, ClosePort(true))
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));
  const Message reply("\"reply\"", mojom::SerializationFormat::kJson, false);
  message_handler()->DeliverMessage(script_context(), reply, original_port_id);
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host1);
}

TEST_F(OneTimeMessageHandlerTest, ResponseCallbackGarbageCollected) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kRegisterListener[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(\n"
      "      function(message, sender, reply) {\n"
      "        return true;  // Reply later\n"
      "      });\n"
      "})";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kRegisterListener);
  RunFunctionOnGlobal(add_listener, context, 0, nullptr);

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);
  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;
  MockMessagePortHost mock_message_port_host;

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote, message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  mock_message_port_host.BindReceiver(std::move(message_port_host_receiver));

  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);
  base::RunLoop run_loop;

  EXPECT_CALL(mock_message_port_host, ResponsePending());
  EXPECT_CALL(mock_message_port_host, ClosePort(false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
  EXPECT_EQ(
      1, message_handler()->GetPendingCallbackCountForTest(script_context()));

  // The listener didn't retain the reply callback, so it should be garbage
  // collected and the related pending callback should have been cleared.
  RunGarbageCollection();
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
  EXPECT_EQ(
      0, message_handler()->GetPendingCallbackCountForTest(script_context()));
}

// runtime.onMessage requires that a listener return `true` if they intend to
// respond to the message asynchronously. Verify that we close the port if no
// listener does so.
TEST_F(OneTimeMessageHandlerTest, ChannelClosedIfTrueNotReturned) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto register_listener = [context](const char* listener) {
    constexpr char kRegisterListenerTemplate[] =
        "(function() { chrome.runtime.onMessage.addListener(%s); })";
    v8::Local<v8::Function> add_listener = FunctionFromString(
        context, base::StringPrintf(kRegisterListenerTemplate, listener));
    RunFunctionOnGlobal(add_listener, context, 0, nullptr);
  };

  register_listener("function(message, reply, sender) { }");
  // Add a listener that returns a truthy value, but not `true`.
  register_listener("function(message, reply, sender) { return {}; }");
  // Add a listener that throws an error.
  register_listener(
      "function(message, reply, sender) { throw new Error('hi!'); }");

  base::UnguessableToken other_context_id = base::UnguessableToken::Create();
  const PortId port_id(other_context_id, 0, false,
                       mojom::SerializationFormat::kJson);

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver;

  MockMessagePortHost mock_message_port_host;
  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote, message_port_host_receiver);
  message_port_remote.EnableUnassociatedUsage();
  message_port_host_receiver.EnableUnassociatedUsage();
  mock_message_port_host.BindReceiver(std::move(message_port_host_receiver));
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  TestJSRunner::AllowErrors allow_errors;
  auto run_loop = std::make_unique<base::RunLoop>();

  // Dispatch the message. Since none of these listeners return `true`, the port
  // should close.
  const Message message("\"Hi\"", mojom::SerializationFormat::kJson, false);
  EXPECT_CALL(mock_message_port_host, ClosePort(false))
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  mojo::PendingAssociatedRemote<mojom::MessagePort> message_port_remote1;
  mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
      message_port_host_receiver1;

  // If any of the listeners return `true`, the channel should be left open.
  register_listener("function(message, reply, sender) { return true; }");
  MockMessagePortHost mock_message_port_host1;
  message_handler()->AddReceiverForTesting(
      script_context(), port_id, sender, messaging_util::kOnMessageEvent,
      message_port_remote1, message_port_host_receiver1);
  message_port_remote1.EnableUnassociatedUsage();
  message_port_host_receiver1.EnableUnassociatedUsage();
  mock_message_port_host1.BindReceiver(std::move(message_port_host_receiver1));

  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  run_loop = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_message_port_host1, ResponsePending())
      .WillOnce(base::test::RunClosure(run_loop->QuitClosure()));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
  run_loop->Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
}

}  // namespace extensions
