// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include <string_view>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_response_validator.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {

namespace {

// Returns true if the value specified by |property| exists in the given
// context.
bool PropertyExists(v8::Local<v8::Context> context, std::string_view property) {
  v8::Local<v8::Value> value = V8ValueFromScriptSource(context, property);
  EXPECT_FALSE(value.IsEmpty());
  return !value->IsUndefined();
}

}  // namespace

TEST_F(NativeExtensionBindingsSystemUnittest, Basic) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddAPIPermissions({"idle", "power", "webRequest"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // chrome.idle.queryState should exist.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> idle = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "idle");
  ASSERT_FALSE(idle.IsEmpty());
  ASSERT_TRUE(idle->IsObject());

  v8::Local<v8::Object> idle_object = v8::Local<v8::Object>::Cast(idle);
  v8::Local<v8::Value> idle_query_state =
      GetPropertyFromObject(idle_object, context, "queryState");
  ASSERT_FALSE(idle_query_state.IsEmpty());

  EXPECT_EQ(ReplaceSingleQuotes(
                "{'ACTIVE':'active','IDLE':'idle','LOCKED':'locked'}"),
            GetStringPropertyFromObject(idle_object, context, "IdleState"));

  {
    // Try calling the function with an invalid invocation - an error should be
    // thrown.
    const char kCallIdleQueryStateInvalid[] =
        "(function() {\n"
        "  chrome.idle.queryState('foo', function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kCallIdleQueryStateInvalid);
    ASSERT_FALSE(function.IsEmpty());
    RunFunctionAndExpectError(
        function, context, 0, nullptr,
        "Uncaught TypeError: " +
            api_errors::InvocationError(
                "idle.queryState",
                "integer detectionIntervalInSeconds, function callback",
                api_errors::NoMatchingSignature()));
  }

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_EQ(last_params().arguments, ListValueFromString("[30]"));

  // Respond and validate.
  bindings_system()->HandleResponse(last_params().request_id, true,
                                    ListValueFromString("['active']"),
                                    std::string());

  std::unique_ptr<base::Value> result_value = GetBaseValuePropertyFromObject(
      context->Global(), context, "responseState");
  ASSERT_TRUE(result_value);
  EXPECT_EQ("\"active\"", ValueToString(*result_value));

  // Sanity-check that another API also exists as expected.
  v8::Local<v8::Value> power_api =
      V8ValueFromScriptSource(context, "chrome.power");
  ASSERT_FALSE(power_api.IsEmpty());
  ASSERT_TRUE(power_api->IsObject());
  v8::Local<v8::Value> request_keep_awake = GetPropertyFromObject(
      power_api.As<v8::Object>(), context, "requestKeepAwake");
  ASSERT_FALSE(request_keep_awake.IsEmpty());
  EXPECT_TRUE(request_keep_awake->IsFunction());

  // Test properties exposed on the API object itself.
  v8::Local<v8::Value> web_request =
      V8ValueFromScriptSource(context, "chrome.webRequest");
  ASSERT_FALSE(web_request.IsEmpty());
  ASSERT_TRUE(web_request->IsObject());
  EXPECT_EQ("20", GetStringPropertyFromObject(
                      web_request.As<v8::Object>(), context,
                      "MAX_HANDLER_BEHAVIOR_CHANGED_CALLS_PER_10_MINUTES"));
}

TEST_F(NativeExtensionBindingsSystemUnittest, Events) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermissions({"idle", "power"}).Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    const char kAddStateChangedListeners[] =
        "(function() {\n"
        "  chrome.idle.onStateChanged.addListener(function() {\n"
        "    this.didThrow = true;\n"
        "    throw new Error('Error!!!');\n"
        "  });\n"
        "  chrome.idle.onStateChanged.addListener(function(newState) {\n"
        "    this.newState = newState;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> add_listeners =
        FunctionFromString(context, kAddStateChangedListeners);
    RunFunctionOnGlobal(add_listeners, context, 0, nullptr);
  }

  {
    TestJSRunner::AllowErrors allow_errors;
    base::Value::List value = ListValueFromString("['idle']");
    bindings_system()->DispatchEventInContext("idle.onStateChanged", value,
                                              nullptr, script_context);
  }

  EXPECT_EQ("\"idle\"", GetStringPropertyFromObject(context->Global(), context,
                                                    "newState"));
  EXPECT_EQ("true", GetStringPropertyFromObject(context->Global(), context,
                                                "didThrow"));
}

// Tests that referencing the same API multiple times returns the same object;
// i.e. chrome.foo === chrome.foo.
TEST_F(NativeExtensionBindingsSystemUnittest, APIObjectsAreEqual) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());
  EXPECT_FALSE(first_idle_object->IsUndefined());
  v8::Local<v8::Value> second_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  EXPECT_TRUE(first_idle_object == second_idle_object);
}

