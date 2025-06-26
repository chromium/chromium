// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/runtime_hooks_delegate.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/api/messaging/send_message_tester.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {
namespace {

void CallAPIAndExpectError(v8::Local<v8::Context> context,
                           std::string_view method_name,
                           std::string_view args) {
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

  RuntimeHooksDelegateTest(const RuntimeHooksDelegateTest&) = delete;
  RuntimeHooksDelegateTest& operator=(const RuntimeHooksDelegateTest&) = delete;

  ~RuntimeHooksDelegateTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    bindings_system()->api_system()->RegisterHooksDelegate(
        "runtime",
        std::make_unique<RuntimeHooksDelegate>(messaging_service_.get()));

    extension_ = BuildExtension();
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
    messaging_service_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  virtual scoped_refptr<const Extension> BuildExtension() {
    // TODO(https://crbug.com/40804030): Update this to use MV3.
    // SendMessageTester needs to be updated since runtime.sendMessage() now
    // returns a promise.
    return ExtensionBuilder("foo").SetManifestVersion(2).Build();
  }

  NativeRendererMessagingService* messaging_service() {
    return messaging_service_.get();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<NativeRendererMessagingService> messaging_service_;

  raw_ptr<ScriptContext> script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

TEST_F(RuntimeHooksDelegateTest, RuntimeId) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  {
    scoped_refptr<const Extension> connectable_extension =
        ExtensionBuilder("connectable")
            .SetManifestPath("externally_connectable.matches",
                             base::Value::List().Append("*://example.com/*"))
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
        CreateScriptContext(web_context, nullptr, mojom::ContextType::kWebPage);
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
  tester.TestConnect("", "", self_target);
  tester.TestConnect("{name: 'channel'}", "channel", self_target);
  tester.TestConnect("{includeTlsChannelId: true}", "", self_target);
  tester.TestConnect("{includeTlsChannelId: true, name: 'channel'}", "channel",
                     self_target);

  std::string other_id = crx_file::id_util::GenerateId("other");
  MessageTarget other_target = MessageTarget::ForExtension(other_id);
  tester.TestConnect(base::StringPrintf("'%s'", other_id.c_str()), "",
                     other_target);
  tester.TestConnect(
      base::StringPrintf("'%s', {name: 'channel'}", other_id.c_str()),
      "channel", other_target);
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
  tester.TestSendMessage("''", R"("")", self_target, SendMessageTester::CLOSED);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendMessage("{data: 'hello'}", kStandardMessage, self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, SendMessageTester::OPEN);
  tester.TestSendMessage("{data: 'hello'}, {includeTlsChannelId: true}",
                         kStandardMessage, self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage(
      "{data: 'hello'}, {includeTlsChannelId: true}, function() {}",
      kStandardMessage, self_target, SendMessageTester::OPEN);

  std::string other_id_str = crx_file::id_util::GenerateId("other");
  const char* other_id = other_id_str.c_str();  // For easy StringPrintf()ing.
  MessageTarget other_target = MessageTarget::ForExtension(other_id_str);

  tester.TestSendMessage(base::StringPrintf("'%s', {data: 'hello'}", other_id),
                         kStandardMessage, other_target,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage(
      base::StringPrintf("'%s', {data: 'hello'}, function() {}", other_id),
      kStandardMessage, other_target, SendMessageTester::OPEN);
  tester.TestSendMessage(base::StringPrintf("'%s', 'string message'", other_id),
                         R"("string message")", other_target,
                         SendMessageTester::CLOSED);

  // The sender could omit the ID by passing null or undefined explicitly.
  // Regression tests for https://crbug.com/828664.
  tester.TestSendMessage("null, {data: 'hello'}, function() {}",
                         kStandardMessage, self_target,
                         SendMessageTester::OPEN);
  tester.TestSendMessage("null, 'test', function() {}", R"("test")",
                         self_target, SendMessageTester::OPEN);
  tester.TestSendMessage("null, 'test'", R"("test")", self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendMessage("undefined, 'test', function() {}", R"("test")",
                         self_target, SendMessageTester::OPEN);

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
      R"({"includeTlsChannelId":true})", other_target,
      SendMessageTester::CLOSED);
  tester.TestSendMessage(
      base::StringPrintf("'%s', {includeTlsChannelId: true}, function() {}",
                         other_id),
      R"({"includeTlsChannelId":true})", other_target, SendMessageTester::OPEN);
}

// Test that some incorrect invocations of sendMessage() throw errors.
TEST_F(RuntimeHooksDelegateTest, SendMessageErrors) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto send_message = [context](std::string_view args) {
    CallAPIAndExpectError(context, "sendMessage", args);
  };

  send_message("{data: 'hi'}, {unknownProp: true}");
  send_message("'some id', 'some message', 'some other string'");
  send_message("'some id', 'some message', {}, {}");
}

TEST_F(RuntimeHooksDelegateTest, ConnectWithTrickyOptions) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  {
    // Even though we parse the message options separately, we do a conversion
    // of the object passed into the API. This means that something subtle like
    // this, which throws on the second access of a property, shouldn't trip us
    // up.
    constexpr char kTrickyConnectOptions[] =
        R"({
             get name() {
               if (this.checkedOnce)
                 throw new Error('tricked!');
               this.checkedOnce = true;
               return 'foo';
             }
           })";
    tester.TestConnect(kTrickyConnectOptions, "foo", self_target);
  }
  {
    // A different form of trickiness: the options object doesn't have the
    // name key (which is acceptable, since its optional), but
    // any attempt to access the key on an object without a value for it results
    // in an error. Our argument parsing code should protect us from this.
    constexpr const char kMessWithObjectPrototype[] =
        R"((function() {
             Object.defineProperty(
                 Object.prototype, 'name',
                 { get() { throw new Error('tricked!'); } });
           }))";
    v8::Local<v8::Function> mess_with_proto =
        FunctionFromString(context, kMessWithObjectPrototype);
    RunFunction(mess_with_proto, context, 0, nullptr);
    tester.TestConnect("{}", "", self_target);
  }
}

