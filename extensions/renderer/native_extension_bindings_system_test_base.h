// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_UNITTEST_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_UNITTEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_extensions_renderer_client.h"
#include "testing/gmock/include/gmock/gmock.h"

struct ExtensionHostMsg_Request_Params;

namespace base {
class DictionaryValue;
}

namespace content {
class MockRenderThread;
}

namespace v8 {
class ExtensionConfiguration;
}

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;
class ScriptContextSet;

// A mocked-up IPC message sender for use in testing.
class TestIPCMessageSender : public IPCMessageSender {
 public:
  TestIPCMessageSender();
  ~TestIPCMessageSender() override;

  // IPCMessageSender:
  void SendRequestIPC(
      ScriptContext* context,
      std::unique_ptr<ExtensionHostMsg_Request_Params> params) override;
  void SendOnRequestResponseReceivedIPC(int request_id) override {}
  // The event listener methods are less of a pain to mock (since they don't
  // have complex parameters like ExtensionHostMsg_Request_Params).
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
                    const base::DictionaryValue& filter,
                    bool is_lazy));
  MOCK_METHOD4(SendRemoveFilteredEventListenerIPC,
               void(ScriptContext* context,
                    const std::string& event_name,
                    const base::DictionaryValue& filter,
                    bool remove_lazy_listener));

  MOCK_METHOD5(SendOpenMessageChannel,
               void(ScriptContext* script_context,
                    const PortId& port_id,
                    const MessageTarget& target,
                    const std::string& channel_name,
                    bool include_tls_channel_id));
  MOCK_METHOD2(SendOpenMessagePort,
               void(int routing_id, const PortId& port_id));
  MOCK_METHOD3(SendCloseMessagePort,
               void(int routing_id, const PortId& port_id, bool close_channel));
  MOCK_METHOD2(SendPostMessageToPort,
               void(const PortId& port_id, const Message& message));

  const ExtensionHostMsg_Request_Params* last_params() const {
    return last_params_.get();
  }

 private:
  std::unique_ptr<ExtensionHostMsg_Request_Params> last_params_;

  DISALLOW_COPY_AND_ASSIGN(TestIPCMessageSender);
};

// A test harness to instantiate the NativeExtensionBindingsSystem (along with
// its dependencies) and support adding/removing extensions and ScriptContexts.
// This is useful for bindings tests that need extensions-specific knowledge.
class NativeExtensionBindingsSystemUnittest : public APIBindingTest {
 public:
  NativeExtensionBindingsSystemUnittest();
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
                                     Feature::Context context_type);

  void RegisterExtension(scoped_refptr<const Extension> extension);

  // Returns whether or not a StrictMock should be used for the
  // IPCMessageSender. The default is to return false.
  virtual bool UseStrictIPCMessageSender();

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  bool has_last_params() const { return !!ipc_message_sender_->last_params(); }
  const ExtensionHostMsg_Request_Params& last_params() {
    return *ipc_message_sender_->last_params();
  }
  StringSourceMap* source_map() { return &source_map_; }
  TestIPCMessageSender* ipc_message_sender() { return ipc_message_sender_; }
  ScriptContextSet* script_context_set() { return script_context_set_.get(); }
  void set_allow_unregistered_contexts(bool allow_unregistered_contexts) {
    allow_unregistered_contexts_ = allow_unregistered_contexts;
  }

 private:
  ExtensionIdSet extension_ids_;
  std::unique_ptr<content::MockRenderThread> render_thread_;
  std::unique_ptr<ScriptContextSet> script_context_set_;
  std::vector<ScriptContext*> raw_script_contexts_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
  // The TestIPCMessageSender; owned by the bindings system.
  TestIPCMessageSender* ipc_message_sender_ = nullptr;

  std::unique_ptr<ExtensionHostMsg_Request_Params> last_params_;

  StringSourceMap source_map_;
  TestExtensionsRendererClient renderer_client_;

  // True if we allow some v8::Contexts to avoid registration as a
  // ScriptContext.
  bool allow_unregistered_contexts_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystemUnittest);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_UNITTEST_H_
