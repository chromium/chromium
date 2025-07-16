// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/extra_response_data.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"

namespace extensions {

namespace {

// Test implementation of mojom::LocalFrame and mojom::LocalFrameHost.
class TestFrameImpl : public mojom::LocalFrame, public mojom::LocalFrameHost {
 public:
  explicit TestFrameImpl(
      mojo::PendingReceiver<mojom::LocalFrame> local_frame_receiver,
      mojo::PendingReceiver<mojom::LocalFrameHost> local_frame_host_receiver)
      : local_frame_receiver_(this, std::move(local_frame_receiver)),
        local_frame_host_receiver_(this, std::move(local_frame_host_receiver)) {
  }
  TestFrameImpl(const TestFrameImpl&) = delete;
  TestFrameImpl& operator=(const TestFrameImpl&) = delete;

  // mojom::LocalFrame:
  void SetFrameName(const std::string& frame_name) override {}
  void SetSpatialNavigationEnabled(bool spatial_nav_enabled) override {}
  void SetTabId(int32_t tab_id) override {}
  void AppWindowClosed(bool send_onclosed) override {}
  void NotifyRenderViewType(mojom::ViewType view_type) override {}
  void MessageInvoke(const ExtensionId& extension_id,
                     const std::string& module_name,
                     const std::string& function_name,
                     base::Value::List args) override {}
  void ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                   ExecuteCodeCallback callback) override {
    std::move(callback).Run(std::string(), GURL(), std::nullopt);
  }
  void ExecuteDeclarativeScript(int32_t tab_id,
                                const ExtensionId& extension_id,
                                const std::string& script_id,
                                const GURL& url) override {}
  void UpdateBrowserWindowId(int32_t window_id) override {}
  void DispatchOnConnect(
      const PortId& port_id,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      mojom::TabConnectionInfoPtr tab_info,
      mojom::ExternalConnectionInfoPtr external_connection_info,
      mojo::PendingAssociatedReceiver<mojom::MessagePort> port,
      mojo::PendingAssociatedRemote<mojom::MessagePortHost> port_host,
      DispatchOnConnectCallback callback) override {
    std::move(callback).Run(false);
  }

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const ExtensionId& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetAppInstallState(const GURL& url,
                          GetAppInstallStateCallback callback) override {
    std::move(callback).Run(std::string());
  }
  void Request(mojom::RequestParamsPtr params,
               RequestCallback callback) override {
    std::move(callback).Run(false, base::Value::List(), std::string(), nullptr);
  }
  void ResponseAck(const base::Uuid& request_uuid) override {}
  void WatchedPageChange(
      const std::vector<std::string>& css_selectors) override {}
  void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const std::vector<StackFrame>& stack_trace,
      blink::mojom::ConsoleMessageLevel level) override {}
  void ContentScriptsExecuting(
      const base::flat_map<std::string, std::vector<std::string>>&
          extension_id_to_scripts,
      const GURL& frame_url) override {}
  void IncrementLazyKeepaliveCount() override {}
  void DecrementLazyKeepaliveCount() override {}
  void AppWindowReady() override {}
  void OpenChannelToExtension(
      mojom::ExternalConnectionInfoPtr info,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {}
  void OpenChannelToNativeApp(
      const std::string& native_app_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {}
  void OpenChannelToTab(int32_t tab_id,
                        int32_t frame_id,
                        const std::optional<std::string>& document_id,
                        mojom::ChannelType channel_type,
                        const std::string& channel_name,
                        const PortId& port_id,
                        mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                        mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                            port_host) override {}

 private:
  mojo::Receiver<mojom::LocalFrame> local_frame_receiver_;
  mojo::Receiver<mojom::LocalFrameHost> local_frame_host_receiver_;
};

class FrameMojomExtensionIdTest : public testing::Test {
 public:
  FrameMojomExtensionIdTest() {
    frame_impl_ = std::make_unique<TestFrameImpl>(
        local_frame_remote_.BindNewPipeAndPassReceiver(),
        local_frame_host_remote_.BindNewPipeAndPassReceiver());
  }

  void MessageInvoke(const ExtensionId& extension_id) {
    local_frame_remote_->MessageInvoke(extension_id, "test_module",
                                       "test_function", base::Value::List());
    local_frame_remote_.FlushForTesting();
  }

  void ExecuteDeclarativeScript(const ExtensionId& extension_id) {
    local_frame_remote_->ExecuteDeclarativeScript(0, extension_id, "test_id",
                                                  GURL());
    local_frame_remote_.FlushForTesting();
  }

  void RequestScriptInjectionPermission(const ExtensionId& extension_id) {
    local_frame_host_remote_->RequestScriptInjectionPermission(
        extension_id, mojom::InjectionType::kContentScript,
        mojom::RunLocation::kBrowserDriven, base::DoNothing());
    local_frame_host_remote_.FlushForTesting();
  }

  bool PipeConnected() {
    return local_frame_remote_.is_connected() &&
           local_frame_host_remote_.is_connected();
  }

  void RebindReceiver() {
    frame_impl_.reset();
    local_frame_remote_.reset();
    local_frame_host_remote_.reset();
    frame_impl_ = std::make_unique<TestFrameImpl>(
        local_frame_remote_.BindNewPipeAndPassReceiver(),
        local_frame_host_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::LocalFrame> local_frame_remote_;
  mojo::Remote<mojom::LocalFrameHost> local_frame_host_remote_;
  std::unique_ptr<TestFrameImpl> frame_impl_;
};

// Tests that passing valid extension IDs to mojom::LocalFrame and
// mojom::LocalFrameHost implementations pass message validation and keep the
// mojom pipe connected.
TEST_F(FrameMojomExtensionIdTest, ValidExtensionId) {
  // Create a valid ExtensionId.
  ExtensionId valid_extension_id(32, 'a');

  MessageInvoke(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  ExecuteDeclarativeScript(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RequestScriptInjectionPermission(valid_extension_id);
  ASSERT_TRUE(PipeConnected());
}

// Tests that passing invalid extension IDs to mojom::LocalFrame and
// mojom::LocalFrameHost implementations fail message validation and close the
// mojom pipe.
TEST_F(FrameMojomExtensionIdTest, InvalidExtensionId) {
  // Create an invalid ExtensionId.
  ExtensionId invalid_extension_id = "invalid_id";

  MessageInvoke(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  ExecuteDeclarativeScript(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RequestScriptInjectionPermission(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());
}

}  // namespace
}  // namespace extensions