// Tests that referencing APIs after the context data is disposed is safe (and
// returns undefined if not yet instantiated).
TEST_F(NativeExtensionBindingsSystemUnittest,
       ReferencingAPIAfterDisposingContext) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermissions({"idle", "power"}).Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());

  DisposeContext(context);
  {
    // Despite disposal, the context has been kept alive via the Local above.
    v8::Context::Scope context_scope(context);

    // Check an API that was instantiated....
    v8::Local<v8::Value> second_idle_object =
        V8ValueFromScriptSource(context, "chrome.idle");
    EXPECT_EQ(first_idle_object, second_idle_object);
    // ... and also one that wasn't.
    v8::Local<v8::Value> power_object =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power_object.IsEmpty());
    EXPECT_TRUE(power_object->IsUndefined());
  }
}

// Tests that traditional custom bindings can be used with the native bindings
// system.
TEST_F(NativeExtensionBindingsSystemUnittest, TestBridgingToJSCustomBindings) {
  // Custom binding code. This basically utilizes the interface in binding.js in
  // order to test backwards compatibility.
  const char kCustomBinding[] =
      "apiBridge.registerCustomHook((api, extensionId, contextType) => {\n"
      "  api.apiFunctions.setHandleRequest('queryState',\n"
      "                                    (time, callback) => {\n"
      "    this.timeArg = time;\n"
      "    callback('active');\n"
      "  });\n"
      "  api.apiFunctions.setUpdateArgumentsPreValidate(\n"
      "      'setDetectionInterval', (interval) => {\n"
      "    this.intervalArg = interval;\n"
      "    return [50];\n"
      "  });\n"
      "  this.hookedExtensionId = extensionId;\n"
      "  this.hookedContextType = contextType;\n"
      "  api.compiledApi.hookedApiProperty = 'someProperty';\n"
      "});\n";

  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // To start, check that the properties we set when running the hooks are
  // correct. We do this after calling the function because the API objects (and
  // thus the hooks) are set up lazily.
  v8::Local<v8::Object> global = context->Global();
  EXPECT_EQ(base::StringPrintf("\"%s\"", extension->id().c_str()),
            GetStringPropertyFromObject(global, context, "hookedExtensionId"));
  EXPECT_EQ("\"BLESSED_EXTENSION\"",
            GetStringPropertyFromObject(global, context, "hookedContextType"));
  v8::Local<v8::Value> idle_api =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(idle_api.IsEmpty());
  ASSERT_TRUE(idle_api->IsObject());
  EXPECT_EQ("\"someProperty\"",
            GetStringPropertyFromObject(idle_api.As<v8::Object>(), context,
                                        "hookedApiProperty"));

  // Next, we need to check two pieces: first, that the custom handler was
  // called with the proper arguments....
  EXPECT_EQ("30", GetStringPropertyFromObject(global, context, "timeArg"));

  // ...and second, that the callback was called with the proper result.
  EXPECT_EQ("\"active\"",
            GetStringPropertyFromObject(global, context, "responseState"));

  // Test the updateArgumentsPreValidate hook.
  {
    // Call the function correctly.
    const char kCallIdleSetInterval[] =
        "(function() {\n"
        "  chrome.idle.setDetectionInterval(20);\n"
        "});";

    v8::Local<v8::Function> call_idle_set_interval =
        FunctionFromString(context, kCallIdleSetInterval);
    RunFunctionOnGlobal(call_idle_set_interval, context, 0, nullptr);
  }

  // Since we don't have a custom request handler, the hook should have only
  // updated the arguments. The request then should have gone to the browser
  // normally.
  EXPECT_EQ("20", GetStringPropertyFromObject(global, context, "intervalArg"));
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.setDetectionInterval", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_FALSE(last_params().has_callback);
  EXPECT_EQ(last_params().arguments, ListValueFromString("[50]"));
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestSendRequestHook) {
  // Custom binding code. This basically utilizes the interface in binding.js in
  // order to test backwards compatibility.
  const char kCustomBinding[] =
      "apiBridge.registerCustomHook((api) => {\n"
      "  api.apiFunctions.setHandleRequest('queryState',\n"
      "                                    (time, callback) => {\n"
      "    bindingUtil.sendRequest('idle.queryState', [time, callback],\n"
      "                            undefined, undefined);\n"
      "  });\n"
      "});\n";

  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() { chrome.idle.queryState(30, function() {}); });";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_EQ(last_params().arguments, ListValueFromString("[30]"));
}

