// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Test implementation of mojom::Renderer.
class TestRendererImpl : public mojom::Renderer {
 public:
  explicit TestRendererImpl(mojo::PendingReceiver<mojom::Renderer> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestRendererImpl(const TestRendererImpl&) = delete;
  TestRendererImpl& operator=(const TestRendererImpl&) = delete;

  // mojom::Renderer (methods with ExtensionId arguments):
  void ActivateExtension(const ExtensionId& extension_id) override {}
  void UnloadExtension(const ExtensionId& extension_id) override {}
  void SuspendExtension(
      const ExtensionId& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override {
    std::move(callback).Run();
  }
  void CancelSuspendExtension(const ExtensionId& extension_id) override {}
  void SetUserScriptsAllowed(const ExtensionId& extension_id,
                             bool enabled) override {}
  void SetScriptingAllowlist(
      const std::vector<ExtensionId>& extension_ids) override {}
  void ClearUserScriptWorldConfig(
      const ExtensionId& extension_id,
      const std::optional<std::string>& world_id) override {}
  void UpdatePermissions(const ExtensionId& extension_id,
                         PermissionSet active_permissions,
                         PermissionSet withheld_permissions,
                         URLPatternSet policy_blocked_hosts,
                         URLPatternSet policy_allowed_hosts,
                         bool uses_default_policy_host_restrictions) override {}
  void UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                    URLPatternSet new_hosts,
                                    int tab_id,
                                    bool update_origin_allowlist) override {}
  void ClearTabSpecificPermissions(
      const std::vector<ExtensionId>& extension_ids,
      int tab_id,
      bool update_origin_allowlist) override {}

  // mojom::Renderer (other methods):
  void SetActivityLoggingEnabled(bool enabled) override {}
  void LoadExtensions(
      std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) override {
  }
  void SetDeveloperMode(bool current_developer_mode) override {}
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session_type) override {}
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override {}
  void SetWebViewPartitionID(const std::string& partition_id) override {}
  void UpdateUserScriptWorlds(
      std::vector<mojom::UserScriptWorldInfoPtr> infos) override {}
  void ShouldSuspend(ShouldSuspendCallback callback) override {
    std::move(callback).Run();
  }
  void TransferBlobs(TransferBlobsCallback callback) override {
    std::move(callback).Run();
  }
  void UpdateDefaultPolicyHostRestrictions(
      URLPatternSet default_policy_blocked_hosts,
      URLPatternSet default_policy_allowed_hosts) override {}
  void UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                  URLPatternSet user_allowed_hosts) override {}
  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id) override {}
  void WatchPages(const std::vector<std::string>& css_selectors) override {}

 private:
  mojo::Receiver<mojom::Renderer> receiver_;
};

class RendererMojomExtensionIdTest : public testing::Test {
 public:
  RendererMojomExtensionIdTest() {
    renderer_impl_ = std::make_unique<TestRendererImpl>(
        renderer_remote_.BindNewPipeAndPassReceiver());
  }

  void ActivateExtension(const ExtensionId& extension_id) {
    renderer_remote_->ActivateExtension(extension_id);
    renderer_remote_.FlushForTesting();
  }

  void UnloadExtension(const ExtensionId& extension_id) {
    renderer_remote_->UnloadExtension(extension_id);
    renderer_remote_.FlushForTesting();
  }

  void SuspendExtension(const ExtensionId& extension_id) {
    renderer_remote_->SuspendExtension(extension_id, base::DoNothing());
    renderer_remote_.FlushForTesting();
  }

  void CancelSuspendExtension(const ExtensionId& extension_id) {
    renderer_remote_->CancelSuspendExtension(extension_id);
    renderer_remote_.FlushForTesting();
  }

  void SetUserScriptsAllowed(const ExtensionId& extension_id) {
    renderer_remote_->SetUserScriptsAllowed(extension_id, true);
    renderer_remote_.FlushForTesting();
  }

