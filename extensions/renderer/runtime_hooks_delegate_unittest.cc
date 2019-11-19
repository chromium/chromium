// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/runtime_hooks_delegate.h"

#include <memory>

#include "components/crx_file/id_util.h"
#include "content/public/common/child_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/send_message_tester.h"

namespace extensions {
namespace {

void CallAPIAndExpectError(v8::Local<v8::Context> context,
                           base::StringPiece method_name,
                           base::StringPiece args) {
  SCOPED_TRACE(base::StringPrintf("Args: `%s`", args.data()));
  constexpr char kTemplate[] = "(function() { chrome.runtime.%s(%s); })";

  v8::Isolate* isolate = context->GetIsolate();

  // Just verify some error was thrown. Expecting the exact error message
  // tends to rely too much on our argument spec code, which is tested
  // separately.
  v8::Local<v8::Function> function = FunctionFromString(
      context, base::StringPrintf(kTemplate, method_name.data(), args.data()));
  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> result =
      function->Call(context, v8::Undefined(isolate), 0, nullptr);
  EXPECT_TRUE(result.IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}

}  // namespace

class RuntimeHooksDelegateTest : public NativeExtensionBindingsSystemUnittest {
 public:
  RuntimeHooksDelegateTest() {}
  ~RuntimeHooksDelegateTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    bindings_system()->api_system()->GetHooksForAPI("runtime")->SetDelegate(
        std::make_unique<RuntimeHooksDelegate>(messaging_service_.get()));

    extension_ = BuildExtension();
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
    messaging_service_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  virtual scoped_refptr<const Extension> BuildExtension() {
    return ExtensionBuilder("foo").Build();
  }

  NativeRendererMessagingService* messaging_service() {
    return messaging_service_.get();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<NativeRendererMessagingService> messaging_service_;

  ScriptContext* script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeHooksDelegateTest);
};

TEST_F(RuntimeHooksDelegateTest, RuntimeId) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  {
    scoped_refptr<const Extension> connectable_extension =
        ExtensionBuilder("connectable")
            .SetManifestPath({"externally_connectable", "matches"},
                             ListBuilder().Append("*://example.com/*").Build())
            .Build();
    RegisterExtension(connectable_extension);
  }

  auto get_id = [](v8::Local<v8::Context> context) {
    v8::Local<v8::Function> get_id = FunctionFromString(
        context, "(function() { return chrome.runtime.id; })");
    return RunFunction(get_id, context, 0, nullptr);
  };

  {
    v8::Local<v8::Value> id = get_id(context);
    EXPECT_EQ(extension()->id(), gin::V8ToString(isolate(), id));
  }

  {
    // In order for chrome.runtime to be available to web pages, we need to have
    // an associated connectable extension, so pretend to be example.com.
    v8::Local<v8::Context> web_context = AddContext();
    ScriptContext* script_context =
        CreateScriptContext(web_context, nullptr, Feature::WEB_PAGE_CONTEXT);
    script_context->set_url(GURL("http://example.com"));
    bindings_system()->UpdateBindingsForContext(script_context);
    v8::Local<v8::Value> id = get_id(web_context);
    EXPECT_TRUE(id->IsUndefined());
  }
}

TEST_F(RuntimeHooksDelegateTest, GetManifest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  v8::Local<v8::Function> get_manifest = FunctionFromString(
      context, "(function() { return chrome.runtime.getManifest(); })");
  v8::Local<v8::Value> manifest =
      RunFunction(get_manifest, context, 0, nullptr);
  ASSERT_FALSE(manifest.IsEmpty());
  ASSERT_TRUE(manifest->IsObject());
  EXPECT_EQ(ValueToString(*extension()->manifest()->value()),
            V8ToString(manifest, context));
}

TEST_F(RuntimeHooksDelegateTest, GetURL) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto get_url = [this, context](const char* args, const GURL& expected_url) {
    SCOPED_TRACE(base::StringPrintf("Args: `%s`", args));
    constexpr char kGetUrlTemplate[] =
        "(function() { return chrome.runtime.getURL(%s); })";
    v8::Local<v8::Function> get_url =
        FunctionFromString(context, base::StringPrintf(kGetUrlTemplate, args));
    v8::Local<v8::Value> url = RunFunction(get_url, context, 0, nullptr);
    ASSERT_FALSE(url.IsEmpty());
    ASSERT_TRUE(url->IsString());
    EXPECT_EQ(expected_url.spec(), gin::V8ToString(isolate(), url));
  };