// Tests that we can notify the browser as event listeners are added or removed.
// Note: the notification logic is tested more thoroughly in the APIEventHandler
// unittests.
TEST_F(NativeExtensionBindingsSystemUnittest, TestEventRegistration) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermissions({"idle", "power"}).Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // Add a new event listener. We should be notified of the change.
  const char kEventName[] = "idle.onStateChanged";
  v8::Local<v8::Function> listener =
      FunctionFromString(context, "(function() {})");
  const char kAddListener[] =
      "(function(listener) {\n"
      "  chrome.idle.onStateChanged.addListener(listener);\n"
      "});";
  v8::Local<v8::Function> add_listener =
      FunctionFromString(context, kAddListener);
  EXPECT_CALL(*ipc_message_sender(),
              SendAddUnfilteredEventListenerIPC(script_context, kEventName))
      .Times(1);
  v8::Local<v8::Value> argv[] = {listener};
  RunFunction(add_listener, context, std::size(argv), argv);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_TRUE(bindings_system()->HasEventListenerInContext(
      "idle.onStateChanged", script_context));

  // Remove the event listener. We should be notified again.
  const char kRemoveListener[] =
      "(function(listener) {\n"
      "  chrome.idle.onStateChanged.removeListener(listener);\n"
      "});";
  EXPECT_CALL(*ipc_message_sender(),
              SendRemoveUnfilteredEventListenerIPC(script_context, kEventName))
      .Times(1);
  v8::Local<v8::Function> remove_listener =
      FunctionFromString(context, kRemoveListener);
  RunFunction(remove_listener, context, std::size(argv), argv);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
  EXPECT_FALSE(bindings_system()->HasEventListenerInContext(
      "idle.onStateChanged", script_context));
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       TestPrefixedApiEventsAndAppBinding) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder("foo", ExtensionBuilder::Type::PLATFORM_APP).Build();
  EXPECT_TRUE(app->is_platform_app());
  RegisterExtension(app);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, app.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(app->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // The 'chrome.app' object should have 'runtime' and 'window' entries, but
  // not the internal 'currentWindowInternal' object.
  v8::Local<v8::Value> app_binding_keys = V8ValueFromScriptSource(
      context, "JSON.stringify(Object.keys(chrome.app));");
  ASSERT_FALSE(app_binding_keys.IsEmpty());
  ASSERT_TRUE(app_binding_keys->IsString());
  EXPECT_EQ("[\"runtime\",\"window\"]",
            gin::V8ToString(isolate(), app_binding_keys));

  const char kUseAppRuntime[] =
      "(function() {\n"
      "  chrome.app.runtime.onLaunched.addListener(function() {});\n"
      "});";
  v8::Local<v8::Function> use_app_runtime =
      FunctionFromString(context, kUseAppRuntime);
  EXPECT_CALL(*ipc_message_sender(),
              SendAddUnfilteredEventListenerIPC(script_context,
                                                "app.runtime.onLaunched"))
      .Times(1);
  RunFunctionOnGlobal(use_app_runtime, context, 0, nullptr);
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
}

TEST_F(NativeExtensionBindingsSystemUnittest,
       TestPrefixedApiMethodsAndSystemBinding) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("system.cpu").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // The system.cpu object should exist, but system.network should not (as the
  // extension didn't request permission to it).
  v8::Local<v8::Value> system_cpu =
      V8ValueFromScriptSource(context, "chrome.system.cpu");
  ASSERT_FALSE(system_cpu.IsEmpty());
  EXPECT_TRUE(system_cpu->IsObject());
  EXPECT_FALSE(system_cpu->IsUndefined());

  v8::Local<v8::Value> system_network =
      V8ValueFromScriptSource(context, "chrome.system.network");
  ASSERT_FALSE(system_network.IsEmpty());
  EXPECT_TRUE(system_network->IsUndefined());

  const char kUseSystemCpu[] =
      "(function() {\n"
      "  chrome.system.cpu.getInfo(function() {})\n"
      "});";
  v8::Local<v8::Function> use_system_cpu =
      FunctionFromString(context, kUseSystemCpu);
  RunFunctionOnGlobal(use_system_cpu, context, 0, nullptr);

  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("system.cpu.getInfo", last_params().name);
  EXPECT_TRUE(last_params().has_callback);
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestLastError) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermissions({"idle", "power"}).Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kCallFunction[] =
      "(function() {\n"
      "  chrome.idle.queryState(30, function(state) {\n"
      "    if (chrome.runtime.lastError)\n"
      "      this.lastErrorMessage = chrome.runtime.lastError.message;\n"
      "  });\n"
      "});";
  v8::Local<v8::Function> function = FunctionFromString(context, kCallFunction);
  ASSERT_FALSE(function.IsEmpty());
  RunFunctionOnGlobal(function, context, 0, nullptr);

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);

  int first_request_id = last_params().request_id;
  // Respond with an error.
  bindings_system()->HandleResponse(last_params().request_id, false,
                                    base::Value::List(), "Some API Error");
  EXPECT_EQ("\"Some API Error\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "lastErrorMessage"));

  // Test responding with a failure, but no set error.
  RunFunctionOnGlobal(function, context, 0, nullptr);
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_NE(first_request_id, last_params().request_id);

  bindings_system()->HandleResponse(last_params().request_id, false,
                                    base::Value::List(), std::string());
  EXPECT_EQ("\"Unknown error.\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "lastErrorMessage"));
}

TEST_F(NativeExtensionBindingsSystemUnittest, TestCustomProperties) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("storage extension").AddAPIPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage");
  ASSERT_FALSE(storage.IsEmpty());
  ASSERT_TRUE(storage->IsObject());

  v8::Local<v8::Value> local =
      GetPropertyFromObject(storage.As<v8::Object>(), context, "local");
  ASSERT_FALSE(local.IsEmpty());
  ASSERT_TRUE(local->IsObject());

  v8::Local<v8::Object> local_object = local.As<v8::Object>();
  const std::vector<std::string> kKeys = {"get", "set", "remove", "clear",
                                          "getBytesInUse"};
  for (const auto& key : kKeys) {
    v8::Local<v8::String> v8_key = gin::StringToV8(isolate(), key);
    EXPECT_TRUE(local_object->HasOwnProperty(context, v8_key).FromJust())
        << key;
  }
}

