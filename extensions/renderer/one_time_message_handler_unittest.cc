// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/one_time_message_handler.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
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
  ~OneTimeMessageHandlerTest() override {}

  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    message_handler_ =
        std::make_unique<OneTimeMessageHandler>(bindings_system());

    extension_ = ExtensionBuilder("foo").Build();
    RegisterExtension(extension_);

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    script_context_ = CreateScriptContext(context, extension_.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT);
    script_context_->set_url(extension_->url());
    bindings_system()->UpdateBindingsForContext(script_context_);
  }
  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    message_handler_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  std::string GetGlobalProperty(v8::Local<v8::Context> context,
                                base::StringPiece property) {
    return GetStringPropertyFromObject(context->Global(), context, property);
  }

  OneTimeMessageHandler* message_handler() { return message_handler_.get(); }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<OneTimeMessageHandler> message_handler_;

  ScriptContext* script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(OneTimeMessageHandlerTest);
};

// Tests sending a message without expecting a reply, as in
// chrome.runtime.sendMessage({foo: 'bar'});
TEST_F(OneTimeMessageHandlerTest, SendMessageAndDontExpectReply) {
  const PortId port_id(script_context()->context_id(), 0, true);
  const bool include_tls_channel_id = false;
  const Message message("\"Hello\"", false);

  // We should open a message port, send a message, and then close it
  // immediately.
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     messaging_util::kSendMessageChannel,
                                     include_tls_channel_id));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, port_id, message));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));

  message_handler()->SendMessage(
      script_context(), port_id, target, messaging_util::kSendMessageChannel,
      include_tls_channel_id, message, v8::Local<v8::Function>());
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
}

