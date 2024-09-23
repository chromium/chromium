// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system_test_base.h"

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "content/public/test/mock_render_thread.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/utils/extension_utils.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "extensions/renderer/test_v8_extension_configuration.h"

namespace extensions {

TestIPCMessageSender::TestIPCMessageSender() = default;
TestIPCMessageSender::~TestIPCMessageSender() = default;
void TestIPCMessageSender::SendRequestIPC(ScriptContext* context,
                                          mojom::RequestParamsPtr params) {
  last_params_ = std::move(params);
}

NativeExtensionBindingsSystemUnittest::NativeExtensionBindingsSystemUnittest() {
}

NativeExtensionBindingsSystemUnittest::
    ~NativeExtensionBindingsSystemUnittest() {}

v8::ExtensionConfiguration*
NativeExtensionBindingsSystemUnittest::GetV8ExtensionConfiguration() {
  return TestV8ExtensionConfiguration::GetConfiguration();
}

void NativeExtensionBindingsSystemUnittest::SetUp() {
  render_thread_ = std::make_unique<content::MockRenderThread>();
  script_context_set_ = std::make_unique<ScriptContextSet>(&extension_ids_);
  std::unique_ptr<TestIPCMessageSender> message_sender;
  if (UseStrictIPCMessageSender()) {
    message_sender =
        std::make_unique<testing::StrictMock<TestIPCMessageSender>>();
  } else {
    message_sender = std::make_unique<TestIPCMessageSender>();
  }
  ipc_message_sender_ = message_sender.get();
  bindings_system_ = std::make_unique<NativeExtensionBindingsSystem>(
      this, std::move(message_sender));
  api_provider_.AddBindingsSystemHooks(/*dispatcher=*/nullptr,
                                       bindings_system_.get());
  APIBindingTest::SetUp();
}

void NativeExtensionBindingsSystemUnittest::TearDown() {
  // Dispose all contexts now so we call WillReleaseScriptContext() on the
  // bindings system.
  DisposeAllContexts();

  // ScriptContexts are deleted asynchronously by the ScriptContextSet, so we
  // need spin here to ensure we don't leak. See also
  // ScriptContextSet::Remove().
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(raw_script_contexts_.empty());
  script_context_set_.reset();
  bindings_system_.reset();
  render_thread_.reset();
  APIBindingTest::TearDown();
}

ScriptContext* NativeExtensionBindingsSystemUnittest::CreateScriptContext(
    v8::Local<v8::Context> v8_context,
    const Extension* extension,
    mojom::ContextType context_type) {
  auto script_context = std::make_unique<ScriptContext>(
      v8_context, nullptr, GenerateHostIdFromExtension(extension), extension,
      /*blink_isolated_world_id=*/std::nullopt, context_type, extension,
      context_type);
  script_context->SetModuleSystem(
      std::make_unique<ModuleSystem>(script_context.get(), source_map()));
  ScriptContext* raw_script_context = script_context.get();
  raw_script_contexts_.push_back(raw_script_context);
  script_context_set_->AddForTesting(std::move(script_context));
  bindings_system_->DidCreateScriptContext(raw_script_context);
  return raw_script_context;
}

void NativeExtensionBindingsSystemUnittest::OnWillDisposeContext(
    v8::Local<v8::Context> context) {
  auto iter = base::ranges::find(raw_script_contexts_, context,
                                 &ScriptContext::v8_context);
  if (iter == raw_script_contexts_.end()) {
    ASSERT_TRUE(allow_unregistered_contexts_);
    return;
  }
  bindings_system_->WillReleaseScriptContext(*iter);
  script_context_set_->Remove(*iter);
  raw_script_contexts_.erase(iter);
}

std::unique_ptr<TestJSRunner::Scope>
NativeExtensionBindingsSystemUnittest::CreateTestJSRunner() {
  // The NativeExtensionBindingsSystem handles setting the JSRunner for each
  // context, so we don't need to fake one.
  return nullptr;
}

void NativeExtensionBindingsSystemUnittest::RegisterExtension(
    scoped_refptr<const Extension> extension) {
  extension_ids_.insert(extension->id());
  RendererExtensionRegistry::Get()->Insert(extension);
}

bool NativeExtensionBindingsSystemUnittest::UseStrictIPCMessageSender() {
  return false;
}

ScriptContextSetIterable*
NativeExtensionBindingsSystemUnittest::GetScriptContextSet() {
  return script_context_set_.get();
}

}  // namespace extensions