// Ensure that different contexts have different API objects.
TEST_F(NativeExtensionBindingsSystemUnittest,
       CheckDifferentContextsHaveDifferentAPIObjects) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  ScriptContext* script_context_a = CreateScriptContext(
      context_a, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context_a->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context_a);

  ScriptContext* script_context_b = CreateScriptContext(
      context_b, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context_b->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context_b);

  auto check_properties_inequal = [](v8::Local<v8::Context> context_a,
                                     v8::Local<v8::Context> context_b,
                                     std::string_view property) {
    v8::Local<v8::Value> value_a = V8ValueFromScriptSource(context_a, property);
    v8::Local<v8::Value> value_b = V8ValueFromScriptSource(context_b, property);
    EXPECT_FALSE(value_a.IsEmpty()) << property;
    EXPECT_FALSE(value_b.IsEmpty()) << property;
    EXPECT_NE(value_a, value_b) << property;
  };

  check_properties_inequal(context_a, context_b, "chrome");
  check_properties_inequal(context_a, context_b, "chrome.idle");
  check_properties_inequal(context_a, context_b, "chrome.idle.onStateChanged");
}

// Tests behavior when script sets window.chrome to be various things.
TEST_F(NativeExtensionBindingsSystemUnittest, TestUsingOtherChromeObjects) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  ScriptContext* script_context_a = CreateScriptContext(
      context_a, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context_a->set_url(extension->url());
  ScriptContext* script_context_b = CreateScriptContext(
      context_b, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context_b->set_url(extension->url());

  auto check_runtime = [this, context_a, context_b, script_context_a,
                        script_context_b](bool expect_b_has_runtime) {
    bindings_system()->UpdateBindingsForContext(script_context_a);
    bindings_system()->UpdateBindingsForContext(script_context_b);

    const char kRuntime[] = "chrome.runtime";
    // chrome.runtime should always exist in context a - we only mess with
    // context b.
    EXPECT_TRUE(PropertyExists(context_a, kRuntime));
    EXPECT_EQ(expect_b_has_runtime, PropertyExists(context_b, kRuntime));
  };

  // By default, runtime should exist in both contexts (since both have access
  // to the API).
  check_runtime(true);

  {
    v8::Context::Scope scope(context_a);
    v8::Local<v8::Object> fake_chrome = v8::Object::New(isolate());
    EXPECT_EQ(context_a, fake_chrome->GetCreationContextChecked());
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // context_b has a chrome object that was created in a different context
  // (context_a), so we shouldn't have used it. This can legitimately happen in
  // the case of a parent frame modifying a child frame's window.chrome.
  check_runtime(false);

  {
    v8::Context::Scope scope(context_b);
    v8::Local<v8::Object> fake_chrome = v8::Object::New(isolate());
    EXPECT_EQ(context_b, fake_chrome->GetCreationContextChecked());
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // When the chrome object is created in the same context (context_b), that
  // object will be used.
  check_runtime(true);

  {
    v8::Context::Scope scope(context_b);
    v8::Local<v8::Boolean> fake_chrome = v8::Boolean::New(isolate(), true);
    context_b->Global()
        ->Set(context_b, gin::StringToSymbol(isolate(), "chrome"), fake_chrome)
        .ToChecked();
  }
  // A non-object chrome shouldn't be used.
  check_runtime(false);
}

// Tests updating a context's bindings after adding or removing permissions.
TEST_F(NativeExtensionBindingsSystemUnittest, TestUpdatingPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddAPIPermission("idle").Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);

  // To start, chrome.idle should be available.
  v8::Local<v8::Value> initial_idle =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(initial_idle.IsEmpty());
  EXPECT_TRUE(initial_idle->IsObject());

  {
    // chrome.power should not be defined.
    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsUndefined());
  }

  // Remove all permissions (`idle`).
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(), std::make_unique<PermissionSet>());

  bindings_system()->UpdateBindings(
      extension->id(), true /* permissions_changed */, script_context_set());
  {
    // TODO(devlin): Neither the native nor JS bindings systems clear the
    // property on the chrome object when an API is no longer available. This
    // seems unexpected, but warrants further investigation before changing
    // behavior. It can be complicated by the fact that chrome.idle may not be
    // the same chrome.idle the system instantiated, or may have additional
    // properties.
    // v8::Local<v8::Value> idle =
    //     V8ValueFromScriptSource(context, "chrome.idle");
    // ASSERT_FALSE(idle.IsEmpty());
    // EXPECT_TRUE(idle->IsUndefined());

    // chrome.power should still be undefined.
    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsUndefined());
  }

  v8::Local<v8::Function> run_idle = FunctionFromString(
      context, "(function(idle) { idle.queryState(30, function() {}); })");
  {
    // Trying to run a chrome.idle function should fail.
    v8::Local<v8::Value> args[] = {initial_idle};
    RunFunctionAndExpectError(
        run_idle, context, std::size(args), args,
        "Uncaught Error: 'idle.queryState' is not available in this context.");
    EXPECT_FALSE(has_last_params());
  }

  {
    // Add back the `idle` permission, and also add `power`.
    APIPermissionSet apis;
    apis.insert(mojom::APIPermissionID::kPower);
    apis.insert(mojom::APIPermissionID::kIdle);
    extension->permissions_data()->SetPermissions(
        std::make_unique<PermissionSet>(std::move(apis),
                                        ManifestPermissionSet(),
                                        URLPatternSet(), URLPatternSet()),
        std::make_unique<PermissionSet>());
    bindings_system()->UpdateBindings(
        extension->id(), true /* permissions_changed */, script_context_set());
  }

  {
    // Both chrome.idle and chrome.power should be defined.
    v8::Local<v8::Value> idle = V8ValueFromScriptSource(context, "chrome.idle");
    ASSERT_FALSE(idle.IsEmpty());
    EXPECT_TRUE(idle->IsObject());

    v8::Local<v8::Value> power =
        V8ValueFromScriptSource(context, "chrome.power");
    ASSERT_FALSE(power.IsEmpty());
    EXPECT_TRUE(power->IsObject());
  }

  {
    // Trying to run a chrome.idle function should now succeed.
    v8::Local<v8::Value> args[] = {initial_idle};
    RunFunction(run_idle, context, std::size(args), args);
    EXPECT_EQ("idle.queryState", last_params().name);
  }
}