  get_url("''", extension()->url());
  get_url("'foo'", extension()->GetResourceURL("foo"));
  get_url("'/foo'", extension()->GetResourceURL("foo"));
  get_url("'https://www.google.com'",
          GURL(extension()->url().spec() + "https://www.google.com"));
}

TEST_F(RuntimeHooksDelegateTest, Connect) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  {
    // Sanity check: connectNative is unavailable (missing permission).
    v8::Local<v8::Value> connect_native =
        V8ValueFromScriptSource(context, "chrome.runtime.connectNative");
    ASSERT_FALSE(connect_native.IsEmpty());
    EXPECT_TRUE(connect_native->IsUndefined());
  }

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");
  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  tester.TestConnect("", "", self_target, false);
  tester.TestConnect("{name: 'channel'}", "channel", self_target, false);
  tester.TestConnect("{includeTlsChannelId: true}", "", self_target, true);
  tester.TestConnect("{includeTlsChannelId: true, name: 'channel'}", "channel",
                     self_target, true);

  std::string other_id = crx_file::id_util::GenerateId("other");
  MessageTarget other_target = MessageTarget::ForExtension(other_id);
  tester.TestConnect(base::StringPrintf("'%s'", other_id.c_str()), "",
                     other_target, false);
  tester.TestConnect(
      base::StringPrintf("'%s', {name: 'channel'}", other_id.c_str()),
      "channel", other_target, false);
}