class RuntimeHooksDelegateNativeMessagingTest
    : public RuntimeHooksDelegateTest {
 public:
  RuntimeHooksDelegateNativeMessagingTest() {}
  ~RuntimeHooksDelegateNativeMessagingTest() override {}

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo").AddAPIPermission("nativeMessaging").Build();
  }
};

TEST_F(RuntimeHooksDelegateNativeMessagingTest, ConnectNative) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  int next_context_port_id = 0;
  auto run_connect_native = [this, context, &next_context_port_id](
                                const std::string& args,
                                const std::string& expected_app_name) {
    // connectNative() doesn't name channels.
    const std::string kEmptyExpectedChannel;

    SCOPED_TRACE(base::StringPrintf("Args: '%s'", args.c_str()));
    constexpr char kAddPortTemplate[] =
        "(function() { return chrome.runtime.connectNative(%s); })";
    PortId expected_port_id(script_context()->context_id(),
                            next_context_port_id++, true,
                            mojom::SerializationFormat::kJson);
    MessageTarget expected_target(
        MessageTarget::ForNativeApp(expected_app_name));
    EXPECT_CALL(
        *ipc_message_sender(),
        SendOpenMessageChannel(script_context(), expected_port_id,
                               expected_target, mojom::ChannelType::kNative,
                               kEmptyExpectedChannel, testing::_, testing::_));

    v8::Local<v8::Function> add_port = FunctionFromString(
        context, base::StringPrintf(kAddPortTemplate, args.c_str()));
    v8::Local<v8::Value> port = RunFunction(add_port, context, 0, nullptr);
    ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
    ASSERT_FALSE(port.IsEmpty());
    ASSERT_TRUE(port->IsObject());
  };

  run_connect_native("'native_app'", "native_app");
  run_connect_native("'some_other_native_app'", "some_other_native_app");

  auto connect_native_error = [context](std::string_view args) {
    CallAPIAndExpectError(context, "connectNative", args);
  };
  connect_native_error("'native_app', {name: 'name'}");
}

TEST_F(RuntimeHooksDelegateNativeMessagingTest, SendNativeMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  tester.TestSendNativeMessage("'native_app', {hi:'bye'}", R"({"hi":"bye"})",
                               "native_app");
  tester.TestSendNativeMessage(
      "'another_native_app', {alpha: 2}, function() {}", R"({"alpha":2})",
      "another_native_app");

  auto send_native_message_error = [context](std::string_view args) {
    CallAPIAndExpectError(context, "sendNativeMessage", args);
  };

  send_native_message_error("{data: 'hi'}, function() {}");
  send_native_message_error(
      "'native_app', 'some message', {includeTlsChannelId: true}");
}

class RuntimeHooksDelegateMV3Test : public RuntimeHooksDelegateTest {
 public:
  RuntimeHooksDelegateMV3Test() = default;
  ~RuntimeHooksDelegateMV3Test() override = default;

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo")
        .SetManifestKey("manifest_version", 3)
        .Build();
  }
};

TEST_F(RuntimeHooksDelegateMV3Test, SendMessageUsingPromise) {
  v8::HandleScope handle_scope(isolate());

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  // The port remains open here after the call because in MV3 we return a
  // promise if the callback parameter is omitted, so we can't use the presence/
  // lack of the callback to determine if the caller is/isn't going to handle
  // the response.
  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  tester.TestSendMessage("''", R"("")", self_target, SendMessageTester::OPEN);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  {
    // Calling sendMessage with a callback should result in no value returned.
    v8::Local<v8::Value> result = tester.TestSendMessage(
        "{data: 'hello'}, function() {}", kStandardMessage, self_target,
        SendMessageTester::OPEN);
    EXPECT_TRUE(result->IsUndefined());
  }

  {
    // Calling sendMessage without the callback should result in a promise
    // returned.
    v8::Local<v8::Value> result =
        tester.TestSendMessage("{data: 'hello'}", kStandardMessage, self_target,
                               SendMessageTester::OPEN);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());
  }
}