TEST_F(NativeExtensionBindingsSystemUnittest, UnmanagedEvents) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kAddListeners[] =
      "(function() {\n"
      "  chrome.runtime.onMessage.addListener(function() {});\n"
      "  chrome.runtime.onConnect.addListener(function() {});\n"
      "});";

  v8::Local<v8::Function> add_listeners =
      FunctionFromString(context, kAddListeners);
  RunFunctionOnGlobal(add_listeners, context, 0, nullptr);

  // We should have no notifications for event listeners added (since the
  // mock is a strict mock, this will fail if anything was called).
  ::testing::Mock::VerifyAndClearExpectations(ipc_message_sender());
}

// Tests that a context having access to an aliased API (like networking.onc)
// does not allow for accessing the source API (networkingPrivate) directly.
TEST_F(NativeExtensionBindingsSystemUnittest,
       AccessToAliasSourceDoesntGiveAliasAccess) {
  const char kAllowlistedId[] = "jlgegmdnodfhciolbdjciihnlaljdbjo";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetID(kAllowlistedId)
          .AddAPIPermission("networkingPrivate")
          .Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);

  bindings_system()->UpdateBindingsForContext(script_context);

  // The extension only has access to networkingPrivate, so networking.onc
  // (and chrome.networking in general) should be undefined.
  EXPECT_EQ("object",
            gin::V8ToString(isolate(),
                            V8ValueFromScriptSource(
                                context, "typeof chrome.networkingPrivate")));
  EXPECT_EQ(
      "undefined",
      gin::V8ToString(isolate(), V8ValueFromScriptSource(
                                     context, "typeof chrome.networking")));
}

// Tests that a context having access to the source for an aliased API does not
// allow for accessing the alias.
TEST_F(NativeExtensionBindingsSystemUnittest,
       AccessToAliasDoesntGiveAliasSourceAccess) {
  const char kAllowlistedId[] = "jlgegmdnodfhciolbdjciihnlaljdbjo";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetID(kAllowlistedId)
          .AddAPIPermission("networking.onc")
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);

  bindings_system()->UpdateBindingsForContext(script_context);

  // The extension only has access to networking.onc, so networkingPrivate
  // should be undefined.
  EXPECT_EQ("undefined",
            gin::V8ToString(isolate(),
                            V8ValueFromScriptSource(
                                context, "typeof chrome.networkingPrivate")));
  EXPECT_EQ(
      "object",
      gin::V8ToString(isolate(), V8ValueFromScriptSource(
                                     context, "typeof chrome.networking.onc")));
}

// Test that if an extension has access to both an alias and an alias source,
// the objects on the API are different.
TEST_F(NativeExtensionBindingsSystemUnittest, AliasedAPIsAreDifferentObjects) {
  const char kAllowlistedId[] = "jlgegmdnodfhciolbdjciihnlaljdbjo";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .SetID(kAllowlistedId)
          .AddAPIPermissions({"networkingPrivate", "networking.onc"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);

  bindings_system()->UpdateBindingsForContext(script_context);

  // Both APIs should be defined, since the extension has access to each.
  EXPECT_EQ("object",
            gin::V8ToString(isolate(),
                            V8ValueFromScriptSource(
                                context, "typeof chrome.networkingPrivate")));
  EXPECT_EQ(
      "object",
      gin::V8ToString(isolate(), V8ValueFromScriptSource(
                                     context, "typeof chrome.networking.onc")));

  // The APIs should not be equal.
  bool equal = true;
  EXPECT_TRUE(gin::ConvertFromV8(
      isolate(),
      V8ValueFromScriptSource(
          context, "chrome.networkingPrivate == chrome.networking.onc"),
      &equal));
  EXPECT_FALSE(equal);
}

// Tests that script can overwrite the value of an API.
TEST_F(NativeExtensionBindingsSystemUnittest, CanOverwriteAPIs) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Function> overwrite_api =
      FunctionFromString(context, "(function() { chrome.runtime = 'bar'; })");
  RunFunction(overwrite_api, context, 0, nullptr);
  v8::Local<v8::Value> property =
      V8ValueFromScriptSource(context, "chrome.runtime");
  EXPECT_TRUE(property->IsString());
  EXPECT_EQ("bar", gin::V8ToString(isolate(), property));
}