// Tests sending a message and expecting a reply, as in
// chrome.runtime.sendMessage({foo: 'bar'}, function(reply) { ... });
TEST_F(OneTimeMessageHandlerTest, SendMessageAndExpectReply) {
  const PortId port_id(script_context()->context_id(), 0, true);
  const bool include_tls_channel_id = false;
  const Message message("\"Hello\"", false);

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
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     messaging_util::kSendMessageChannel,
                                     include_tls_channel_id));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, port_id, message));

  message_handler()->SendMessage(script_context(), port_id, target,
                                 messaging_util::kSendMessageChannel,
                                 include_tls_channel_id, message, callback);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

  // We should have added a pending request to the APIRequestHandler, but
  // shouldn't yet have triggered the reply callback.
  EXPECT_FALSE(request_handler->GetPendingRequestIdsForTesting().empty());
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "lastError"));

  // Deliver the reply; the message port should close.
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  const Message reply("\"Hi\"", false);
  message_handler()->DeliverMessage(script_context(), reply, port_id);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // And the callback should have been triggered, completing the request.
  EXPECT_EQ("[\"Hi\"]", GetGlobalProperty(context, "replyArgs"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "lastError"));
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Tests disconnecting an opener (initiator of a sendMessage() call); this can
// happen when no receiving end exists (i.e., no listener to runtime.onMessage).
TEST_F(OneTimeMessageHandlerTest, DisconnectOpener) {
  const PortId port_id(script_context()->context_id(), 0, true);
  const bool include_tls_channel_id = false;
  const Message message("\"Hello\"", false);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Function> callback =
      FunctionFromString(context, kEchoArgsAndError);

  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(*ipc_message_sender(),
              SendOpenMessageChannel(script_context(), port_id, target,
                                     messaging_util::kSendMessageChannel,
                                     include_tls_channel_id));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, port_id, message));
  message_handler()->SendMessage(script_context(), port_id, target,
                                 messaging_util::kSendMessageChannel,
                                 include_tls_channel_id, message, callback);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

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
  const PortId port_id(other_context_id, 0, false);

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
  v8::Local<v8::Object> sender = gin::DataObjectBuilder(isolate())
                                     .Set("key", std::string("sender"))
                                     .Build();
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventMessage"));
  EXPECT_EQ("undefined", GetGlobalProperty(context, "eventSender"));

  const Message message("\"Hi\"", false);
  message_handler()->DeliverMessage(script_context(), message, port_id);

  EXPECT_EQ("\"Hi\"", GetGlobalProperty(context, "eventMessage"));
  EXPECT_EQ(R"({"key":"sender"})", GetGlobalProperty(context, "eventSender"));

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
  const PortId port_id(other_context_id, 0, false);

  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  const Message message("\"Hi\"", false);

  // When the listener replies, we should post the reply to the message port and
  // close the channel.
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, port_id,
                                    Message(R"({"data":"hey"})", false)));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
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
  const PortId port_id(other_context_id, 0, false);

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  const Message message("\"Hi\"", false);

  message_handler()->DeliverMessage(script_context(), message, port_id);

  v8::Local<v8::Value> reply =
      GetPropertyFromObject(context->Global(), context, "sendReply");
  ASSERT_FALSE(reply.IsEmpty());
  ASSERT_TRUE(reply->IsFunction());

  v8::Local<v8::Value> reply_arg = V8ValueFromScriptSource(context, "'hi'");
  v8::Local<v8::Value> args[] = {reply_arg};

  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, port_id,
                                    Message("\"hi\"", false)));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, true));
  RunFunction(reply.As<v8::Function>(), context, arraysize(args), args);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // Running the reply function a second time shouldn't do anything.
  // TODO(devlin): Add an error message.
  RunFunction(reply.As<v8::Function>(), context, arraysize(args), args);
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
  const PortId original_port_id(sender_context_id, 0, false);

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiver(script_context(), original_port_id, sender,
                                 messaging_util::kOnMessageEvent);

  // On delivering the message, we expect the listener to open a new message
  // channel by using sendMessage(). The original message channel will be
  // closed.
  const PortId listener_created_port_id(script_context()->context_id(), 0,
                                        true);
  const Message listener_sent_message("\"foo\"", false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(
      *ipc_message_sender(),
      SendOpenMessageChannel(script_context(), listener_created_port_id, target,
                             messaging_util::kSendMessageChannel, false));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, listener_created_port_id,
                                    listener_sent_message));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, original_port_id, false));

  const Message message("\"Hi\"", false);
  message_handler()->DeliverMessage(script_context(), message,
                                    original_port_id);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
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
  const PortId original_port_id(script_context()->context_id(), 0, true);
  const Message original_message("\"foo\"", false);
  MessageTarget target(MessageTarget::ForExtension(extension()->id()));
  EXPECT_CALL(
      *ipc_message_sender(),
      SendOpenMessageChannel(script_context(), original_port_id, target,
                             messaging_util::kSendMessageChannel, false));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, original_port_id,
                                    original_message));
  RunFunctionOnGlobal(send_message, context, 0, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());

  // Upon delivering the reply to the sender, it should send a second message
  // ('bar'). The original message channel should be closed.
  const PortId new_port_id(script_context()->context_id(), 1, true);
  EXPECT_CALL(
      *ipc_message_sender(),
      SendOpenMessageChannel(script_context(), new_port_id, target,
                             messaging_util::kSendMessageChannel, false));
  EXPECT_CALL(*ipc_message_sender(),
              SendPostMessageToPort(MSG_ROUTING_NONE, new_port_id,
                                    Message("\"bar\"", false)));
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, original_port_id, true));
  const Message reply("\"reply\"", false);
  message_handler()->DeliverMessage(script_context(), reply, original_port_id);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
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
  const PortId port_id(other_context_id, 0, false);

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  const Message message("\"Hi\"", false);

  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, false));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  // The listener didn't retain the reply callback, so it should be garbage
  // collected.
  RunGarbageCollection();
  base::RunLoop().RunUntilIdle();

  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));
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
  const PortId port_id(other_context_id, 0, false);

  v8::Local<v8::Object> sender = v8::Object::New(isolate());
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  TestJSRunner::AllowErrors allow_errors;

  // Dispatch the message. Since none of these listeners return `true`, the port
  // should close.
  const Message message("\"Hi\"", false);
  EXPECT_CALL(*ipc_message_sender(),
              SendCloseMessagePort(MSG_ROUTING_NONE, port_id, false));
  message_handler()->DeliverMessage(script_context(), message, port_id);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(message_handler()->HasPort(script_context(), port_id));

  // If any of the listeners return `true`, the channel should be left open.
  register_listener("function(message, reply, sender) { return true; }");
  message_handler()->AddReceiver(script_context(), port_id, sender,
                                 messaging_util::kOnMessageEvent);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));

  message_handler()->DeliverMessage(script_context(), message, port_id);
  EXPECT_TRUE(message_handler()->HasPort(script_context(), port_id));
}

}  // namespace extensions
