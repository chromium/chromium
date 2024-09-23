// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_TEST_BASE_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/frame.mojom-forward.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/core_extensions_renderer_api_provider.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "v8/include/v8-forward.h"

namespace content {
class MockRenderThread;
}

namespace v8 {
class ExtensionConfiguration;
}

namespace extensions {

class ScriptContext;
class ScriptContextSet;

// A mocked-up IPC message sender for use in testing.
class TestIPCMessageSender : public IPCMessageSender {
 public:
  TestIPCMessageSender();

  TestIPCMessageSender(const TestIPCMessageSender&) = delete;
  TestIPCMessageSender& operator=(const TestIPCMessageSender&) = delete;

  ~TestIPCMessageSender() override;

  // IPCMessageSender:
  void SendRequestIPC(ScriptContext* context,
                      mojom::RequestParamsPtr params) override;
  MOCK_METHOD2(SendResponseAckIPC,
               void(ScriptContext* context, const base::Uuid& uuid));
  // The event listener methods are less of a pain to mock (since they don't
  // have complex parameters like mojom::RequestParams).
  MOCK_METHOD2(SendAddUnfilteredEventListenerIPC,
               void(ScriptContext* context, const std::string& event_name));
  MOCK_METHOD2(SendRemoveUnfilteredEventListenerIPC,
               void(ScriptContext* context, const std::string& event_name));

  // Send a message to add/remove a lazy unfiltered listener.
  MOCK_METHOD2(SendAddUnfilteredLazyEventListenerIPC,
               void(ScriptContext* context, const std::string& event_name));
  MOCK_METHOD2(SendRemoveUnfilteredLazyEventListenerIPC,
               void(ScriptContext* context, const std::string& event_name));

  // Send a message to add/remove a filtered listener.
  MOCK_METHOD4(SendAddFilteredEventListenerIPC,
               void(ScriptContext* context,
                    const std::string& event_name,
                    const base::Value::Dict& filter,
                    bool is_lazy));
  MOCK_METHOD4(SendRemoveFilteredEventListenerIPC,
               void(ScriptContext* context,
                    const std::string& event_name,
                    const base::Value::Dict& filter,
                    bool remove_lazy_listener));
  MOCK_METHOD2(
      SendBindAutomationIPC,
      void(ScriptContext* context,
           mojo::PendingAssociatedRemote<ax::mojom::Automation> remote));
  MOCK_METHOD7(
      SendOpenMessageChannel,
      void(ScriptContext* script_context,
           const PortId& port_id,
           const MessageTarget& target,
           mojom::ChannelType channel_type,
           const std::string& channel_name,
           mojo::PendingAssociatedRemote<mojom::MessagePort> port,
           mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host));
  MOCK_METHOD6(SendActivityLogIPC,
               void(ScriptContext* script_context,
                    const ExtensionId& extension_id,
                    IPCMessageSender::ActivityLogCallType call_type,
                    const std::string& call_name,
                    base::Value::List args,
                    const std::string& extra));
  const mojom::RequestParams* last_params() const { return last_params_.get(); }

 private:
  mojom::RequestParamsPtr last_params_;
};

// A test harness to instantiate the NativeExtensionBindingsSystem (along with
// its dependencies) and support adding/removing extensions and ScriptContexts.
// This is useful for bindings tests that need extensions-specific knowledge.
class NativeExtensionBindingsSystemUnittest
    : public APIBindingTest,
      public NativeExtensionBindingsSystem::Delegate {
 public:
  NativeExtensionBindingsSystemUnittest();

  NativeExtensionBindingsSystemUnittest(
      const NativeExtensionBindingsSystemUnittest&) = delete;
  NativeExtensionBindingsSystemUnittest& operator=(
      const NativeExtensionBindingsSystemUnittest&) = delete;

  ~NativeExtensionBindingsSystemUnittest() override;

 protected:
  // APIBindingTest:
  void SetUp() override;
  void TearDown() override;
  void OnWillDisposeContext(v8::Local<v8::Context> context) override;
  v8::ExtensionConfiguration* GetV8ExtensionConfiguration() override;
  std::unique_ptr<TestJSRunner::Scope> CreateTestJSRunner() override;

  ScriptContext* CreateScriptContext(v8::Local<v8::Context> v8_context,
                                     const Extension* extension,
                                     mojom::ContextType context_type);

  void RegisterExtension(scoped_refptr<const Extension> extension);

  // Returns whether or not a StrictMock should be used for the
  // IPCMessageSender. The default is to return false.
  virtual bool UseStrictIPCMessageSender();

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  bool has_last_params() const { return !!ipc_message_sender_->last_params(); }
  const mojom::RequestParams& last_params() {
    return *ipc_message_sender_->last_params();
  }
  StringSourceMap* source_map() { return &source_map_; }
  TestIPCMessageSender* ipc_message_sender() { return ipc_message_sender_; }
  ScriptContextSet* script_context_set() { return script_context_set_.get(); }
  void set_allow_unregistered_contexts(bool allow_unregistered_contexts) {
    allow_unregistered_contexts_ = allow_unregistered_contexts;
  }

  // NativeExtensionBindingsSystem::Delegate implementation.
  ScriptContextSetIterable* GetScriptContextSet() override;

 private:
  ExtensionIdSet extension_ids_;
  std::unique_ptr<content::MockRenderThread> render_thread_;
  std::unique_ptr<ScriptContextSet> script_context_set_;
  std::vector<raw_ptr<ScriptContext, VectorExperimental>> raw_script_contexts_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
  // The TestIPCMessageSender; owned by the bindings system.
  raw_ptr<TestIPCMessageSender, DanglingUntriaged> ipc_message_sender_ =
      nullptr;

  StringSourceMap source_map_;
  TestExtensionsRendererClient renderer_client_;
  CoreExtensionsRendererAPIProvider api_provider_;

  // True if we allow some v8::Contexts to avoid registration as a
  // ScriptContext.
  bool allow_unregistered_contexts_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_TEST_BASE_H_