// Tests that script can delete an API property.
TEST_F(NativeExtensionBindingsSystemUnittest, CanDeleteAPIs) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();

  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Object> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome")
          .As<v8::Object>();
  v8::Local<v8::String> runtime_key = gin::StringToSymbol(isolate(), "runtime");

  {
    v8::Maybe<bool> has_runtime = chrome->HasOwnProperty(context, runtime_key);
    ASSERT_TRUE(has_runtime.IsJust());
    EXPECT_TRUE(has_runtime.FromJust());
  }

  v8::Local<v8::Function> delete_api =
      FunctionFromString(context, "(function() { delete chrome.runtime; })");
  RunFunction(delete_api, context, 0, nullptr);

  {
    v8::Maybe<bool> has_runtime = chrome->HasOwnProperty(context, runtime_key);
    ASSERT_TRUE(has_runtime.IsJust());
    EXPECT_FALSE(has_runtime.FromJust());
  }

  v8::Local<v8::Value> property =
      V8ValueFromScriptSource(context, "chrome.runtime");
  EXPECT_TRUE(property->IsUndefined());
}

// Test that API initialization happens in the owning context.
TEST_F(NativeExtensionBindingsSystemUnittest, APIIsInitializedByOwningContext) {
  // Attach custom JS hooks.
  const char kCustomBinding[] =
      R"(this.apiBridge = apiBridge;
         apiBridge.registerCustomHook(() => {});)";
  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Create a second, uninitialized context, which will trigger the
    // construction of chrome.idle in the first context.
    set_allow_unregistered_contexts(true);
    v8::Local<v8::Context> second_context = AddContext();

    v8::Local<v8::Function> get_idle = FunctionFromString(
        second_context, "(function(chrome) { chrome.idle; })");
    v8::Local<v8::Value> chrome =
        context->Global()
            ->Get(context, gin::StringToV8(isolate(), "chrome"))
            .ToLocalChecked();
    ASSERT_TRUE(chrome->IsObject());

    v8::Context::Scope context_scope(second_context);
    v8::Local<v8::Value> args[] = {chrome};
    RunFunction(get_idle, second_context, std::size(args), args);
  }

  // The apiBridge should have been created in the owning (original) context,
  // even though the initialization was triggered by the second context.
  v8::Local<v8::Value> api_bridge =
      context->Global()
          ->Get(context, gin::StringToV8(isolate(), "apiBridge"))
          .ToLocalChecked();
  ASSERT_TRUE(api_bridge->IsObject());
  EXPECT_EQ(context, api_bridge.As<v8::Object>()->GetCreationContextChecked());
}

class SignatureValidationNativeExtensionBindingsSystemUnittest
    : public NativeExtensionBindingsSystemUnittest,
      public testing::WithParamInterface<bool> {
 public:
  SignatureValidationNativeExtensionBindingsSystemUnittest() = default;

  SignatureValidationNativeExtensionBindingsSystemUnittest(
      const SignatureValidationNativeExtensionBindingsSystemUnittest&) = delete;
  SignatureValidationNativeExtensionBindingsSystemUnittest& operator=(
      const SignatureValidationNativeExtensionBindingsSystemUnittest&) = delete;

  ~SignatureValidationNativeExtensionBindingsSystemUnittest() override =
      default;

  void SetUp() override {
    response_validation_override_ =
        binding::SetResponseValidationEnabledForTesting(GetParam());
    NativeExtensionBindingsSystemUnittest::SetUp();
  }

  void TearDown() override {
    NativeExtensionBindingsSystemUnittest::TearDown();
    response_validation_override_.reset();
  }

 private:
  std::unique_ptr<base::AutoReset<bool>> response_validation_override_;
};

