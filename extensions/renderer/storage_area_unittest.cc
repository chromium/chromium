// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/storage_area.h"

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using StorageAreaTest = NativeExtensionBindingsSystemUnittest;

// Test that trying to use StorageArea.get without a StorageArea `this` fails
// (with a helpful error message).
TEST_F(StorageAreaTest, TestUnboundedUse) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage_get =
      V8ValueFromScriptSource(context, "chrome.storage.local.get");
  ASSERT_TRUE(storage_get->IsFunction());

  constexpr char kRunStorageGet[] =
      "(function(get) { get('foo', function() {}); })";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage_get};
  RunFunctionAndExpectError(
      run_storage_get, context, std::size(args), args,
      "Uncaught TypeError: Illegal invocation: Function must be called on "
      "an object of type StorageArea");
}

TEST_F(StorageAreaTest, TestUseAfterInvalidation) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage.local");
  ASSERT_TRUE(storage->IsObject());

  constexpr char kRunStorageGet[] =
      "(function(storage) { storage.get('foo', function() {}); })";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage};
  RunFunction(run_storage_get, context, std::size(args), args);

  DisposeContext(context);

  EXPECT_FALSE(binding::IsContextValid(context));
  RunFunctionAndExpectError(run_storage_get, context, std::size(args), args,
                            "Uncaught Error: Extension context invalidated.");
}

TEST_F(StorageAreaTest, InvalidInvocationError) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddAPIPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage.local");
  ASSERT_TRUE(storage->IsObject());

  constexpr char kRunStorageGet[] =
      "(function(storage) { storage.get(1, function() {}); })";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage};
  RunFunctionAndExpectError(
      run_storage_get, context, std::size(args), args,
      "Uncaught TypeError: " +
          api_errors::InvocationError(
              "storage.get",
              "optional [string|array|object] keys, function callback",
              "No matching signature."));
}

TEST_F(StorageAreaTest, HasOnChanged) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                 .SetManifestVersion(3)
                                                 .AddAPIPermission("storage")
                                                 .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  const char* kStorages[] = {"sync", "local", "managed", "session"};
  for (auto* kStorage : kStorages) {
    const std::string kRegisterListener = base::StringPrintf(
        R"((function() {
             chrome.storage.%s.onChanged.addListener(
                function(change) {
                  this.change = change;
              });
        }))",
        kStorage);
    v8::Local<v8::Function> add_listener =
        FunctionFromString(context, kRegisterListener);
    RunFunctionOnGlobal(add_listener, context, 0, nullptr);

    base::Value::List value = ListValueFromString("['foo']");
    bindings_system()->DispatchEventInContext(
        base::StringPrintf("storage.%s.onChanged", kStorage).c_str(), value,
        nullptr, script_context);

    EXPECT_EQ("\"foo\"", GetStringPropertyFromObject(context->Global(), context,
                                                     "change"));
  }
}

TEST_F(StorageAreaTest, PromiseBasedFunctionsForManifestV3) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                 .SetManifestVersion(3)
                                                 .AddAPIPermission("storage")
                                                 .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage.local");
  ASSERT_TRUE(storage->IsObject());

  constexpr char kRunStorageGet[] =
      "(function(storage) { return storage.get('foo'); });";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage};
  v8::Local<v8::Value> return_value =
      RunFunctionOnGlobal(run_storage_get, context, std::size(args), args);

  ASSERT_TRUE(return_value->IsPromise());
  v8::Local<v8::Promise> promise = return_value.As<v8::Promise>();

  EXPECT_EQ(v8::Promise::kPending, promise->State());

  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("storage.get", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  // We treat returning a promise as having a callback in the request params.
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_THAT(last_params().arguments,
              base::test::IsJson(R"(["local", "foo"])"));

  bindings_system()->HandleResponse(last_params().request_id, /*success=*/true,
                                    ListValueFromString(R"([{"foo": 42}])"),
                                    /*error=*/std::string());

  EXPECT_EQ(v8::Promise::kFulfilled, promise->State());
  EXPECT_EQ(R"({"foo":42})", V8ToString(promise->Result(), context));
}

TEST_F(StorageAreaTest, PromiseBasedFunctionsDisallowedForManifestV2) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo")
                                                 .SetManifestVersion(2)
                                                 .AddAPIPermission("storage")
                                                 .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> storage =
      V8ValueFromScriptSource(context, "chrome.storage.local");
  ASSERT_TRUE(storage->IsObject());

  constexpr char kRunStorageGet[] =
      "(function(storage) { this.returnValue = storage.get('foo'); });";
  v8::Local<v8::Function> run_storage_get =
      FunctionFromString(context, kRunStorageGet);
  v8::Local<v8::Value> args[] = {storage};
  auto expected_error =
      "Uncaught TypeError: " +
      api_errors::InvocationError(
          "storage.get",
          "optional [string|array|object] keys, function callback",
          api_errors::NoMatchingSignature());
  RunFunctionAndExpectError(run_storage_get, context, std::size(args), args,
                            expected_error);
}

}  // namespace extensions
