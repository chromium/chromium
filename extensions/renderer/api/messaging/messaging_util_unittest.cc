// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/renderer/api/messaging/messaging_util.h"

#include <memory>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8.h"

namespace extensions {

using MessagingUtilTest = APIBindingTest;

TEST_F(MessagingUtilTest, TestMaximumMessageSize) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kMessageTooLongError[] =
      "Message length exceeded maximum allowed length.";

  v8::Local<v8::Value> long_message =
      V8ValueFromScriptSource(context, "'a'.repeat(1024 *1024 * 65)");
  std::string error;
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      context, long_message, mojom::SerializationFormat::kJson, &error);
  EXPECT_FALSE(message);
  EXPECT_EQ(kMessageTooLongError, error);
}

TEST_F(MessagingUtilTest, TestParseMessageOptionsFrameId) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  struct {
    int expected_frame_id;
    const char* string_options;
  } test_cases[] = {
      {messaging_util::kNoFrameId, "({})"},
      {messaging_util::kNoFrameId, "({frameId: undefined})"},
      // Note: we don't test null here, because the argument parsing code
      // ensures we would never pass undefined to ParseMessageOptions (and
      // there's a DCHECK to validate it). The null case is tested in the tabs'
      // API hooks delegate test.
      {0, "({frameId: 0})"},
      {2, "({frameId: 2})"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.string_options);
    v8::Local<v8::Value> value =
        V8ValueFromScriptSource(context, test_case.string_options);
    ASSERT_FALSE(value.IsEmpty());
    ASSERT_TRUE(value->IsObject());
    messaging_util::MessageOptions options =
        messaging_util::ParseMessageOptions(context, value.As<v8::Object>(),
                                            messaging_util::PARSE_FRAME_ID);
    EXPECT_EQ(test_case.expected_frame_id, options.frame_id);
  }
}

// Tests the result of GetEventForChannel().
TEST_F(MessagingUtilTest, TestGetEventForChannel) {
  ExtensionId id1('a', 32);
  ExtensionId id2('b', 32);

  // Exercise a bunch of possible channel endpoints -> extensions.
  // This technically isn't exhaustive, but should give us pretty reasonable
  // coverage.

  // sendRequest, Extension -> Self
  {
    EXPECT_EQ(messaging_util::kOnRequestEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForExtension(id1), id1,
                  mojom::ChannelType::kSendRequest));
  }

  // sendRequest, Extension 2 -> Extension 1
  {
    EXPECT_EQ(messaging_util::kOnRequestExternalEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForExtension(id2), id1,
                  mojom::ChannelType::kSendRequest));
  }

  // sendMessage, Extension -> Self
  {
    EXPECT_EQ(messaging_util::kOnMessageEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForExtension(id1), id1,
                  mojom::ChannelType::kSendMessage));
  }

  // sendMessage, Extension 2 -> Extension 1
  {
    EXPECT_EQ(messaging_util::kOnMessageExternalEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForExtension(id2), id1,
                  mojom::ChannelType::kSendMessage));
  }

  // sendMessage, Web Page -> Extension
  {
    EXPECT_EQ(
        messaging_util::kOnMessageExternalEvent,
        messaging_util::GetEventForChannel(MessagingEndpoint::ForWebPage(), id1,
                                           mojom::ChannelType::kSendMessage));
  }

  // sendMessage, Content Script -> Extension
  {
    EXPECT_EQ(messaging_util::kOnMessageEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForContentScript(id1), id1,
                  mojom::ChannelType::kSendMessage));
  }

  // sendMessage, User Script -> Extension
  {
    EXPECT_EQ(messaging_util::kOnUserScriptMessageEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForUserScript(id1), id1,
                  mojom::ChannelType::kSendMessage));
  }

  // connect, Extension -> Self
  {
    EXPECT_EQ(
        messaging_util::kOnConnectEvent,
        messaging_util::GetEventForChannel(MessagingEndpoint::ForExtension(id1),
                                           id1, mojom::ChannelType::kConnect));
  }

  // connect, Extension 2 -> Extension 1
  {
    EXPECT_EQ(
        messaging_util::kOnConnectExternalEvent,
        messaging_util::GetEventForChannel(MessagingEndpoint::ForExtension(id2),
                                           id1, mojom::ChannelType::kConnect));
  }

  // connect, Web Page -> Extension
  {
    EXPECT_EQ(
        messaging_util::kOnConnectExternalEvent,
        messaging_util::GetEventForChannel(MessagingEndpoint::ForWebPage(), id1,
                                           mojom::ChannelType::kConnect));
  }

  // connect, Content Script -> Extension
  {
    EXPECT_EQ(messaging_util::kOnConnectEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForContentScript(id1), id1,
                  mojom::ChannelType::kConnect));
  }

  // connect, User Script -> Extension
  {
    EXPECT_EQ(messaging_util::kOnUserScriptConnectEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForUserScript(id1), id1,
                  mojom::ChannelType::kConnect));
  }

  // connect, Native App -> Extension
  {
    EXPECT_EQ(messaging_util::kOnConnectNativeEvent,
              messaging_util::GetEventForChannel(
                  MessagingEndpoint::ForNativeApp("some app"), id1,
                  mojom::ChannelType::kNative));
  }
}

