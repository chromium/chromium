// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/storage_area.h"

#include "base/cxx17_backports.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

using StorageAreaTest = NativeExtensionBindingsSystemUnittest;

// A specialization of StorageAreaTest that pretends it's running
// on version_info::Channel::UNKNOWN.
class StorageAreaTrunkTest : public StorageAreaTest {
 public:
  StorageAreaTrunkTest() = default;
  ~StorageAreaTrunkTest() override = default;
  StorageAreaTrunkTest(const StorageAreaTrunkTest& other) = delete;
  StorageAreaTrunkTest& operator=(const StorageAreaTrunkTest& other) = delete;

 private:
  // TODO(crbug.com/1185226): Remove unknown channel when chrome.storage.session
  // is released in stable.
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
};

// Test that trying to use StorageArea.get without a StorageArea `this` fails
// (with a helpful error message).
TEST_F(StorageAreaTest, TestUnboundedUse) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
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
      run_storage_get, context, base::size(args), args,
      "Uncaught TypeError: Illegal invocation: Function must be called on "
      "an object of type StorageArea");
}

TEST_F(StorageAreaTest, TestUseAfterInvalidation) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
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
  RunFunction(run_storage_get, context, base::size(args), args);

  DisposeContext(context);

  EXPECT_FALSE(binding::IsContextValid(context));
  RunFunctionAndExpectError(run_storage_get, context, base::size(args), args,
                            "Uncaught Error: Extension context invalidated.");
}

TEST_F(StorageAreaTest, InvalidInvocationError) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").AddPermission("storage").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
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
      run_storage_get, context, base::size(args), args,
      "Uncaught TypeError: " +
          api_errors::InvocationError(
              "storage.get",
              "optional [string|array|object] keys, function callback",
              "No matching signature."));
}

TEST_F(StorageAreaTrunkTest, HasOnChanged) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .SetManifestKey("manifest_version", 3)
          .AddPermission("storage")
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
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

    bindings_system()->DispatchEventInContext(
        base::StringPrintf("storage.%s.onChanged", kStorage).c_str(),
        ListValueFromString("['foo']").get(), nullptr, script_context);

    EXPECT_EQ("\"foo\"", GetStringPropertyFromObject(context->Global(), context,
                                                     "change"));
  }
}

}  // namespace extensions