// Test that the asynchronous return from runtime.requestUpdateCheck differs in
// structure for callback based calls vs promise based calls.
// Note: This doesn't exercise the actual implementation of the API, just that
// the bindings modify the structure of the result.
TEST_F(RuntimeHooksDelegateMV3Test, RequestUpdateCheck) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  constexpr char kUpdateAvailableAPIResponse[] =
      R"([{"status": "update_available", "version": "2.0"}])";

  // Calling requestUpdateCheck without a callback should return a promise that
  // gets fulfilled with an object with the results as properties on it.
  {
    v8::Local<v8::Function> func = FunctionFromString(
        context,
        "(function() { return chrome.runtime.requestUpdateCheck(); })");
    v8::Local<v8::Value> result = RunFunction(func, context, 0, nullptr);
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());

    bindings_system()->HandleResponse(
        last_params().request_id,
        /*success=*/true, ListValueFromString(kUpdateAvailableAPIResponse),
        /*error=*/std::string());

    EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
    // Note that the object here differs slightly from the response, in that it
    // is not array wrapped and any keys become alphabetized.
    EXPECT_EQ(R"({"status":"update_available","version":"2.0"})",
              V8ToString(promise->Result(), context));
  }

  // Calling requestUpdateCheck with a callback should end up with the callback
  // being called with multiple parameters rather than a single object and the
  // version info as a parameter on a details object.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             chrome.runtime.requestUpdateCheck((status, details) => {
               this.argument1 = status;
               this.argument2 = details;
             });
           }))";
    v8::Local<v8::Function> func = FunctionFromString(context, kFunctionCall);
    RunFunctionOnGlobal(func, context, 0, nullptr);

    bindings_system()->HandleResponse(
        last_params().request_id,
        /*success=*/true, ListValueFromString(kUpdateAvailableAPIResponse),
        /*error=*/std::string());

    EXPECT_EQ(
        R"("update_available")",
        GetStringPropertyFromObject(context->Global(), context, "argument1"));
    EXPECT_EQ(
        R"({"version":"2.0"})",
        GetStringPropertyFromObject(context->Global(), context, "argument2"));
  }

  constexpr char kNoUpdateAPIResponse[] =
      R"([{"status": "no_update", "version": ""}])";

  // If version is specified as an empty string, it will still be sent along to
  // the callback wrapped in an object.
  {
    constexpr char kFunctionCall[] =
        R"((function(api) {
             chrome.runtime.requestUpdateCheck((status, details) => {
               this.argument1 = status;
               this.argument2 = details;
             });
           }))";
    v8::Local<v8::Function> func = FunctionFromString(context, kFunctionCall);
    RunFunctionOnGlobal(func, context, 0, nullptr);

    bindings_system()->HandleResponse(last_params().request_id,
                                      /*success=*/true,
                                      ListValueFromString(kNoUpdateAPIResponse),
                                      /*error=*/std::string());

    EXPECT_EQ(R"("no_update")", GetStringPropertyFromObject(
                                    context->Global(), context, "argument1"));
    EXPECT_EQ(
        R"({"version":""})",
        GetStringPropertyFromObject(context->Global(), context, "argument2"));
  }
}

class RuntimeHooksDelegateNativeMessagingMV3Test
    : public RuntimeHooksDelegateTest {
 public:
  RuntimeHooksDelegateNativeMessagingMV3Test() = default;
  ~RuntimeHooksDelegateNativeMessagingMV3Test() override = default;

  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder("foo")
        .SetManifestKey("manifest_version", 3)
        .AddAPIPermission("nativeMessaging")
        .Build();
  }
};

TEST_F(RuntimeHooksDelegateNativeMessagingMV3Test, SendNativeMessage) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "runtime");

  {
    // Calling sendNativeMessage without the callback should result in a promise
    // returned.
    v8::Local<v8::Value> result = tester.TestSendNativeMessage(
        "'native_app', {hi:'bye'}", R"({"hi":"bye"})", "native_app");
    v8::Local<v8::Promise> promise;
    ASSERT_TRUE(GetValueAs(result, &promise));
    EXPECT_EQ(v8::Promise::kPending, promise->State());
  }

  {
    // Calling sendNativeMessage with a callback should result in no value
    // returned.
    v8::Local<v8::Value> result = tester.TestSendNativeMessage(
        "'another_native_app', {alpha: 2}, function() {}", R"({"alpha":2})",
        "another_native_app");
    EXPECT_TRUE(result->IsUndefined());
  }

  auto send_native_message_error = [context](std::string_view args) {
    CallAPIAndExpectError(context, "sendNativeMessage", args);
  };

  // Invoking the API with incorrect parameters should emit errors.
  send_native_message_error("{data: 'hi'}, function() {}");
  send_native_message_error(
      "'native_app', 'some message', {includeTlsChannelId: true}");
}

}  // namespace extensions