// Tests the end-to-end (renderer) flow for a call to runtime.sendMessage
// from the call in JS to the expected IPCs. The intricacies of sendMessage
// are also tested more in the renderer messaging service unittests.
TEST_F(RuntimeHooksDelegateTest, SendMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  {
    // Sanity check: sendNativeMessage is unavailable (missing permission).
    v8::Local<v8::Value> send_native_message =
        V8ValueFromScriptSource(context, "chrome.runtime.sendNativeMessage");
    ASSERT_FALSE(send_native_message.IsEmpty());
    EXPECT_TRUE(send_native_message->IsUndefined());
  }

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  tester.TestSendMessage("''", R"("")", self_target, false,
                         SendMessageTester::CLOSED);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendMessage("{data: 'hello'}", kStandardMessage, self_target,
                         false, SendMessageTester::CLOSED);
  tester.TestSendMessage("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, false, SendMessageTester::OPEN);
  tester.TestSendMessage("{data: 'hello'}, {includeTlsChannelId: true}",
                         kStandardMessage, self_target, true,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage(
      "{data: 'hello'}, {includeTlsChannelId: true}, function() {}",
      kStandardMessage, self_target, true, SendMessageTester::OPEN);

  std::string other_id_str = crx_file::id_util::GenerateId("other");
  const char* other_id = other_id_str.c_str();  // For easy StringPrintf()ing.
  MessageTarget other_target = MessageTarget::ForExtension(other_id_str);

  tester.TestSendMessage(base::StringPrintf("'%s', {data: 'hello'}", other_id),
                         kStandardMessage, other_target, false,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage(
      base::StringPrintf("'%s', {data: 'hello'}, function() {}", other_id),
      kStandardMessage, other_target, false, SendMessageTester::OPEN);
  tester.TestSendMessage(base::StringPrintf("'%s', 'string message'", other_id),
                         R"("string message")", other_target, false,
                         SendMessageTester::CLOSED);

  // The sender could omit the ID by passing null or undefined explicitly.
  // Regression tests for https://crbug.com/828664.
  tester.TestSendMessage("null, {data: 'hello'}, function() {}",
                         kStandardMessage, self_target, false,
                         SendMessageTester::OPEN);
  tester.TestSendMessage("null, 'test', function() {}", R"("test")",
                         self_target, false, SendMessageTester::OPEN);
  tester.TestSendMessage("null, 'test'", R"("test")", self_target, false,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("undefined, 'test', function() {}", R"("test")",
                         self_target, false, SendMessageTester::OPEN);

  // Funny case. The only required argument is `message`, which can be any type.
  // This means that if an extension provides a <string, object> pair for the
  // first three arguments, it could apply to either the target id and the
  // message or to the message and the connect options.
  // In this case, we *always* pick the arguments as target id and message,
  // because they were the first options (connectOptions was added later), and
  // because connectOptions is pretty rarely used.
  // TODO(devlin): This is the determination JS has always made, but we could be
  // a bit more intelligent about it. We could examine the string to see if it's
  // a valid extension id as well as looking at the properties on the object.
  // But probably not worth it at this time.
  tester.TestSendMessage(
      base::StringPrintf("'%s', {includeTlsChannelId: true}", other_id),
      R"({"includeTlsChannelId":true})", other_target, false,
      SendMessageTester::CLOSED);
  tester.TestSendMessage(
      base::StringPrintf("'%s', {includeTlsChannelId: true}, function() {}",
                         other_id),
      R"({"includeTlsChannelId":true})", other_target, false,
      SendMessageTester::OPEN);
}

// Test that some incorrect invocations of sendMessage() throw errors.
TEST_F(RuntimeHooksDelegateTest, SendMessageErrors) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto send_message = [context](base::StringPiece args) {
    CallAPIAndExpectError(context, "sendMessage", args);
  };

  send_message("{data: 'hi'}, {unknownProp: true}");
  send_message("'some id', 'some message', 'some other string'");
  send_message("'some id', 'some message', {}, {}");
}

TEST_F(RuntimeHooksDelegateTest, SendMessageWithTrickyOptions) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  {
    // Even though we parse the message options separately, we do a conversion
    // of the object passed into the API. This means that something subtle like
    // this, which throws on the second access of a property, shouldn't trip us
    // up.
    constexpr char kTrickyConnectOptions[] =
        R"({data: 'hello'},
           {
             get includeTlsChannelId() {
               if (this.checkedOnce)
                 throw new Error('tricked!');
               this.checkedOnce = true;
               return true;
             }
           })";
    tester.TestSendMessage(kTrickyConnectOptions, kStandardMessage, self_target,
                           true, SendMessageTester::CLOSED);
  }
  {
    // A different form of trickiness: the options object doesn't have the
    // includeTlsChannelId key (which is acceptable, since its optional), but
    // any attempt to access the key on an object without a value for it results
    // in an error. Our argument parsing code should protect us from this.
    constexpr const char kMessWithObjectPrototype[] =
        R"((function() {
             Object.defineProperty(
                 Object.prototype, 'includeTlsChannelId',
                 { get() { throw new Error('tricked!'); } });
           }))";
    v8::Local<v8::Function> mess_with_proto =
        FunctionFromString(context, kMessWithObjectPrototype);
    RunFunction(mess_with_proto, context, 0, nullptr);
    tester.TestSendMessage("{data: 'hello'}, {}", kStandardMessage, self_target,
                           false, SendMessageTester::CLOSED);
  }
}

class RuntimeHooksDelegateNativeMessagingTest
    : public RuntimeHooksDelegateTest {
 public:
  RuntimeHooksDelegateNativeMessagingTest() {}
  ~RuntimeHooksDelegateNativeMessagingTest() override {}

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo").AddPermission("nativeMessaging").Build();
  }
};