using MessagingUtilWithSystemTest = NativeExtensionBindingsSystemUnittest;

TEST_F(MessagingUtilWithSystemTest, TestGetTargetIdFromExtensionContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  std::string other_id(32, 'a');
  struct {
    v8::Local<v8::Value> passed_id;
    std::string_view expected_id;
    bool should_pass;
  } test_cases[] = {
      // If the extension ID is not provided, the bindings use the calling
      // extension's.
      {v8::Null(isolate()), extension->id(), true},
      // We treat the empty string to be the same as null, even though it's
      // somewhat unfortunate.
      // See https://crbug.com/823577.
      {gin::StringToV8(isolate(), ""), extension->id(), true},
      {gin::StringToV8(isolate(), extension->id()), extension->id(), true},
      {gin::StringToV8(isolate(), other_id), other_id, true},
      {gin::StringToV8(isolate(), "invalid id"), std::string_view(), false},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];
    std::string target;
    std::string error;
    EXPECT_EQ(test_case.should_pass,
              messaging_util::GetTargetExtensionId(
                  script_context, test_case.passed_id, "runtime.sendMessage",
                  &target, &error));
    EXPECT_EQ(test_case.expected_id, target);
    EXPECT_EQ(test_case.should_pass, error.empty()) << error;
  }
}

TEST_F(MessagingUtilWithSystemTest, TestGetTargetIdFromWebContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context =
      CreateScriptContext(context, nullptr, mojom::ContextType::kWebPage);
  script_context->set_url(GURL("https://example.com"));

  std::string other_id(32, 'a');
  struct {
    v8::Local<v8::Value> passed_id;
    std::string_view expected_id;
    bool should_pass;
  } test_cases[] = {
      // A web page should always have to specify the extension id.
      {gin::StringToV8(isolate(), other_id), other_id, true},
      {v8::Null(isolate()), std::string_view(), false},
      {gin::StringToV8(isolate(), ""), std::string_view(), false},
      {gin::StringToV8(isolate(), "invalid id"), std::string_view(), false},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];
    std::string target;
    std::string error;
    EXPECT_EQ(test_case.should_pass,
              messaging_util::GetTargetExtensionId(
                  script_context, test_case.passed_id, "runtime.sendMessage",
                  &target, &error));
    EXPECT_EQ(test_case.expected_id, target);
    EXPECT_EQ(test_case.should_pass, error.empty()) << error;
  }
}

TEST_F(MessagingUtilWithSystemTest, TestGetTargetIdFromUserScriptContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kUserScript);
  script_context->set_url(extension->url());

  std::string other_id(32, 'a');
  struct {
    v8::Local<v8::Value> passed_id;
    std::string_view expected_id;
    bool should_pass;
  } test_cases[] = {
      // If the extension ID is not provided, the bindings use the calling
      // extension's.
      {v8::Null(isolate()), extension->id(), true},
      // We treat the empty string to be the same as null, even though it's
      // somewhat unfortunate.
      // See https://crbug.com/823577.
      {gin::StringToV8(isolate(), ""), extension->id(), true},
      {gin::StringToV8(isolate(), extension->id()), extension->id(), true},
      // User scripts may not target other extensions.
      {gin::StringToV8(isolate(), other_id), std::string_view(), false},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];
    std::string target;
    std::string error;
    EXPECT_EQ(test_case.should_pass,
              messaging_util::GetTargetExtensionId(
                  script_context, test_case.passed_id, "runtime.sendMessage",
                  &target, &error));
    EXPECT_EQ(test_case.expected_id, target);
    EXPECT_EQ(test_case.should_pass, error.empty()) << error;
  }
}

}  // namespace extensions