  void SetScriptingAllowlist(const std::vector<ExtensionId>& extension_ids) {
    renderer_remote_->SetScriptingAllowlist(extension_ids);
    renderer_remote_.FlushForTesting();
  }

  void ClearUserScriptWorldConfig(const ExtensionId& extension_id) {
    renderer_remote_->ClearUserScriptWorldConfig(extension_id, std::nullopt);
    renderer_remote_.FlushForTesting();
  }

  void UpdatePermissions(const ExtensionId& extension_id) {
    renderer_remote_->UpdatePermissions(extension_id, PermissionSet(),
                                        PermissionSet(), URLPatternSet(),
                                        URLPatternSet(), false);
    renderer_remote_.FlushForTesting();
  }

  void UpdateTabSpecificPermissions(const ExtensionId& extension_id) {
    renderer_remote_->UpdateTabSpecificPermissions(extension_id,
                                                   URLPatternSet(), 1, true);
    renderer_remote_.FlushForTesting();
  }

  void ClearTabSpecificPermissions(
      const std::vector<ExtensionId>& extension_ids) {
    renderer_remote_->ClearTabSpecificPermissions(extension_ids, 1, true);
    renderer_remote_.FlushForTesting();
  }

  bool IsPipeConnected() { return renderer_remote_.is_connected(); }

  void RebindReceiver() {
    renderer_impl_.reset();
    renderer_remote_.reset();
    renderer_impl_ = std::make_unique<TestRendererImpl>(
        renderer_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::Renderer> renderer_remote_;
  std::unique_ptr<TestRendererImpl> renderer_impl_;
};

// Tests that passing valid extension IDs to mojom::Renderer implementations
// pass message validation and keep the mojom pipe connected.
TEST_F(RendererMojomExtensionIdTest, ValidExtensionId) {
  ExtensionId valid_extension_id(32, 'a');
  std::vector<ExtensionId> valid_extension_ids = {valid_extension_id};

  ActivateExtension(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  UnloadExtension(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  SuspendExtension(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  CancelSuspendExtension(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  SetUserScriptsAllowed(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  SetScriptingAllowlist(valid_extension_ids);
  ASSERT_TRUE(IsPipeConnected());

  ClearUserScriptWorldConfig(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  UpdatePermissions(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  UpdateTabSpecificPermissions(valid_extension_id);
  ASSERT_TRUE(IsPipeConnected());

  ClearTabSpecificPermissions(valid_extension_ids);
  ASSERT_TRUE(IsPipeConnected());
}

// Tests that passing invalid extension IDs to mojom::Renderer
// implementations fail message validation and close the mojom pipe.
TEST_F(RendererMojomExtensionIdTest, InvalidExtensionId) {
  ExtensionId valid_extension_id(32, 'a');
  ExtensionId invalid_extension_id = "invalid_id";
  std::vector<ExtensionId> invalid_extension_ids = {invalid_extension_id};
  std::vector<ExtensionId> mixed_extension_ids = {valid_extension_id,
                                                  invalid_extension_id};
  // Test with invalid extension IDs.
  ActivateExtension(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  UnloadExtension(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  SuspendExtension(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  CancelSuspendExtension(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  SetUserScriptsAllowed(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  ClearUserScriptWorldConfig(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  UpdatePermissions(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  UpdateTabSpecificPermissions(invalid_extension_id);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  SetScriptingAllowlist(invalid_extension_ids);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  ClearTabSpecificPermissions(invalid_extension_ids);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  // Test with a mix of valid and invalid IDs in the vector.
  SetScriptingAllowlist(mixed_extension_ids);
  ASSERT_FALSE(IsPipeConnected());
  RebindReceiver();

  ClearTabSpecificPermissions(mixed_extension_ids);
  ASSERT_FALSE(IsPipeConnected());
}

}  // namespace
}  // namespace extensions