TEST_F(RuntimeHooksDelegateNativeMessagingTest, ConnectNative) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  int next_context_port_id = 0;
  auto run_connect_native = [this, context, &next_context_port_id](
                                const std::string& args,
                                const std::string& expected_app_name) {
    // connectNative() doesn't name channels or ever include the TLS channel ID.
    const std::string kEmptyExpectedChannel;
    const bool kExpectedIncludeTlsChannelId = false;

    SCOPED_TRACE(base::StringPrintf("Args: '%s'", args.c_str()));
    constexpr char kAddPortTemplate[] =
        "(function() { return chrome.runtime.connectNative(%s); })";
    PortId expected_port_id(script_context()->context_id(),
                            next_context_port_id++, true);
    MessageTarget expected_target(
        MessageTarget::ForNativeApp(expected_app_name));
    EXPECT_CALL(*ipc_message_sender(),
                SendOpenMessageChannel(script_context(), expected_port_id,
                                       expected_target, kEmptyExpectedChannel,
                                       kExpectedIncludeTlsChannelId));

    v8::Local<v8::Function> add_port = FunctionFromString(
        context, base::StringPrintf(kAddPortTemplate, args.c_str()));
    v8::Local<v8::Value> port = RunFunction(add_port, context, 0, nullptr);
    ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
    ASSERT_FALSE(port.IsEmpty());
    ASSERT_TRUE(port->IsObject());
  };

  run_connect_native("'native_app'", "native_app");
  run_connect_native("'some_other_native_app'", "some_other_native_app");

  auto connect_native_error = [context](base::StringPiece args) {
    CallAPIAndExpectError(context, "connectNative", args);
  };
  connect_native_error("'native_app', {name: 'name'}");
}

TEST_F(RuntimeHooksDelegateNativeMessagingTest, SendNativeMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  // Whether we expect the port to be open or closed at the end of the call.
  enum PortStatus {
    CLOSED,
    OPEN,
  };

  int next_context_port_id = 0;
  auto send_native_message = [this, context, &next_context_port_id](
                                 const char* args,
                                 const std::string& expected_message,
                                 const std::string& expected_application_name,
                                 PortStatus expected_port_status) {
    // sendNativeMessage() doesn't name channels or ever include the TLS channel
    // ID.
    const std::string kEmptyExpectedChannel;
    const bool kExpectedIncludeTlsChannelId = false;

    SCOPED_TRACE(base::StringPrintf("Args: '%s'", args));
    constexpr char kSendMessageTemplate[] =
        "(function() { chrome.runtime.sendNativeMessage(%s); })";

    PortId expected_port_id(script_context()->context_id(),
                            next_context_port_id++, true);
    MessageTarget expected_target(
        MessageTarget::ForNativeApp(expected_application_name));
    EXPECT_CALL(*ipc_message_sender(),
                SendOpenMessageChannel(script_context(), expected_port_id,
                                       expected_target, kEmptyExpectedChannel,
                                       kExpectedIncludeTlsChannelId));
    Message message(expected_message, false);
    EXPECT_CALL(*ipc_message_sender(),
                SendPostMessageToPort(expected_port_id, message));
    // Note: we don't close native message ports immediately. See comment in
    // OneTimeMessageSender.
    // if (expected_port_status == CLOSED) {
    //   EXPECT_CALL(
    //       *ipc_message_sender(),
    //       SendCloseMessagePort(expected_port_id, true));
    // }
    v8::Local<v8::Function> send_message = FunctionFromString(
        context, base::StringPrintf(kSendMessageTemplate, args));
    RunFunction(send_message, context, 0, nullptr);
    ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  };

  send_native_message("'native_app', {hi:'bye'}", R"({"hi":"bye"})",
                      "native_app", CLOSED);
  send_native_message("'another_native_app', {alpha: 2}, function() {}",
                      R"({"alpha":2})", "another_native_app", OPEN);

  auto send_native_message_error = [context](base::StringPiece args) {
    CallAPIAndExpectError(context, "sendNativeMessage", args);
  };

  send_native_message_error("{data: 'hi'}, function() {}");
  send_native_message_error(
      "'native_app', 'some message', {includeTlsChannelId: true}");
}

}  // namespace extensions
