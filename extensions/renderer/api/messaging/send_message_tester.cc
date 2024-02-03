// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/send_message_tester.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/api/messaging/mock_message_port_host.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "ipc/ipc_message.h"
#include "v8/include/v8.h"

namespace extensions {

SendMessageTester::SendMessageTester(TestIPCMessageSender* ipc_sender,
                                     ScriptContext* script_context,
                                     int next_port_id,
                                     const std::string& api_namespace)
    : ipc_sender_(ipc_sender),
      script_context_(script_context),
      next_port_id_(next_port_id),
      api_namespace_(api_namespace) {}

SendMessageTester::~SendMessageTester() = default;

v8::Local<v8::Value> SendMessageTester::TestSendMessage(
    const std::string& args,
    const std::string& expected_message,
    const MessageTarget& expected_target,
    PortStatus expected_port_status) {
  SCOPED_TRACE(base::StringPrintf("Send Message Args: `%s`", args.c_str()));

  v8::Local<v8::Value> output;
  TestSendMessageOrRequest(args, expected_message, expected_target,
                           expected_port_status, SEND_MESSAGE, output);
  return output;
}

v8::Local<v8::Value> SendMessageTester::TestSendRequest(
    const std::string& args,
    const std::string& expected_message,
    const MessageTarget& expected_target,
    PortStatus expected_port_status) {
  SCOPED_TRACE(base::StringPrintf("Send Request Args: `%s`", args.c_str()));

  v8::Local<v8::Value> output;
  TestSendMessageOrRequest(args, expected_message, expected_target,
                           expected_port_status, SEND_REQUEST, output);
  return output;
}

v8::Local<v8::Value> SendMessageTester::TestSendNativeMessage(
    const std::string& args,
    const std::string& expected_message,
    const std::string& expected_application_name) {
  SCOPED_TRACE(
      base::StringPrintf("Send Native Message Args: `%s`", args.c_str()));

  // Note: we don't close the native message ports immediately, See comment in
  // OneTimeMessageSender.
  PortStatus expected_port_status = OPEN;
  MessageTarget expected_target(
      MessageTarget::ForNativeApp(expected_application_name));

  v8::Local<v8::Value> output;
  TestSendMessageOrRequest(args, expected_message, expected_target,
                           expected_port_status, SEND_NATIVE_MESSAGE, output);
  return output;
}

void SendMessageTester::TestConnect(const std::string& args,
                                    const std::string& expected_channel,
                                    const MessageTarget& expected_target) {
  SCOPED_TRACE(base::StringPrintf("Connect Args: `%s`", args.c_str()));

  v8::Local<v8::Context> v8_context = script_context_->v8_context();

  constexpr char kAddPortTemplate[] =
      "(function() { return chrome.%s.connect(%s); })";
  PortId expected_port_id(script_context_->context_id(), next_port_id_++, true,
                          mojom::SerializationFormat::kJson);
  EXPECT_CALL(*ipc_sender_, SendOpenMessageChannel(
                                script_context_.get(), expected_port_id,
                                expected_target, mojom::ChannelType::kConnect,
                                expected_channel, testing::_, testing::_))
      .WillOnce([](ScriptContext* script_context, const PortId& port_id,
                   const MessageTarget& target, mojom::ChannelType channel_type,
                   const std::string& channel_name,
                   mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                   mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                       port_host) {
        port.EnableUnassociatedUsage();
        port_host.EnableUnassociatedUsage();
      });
  v8::Local<v8::Function> add_port = FunctionFromString(
      v8_context, base::StringPrintf(kAddPortTemplate, api_namespace_.c_str(),
                                     args.c_str()));
  v8::Local<v8::Value> port = RunFunction(add_port, v8_context, 0, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_sender_);
  ASSERT_FALSE(port.IsEmpty());
  ASSERT_TRUE(port->IsObject());
  v8::Local<v8::Object> port_obj = port.As<v8::Object>();
  EXPECT_EQ(base::StringPrintf(R"("%s")", expected_channel.c_str()),
            GetStringPropertyFromObject(port_obj, v8_context, "name"));
}

void SendMessageTester::TestSendMessageOrRequest(
    const std::string& args,
    const std::string& expected_message,
    const MessageTarget& expected_target,
    PortStatus expected_port_status,
    Method method,
    v8::Local<v8::Value>& out_value) {
  constexpr char kSendMessageTemplate[] =
      "(function() { return chrome.%s.%s(%s); })";

  std::string expected_channel;
  mojom::ChannelType channel_type = mojom::ChannelType::kSendMessage;
  const char* method_name = nullptr;
  switch (method) {
    case SEND_MESSAGE:
      method_name = "sendMessage";
      expected_channel = messaging_util::kSendMessageChannel;
      channel_type = mojom::ChannelType::kSendMessage;
      break;
    case SEND_REQUEST:
      method_name = "sendRequest";
      expected_channel = messaging_util::kSendRequestChannel;
      channel_type = mojom::ChannelType::kSendRequest;
      break;
    case SEND_NATIVE_MESSAGE:
      method_name = "sendNativeMessage";
      channel_type = mojom::ChannelType::kNative;
      // sendNativeMessage doesn't have name channels so we don't need to change
      // expected_channel from an empty string.
      break;
  }

  MockMessagePortHost mock_message_port_host;
  PortId expected_port_id(script_context_->context_id(), next_port_id_++, true,
                          mojom::SerializationFormat::kJson);

  base::RunLoop run_loop;
  Message message(expected_message, mojom::SerializationFormat::kJson, false);
  EXPECT_CALL(*ipc_sender_,
              SendOpenMessageChannel(script_context_.get(), expected_port_id,
                                     expected_target, channel_type,
                                     expected_channel, testing::_, testing::_))
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
  if (expected_port_status == CLOSED) {
    EXPECT_CALL(mock_message_port_host, PostMessage(message));
    EXPECT_CALL(mock_message_port_host, ClosePort(true))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  } else {
    EXPECT_CALL(mock_message_port_host, PostMessage(message))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  }

  v8::Local<v8::Context> v8_context = script_context_->v8_context();
  v8::Local<v8::Function> send_message = FunctionFromString(
      v8_context,
      base::StringPrintf(kSendMessageTemplate, api_namespace_.c_str(),
                         method_name, args.c_str()));
  out_value = RunFunction(send_message, v8_context, 0, nullptr);
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(ipc_sender_);
  ::testing::Mock::VerifyAndClearExpectations(&mock_message_port_host);
}

}  // namespace extensions