TEST_P(SignatureValidationNativeExtensionBindingsSystemUnittest,
       ResponseValidation) {
  // The APIResponseValidator should only be used if response validation is
  // enabled. Otherwise, it should be null.
  EXPECT_EQ(GetParam(), bindings_system()
                            ->api_system()
                            ->request_handler()
                            ->has_response_validator_for_testing());

  std::optional<std::string> validation_failure_method_name;
  std::optional<std::string> validation_failure_error;

  auto on_validation_failure =
      [&validation_failure_method_name, &validation_failure_error](
          const std::string& method_name, const std::string& error) {
        validation_failure_method_name = method_name;
        validation_failure_error = error;
      };
  APIResponseValidator::TestHandler test_validation_failure_handler(
      base::BindLambdaForTesting(on_validation_failure));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddAPIPermissions({"idle", "power", "webRequest"})
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kCallIdleQueryState[] =
      "(function() { chrome.idle.queryState(30, function() {}); })";

  v8::Local<v8::Function> call_idle_query_state =
      FunctionFromString(context, kCallIdleQueryState);
  RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);

  EXPECT_FALSE(validation_failure_method_name);
  EXPECT_FALSE(validation_failure_error);

  // Respond with a valid value. Validation should not fail.
  ASSERT_TRUE(has_last_params());
  bindings_system()->HandleResponse(last_params().request_id, true,
                                    ListValueFromString("['active']"),
                                    std::string());

  EXPECT_FALSE(validation_failure_method_name);
  EXPECT_FALSE(validation_failure_error);

  // Run the function again, and response with an invalid value.
  RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  ASSERT_TRUE(has_last_params());
  bindings_system()->HandleResponse(last_params().request_id, true,
                                    ListValueFromString("['bad enum']"),
                                    std::string());

  // Validation should fail iff response validation is enabled.
  if (GetParam()) {
    EXPECT_EQ("idle.queryState",
              validation_failure_method_name.value_or("no value"));
    EXPECT_EQ(api_errors::ArgumentError(
                  "newState",
                  api_errors::InvalidEnumValue({"active", "idle", "locked"})),
              validation_failure_error.value_or("no value"));
  } else {
    EXPECT_FALSE(validation_failure_method_name);
    EXPECT_FALSE(validation_failure_error);
  }
}

TEST_P(SignatureValidationNativeExtensionBindingsSystemUnittest,
       EventArgumentValidation) {
  // The APIResponseValidator should only be used if response validation is
  // enabled. Otherwise, it should be null.
  EXPECT_EQ(GetParam(), bindings_system()
                            ->api_system()
                            ->request_handler()
                            ->has_response_validator_for_testing());

  std::optional<std::string> validation_failure_method_name;
  std::optional<std::string> validation_failure_error;

  auto on_validation_failure =
      [&validation_failure_method_name, &validation_failure_error](
          const std::string& method_name, const std::string& error) {
        validation_failure_method_name = method_name;
        validation_failure_error = error;
      };
  APIResponseValidator::TestHandler test_validation_failure_handler(
      base::BindLambdaForTesting(on_validation_failure));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("idle").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char kAddListenerFunction[] =
      R"((function() {
            chrome.idle.onStateChanged.addListener((state) => {
              this.returnedState = state;
            });
          });)";
  v8::Local<v8::Function> add_listener_function =
      FunctionFromString(context, kAddListenerFunction);
  RunFunctionOnGlobal(add_listener_function, context, 0, nullptr);

  EXPECT_TRUE(bindings_system()->HasEventListenerInContext(
      "idle.onStateChanged", script_context));

  // Dispatch an event with an argument that matches the expected schema.
  {
    auto event_args = base::Value::List().Append("active");
    bindings_system()->DispatchEventInContext("idle.onStateChanged", event_args,
                                              nullptr, script_context);
  }

  // Validation should have succeeded.
  std::string returned_state =
      GetStringPropertyFromObject(context->Global(), context, "returnedState");
  EXPECT_FALSE(validation_failure_method_name);
  EXPECT_FALSE(validation_failure_error);
  EXPECT_EQ(R"("active")", returned_state);

  // Now, dispatch the event with an invalid argument.
  {
    base::Value::List event_args;
    event_args.Append("bad enum");
    bindings_system()->DispatchEventInContext("idle.onStateChanged", event_args,
                                              nullptr, script_context);
  }

  // Event validation should have failed.
  returned_state =
      GetStringPropertyFromObject(context->Global(), context, "returnedState");

  if (GetParam()) {
    EXPECT_EQ(validation_failure_method_name, "idle.onStateChanged");
    EXPECT_EQ(api_errors::ArgumentError(
                  "newState",
                  api_errors::InvalidEnumValue({"active", "idle", "locked"})),
              validation_failure_error.value_or("no value"));
  } else {
    EXPECT_FALSE(validation_failure_method_name);
    EXPECT_FALSE(validation_failure_error);
  }

  // Even though validation failed, we still dispatch the event.
  EXPECT_EQ(R"("bad enum")", returned_state);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SignatureValidationNativeExtensionBindingsSystemUnittest,
    testing::Bool());

class FeatureAvailabilityNativeExtensionBindingsSystemUnittest
    : public NativeExtensionBindingsSystemUnittest,
      public testing::WithParamInterface<bool> {
 public:
  FeatureAvailabilityNativeExtensionBindingsSystemUnittest() = default;

  FeatureAvailabilityNativeExtensionBindingsSystemUnittest(
      const FeatureAvailabilityNativeExtensionBindingsSystemUnittest&) = delete;
  FeatureAvailabilityNativeExtensionBindingsSystemUnittest& operator=(
      const FeatureAvailabilityNativeExtensionBindingsSystemUnittest&) = delete;

  ~FeatureAvailabilityNativeExtensionBindingsSystemUnittest() override =
      default;

  void SetUp() override {
    if (TestApiExposedOnWebPages()) {
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      command_line->AppendSwitch(switches::kExtensionTestApiOnWebPages);
    }
    NativeExtensionBindingsSystemUnittest::SetUp();
  }

  bool TestApiExposedOnWebPages() { return GetParam(); }
};

// Tests that API methods and events that are conditionally available based on
// context are properly present or absent from the API object.
TEST_P(FeatureAvailabilityNativeExtensionBindingsSystemUnittest,
       CheckRestrictedFeaturesBasedOnContext) {
  scoped_refptr<const Extension> connectable_extension;
  {
    auto manifest = base::Value::Dict()
                        .Set("name", "connectable")
                        .Set("manifest_version", 2)
                        .Set("version", "0.1")
                        .Set("description", "test extension");
    base::Value::Dict connectable;
    connectable.Set("matches", base::Value::List().Append("*://example.com/*"));
    manifest.Set("externally_connectable", std::move(connectable));
    connectable_extension =
        ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetLocation(mojom::ManifestLocation::kInternal)
            .SetID(crx_file::id_util::GenerateId("connectable"))
            .Build();
  }

  RegisterExtension(connectable_extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> privileged_context = MainContext();
  v8::Local<v8::Context> connectable_webpage_context = AddContext();
  v8::Local<v8::Context> nonconnectable_webpage_context = AddContext();

  // Create three contexts - a privileged extension context, an externally
  // connectable web page context and a normal non-connectable web page context.
  ScriptContext* privileged_script_context =
      CreateScriptContext(privileged_context, connectable_extension.get(),
                          mojom::ContextType::kPrivilegedExtension);
  privileged_script_context->set_url(connectable_extension->url());
  bindings_system()->UpdateBindingsForContext(privileged_script_context);

  ScriptContext* connectable_webpage_script_context = CreateScriptContext(
      connectable_webpage_context, nullptr, mojom::ContextType::kWebPage);
  connectable_webpage_script_context->set_url(GURL("http://example.com"));
  bindings_system()->UpdateBindingsForContext(
      connectable_webpage_script_context);

  ScriptContext* nonconnectable_webpage_script_context = CreateScriptContext(
      nonconnectable_webpage_context, nullptr, mojom::ContextType::kWebPage);
  nonconnectable_webpage_script_context->set_url(GURL("http://notexample.com"));
  bindings_system()->UpdateBindingsForContext(
      nonconnectable_webpage_script_context);

  // Check that properties are correctly restricted. The privileged context
  // should have access to the whole runtime API, the connectable webpage should
  // only have access to sendMessage in runtime, and the non-connectable webpage
  // should only have access to runtime if it gets it from also having access to
  // the test API. The test API should be available to all the webpage contexts
  // (not the privileged evetension context), but only if the associated
  // commandline flag has been set.
  const char kRuntime[] = "chrome.runtime";
  const char kSendMessage[] = "chrome.runtime.sendMessage";
  const char kGetUrl[] = "chrome.runtime.getURL";
  const char kOnMessage[] = "chrome.runtime.onMessage";
  const char kTest[] = "chrome.test";

  ASSERT_TRUE(PropertyExists(privileged_context, kRuntime));
  EXPECT_TRUE(PropertyExists(privileged_context, kSendMessage));
  EXPECT_TRUE(PropertyExists(privileged_context, kGetUrl));
  EXPECT_TRUE(PropertyExists(privileged_context, kOnMessage));
  EXPECT_FALSE(PropertyExists(privileged_context, kTest));

  ASSERT_TRUE(PropertyExists(connectable_webpage_context, kRuntime));
  EXPECT_TRUE(PropertyExists(connectable_webpage_context, kSendMessage));
  EXPECT_FALSE(PropertyExists(connectable_webpage_context, kGetUrl));
  EXPECT_FALSE(PropertyExists(connectable_webpage_context, kOnMessage));
  EXPECT_EQ(TestApiExposedOnWebPages(),
            PropertyExists(connectable_webpage_context, kTest));

  EXPECT_EQ(TestApiExposedOnWebPages(),
            PropertyExists(nonconnectable_webpage_context, kRuntime));
  // If runtime was exposed to the page because of the test API, it will only
  // get access to sendMessage, as that is the only one exposed to web page
  // contexts.
  if (TestApiExposedOnWebPages()) {
    EXPECT_TRUE(PropertyExists(nonconnectable_webpage_context, kSendMessage));
    EXPECT_FALSE(PropertyExists(nonconnectable_webpage_context, kGetUrl));
    EXPECT_FALSE(PropertyExists(nonconnectable_webpage_context, kOnMessage));
  }
  EXPECT_EQ(TestApiExposedOnWebPages(),
            PropertyExists(nonconnectable_webpage_context, kTest));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FeatureAvailabilityNativeExtensionBindingsSystemUnittest,
    testing::Bool());

}  // namespace extensions
