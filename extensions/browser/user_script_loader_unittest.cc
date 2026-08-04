// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_loader.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/embedder_user_script_loader.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/user_script.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A RendererStartupHelper that intercepts mojom::Renderer messages and records
// which processes receive UpdateUserScripts. This lets the tests below verify
// that user scripts are only shipped to the intended set of renderers.
class TestRendererStartupHelper : public RendererStartupHelper,
                                  public mojom::Renderer {
 public:
  explicit TestRendererStartupHelper(content::BrowserContext* browser_context)
      : RendererStartupHelper(browser_context) {}

  static std::unique_ptr<KeyedService> Build(
      content::BrowserContext* browser_context) {
    return std::make_unique<TestRendererStartupHelper>(browser_context);
  }

  bool ProcessReceivedUpdateUserScripts(
      content::RenderProcessHost* process) const {
    return updated_processes_.contains(process);
  }

 protected:
  // RendererStartupHelper:
  mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process) override {
    mojo::AssociatedRemote<mojom::Renderer> remote;
    renderer_receivers_.Add(
        this, remote.BindNewEndpointAndPassDedicatedReceiver(), process);
    return remote.Unbind();
  }

 private:
  // mojom::Renderer:
  void ActivateExtension(const ExtensionId& extension_id) override {}
  void SetActivityLoggingEnabled(bool enabled) override {}
  void SetPolicyActivityLoggingEnabled(bool enabled) override {}
  void LoadExtensions(
      std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) override {
  }
  void UnloadExtension(const ExtensionId& extension_id) override {}
  void SuspendExtension(
      const ExtensionId& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override {
    std::move(callback).Run();
  }
  void CancelSuspendExtension(const ExtensionId& extension_id) override {}
  void SetDeveloperMode(bool current_developer_mode) override {}
  void SetUserScriptsAllowed(const ExtensionId& extension_id,
                             bool allowed) override {}
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session) override {}
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override {}
  void SetWebViewPartitionID(const std::string& partition_id) override {}
  void SetScriptingAllowlist(
      const std::vector<ExtensionId>& extension_ids) override {}
  void UpdateUserScriptWorlds(
      std::vector<mojom::UserScriptWorldInfoPtr> info) override {}
  void ClearUserScriptWorldConfig(
      const ExtensionId& extension_id,
      const std::optional<std::string>& world_id) override {}
  void ShouldSuspend(ShouldSuspendCallback callback) override {
    std::move(callback).Run();
  }
  void TransferBlobs(TransferBlobsCallback callback) override {
    std::move(callback).Run();
  }
  void UpdatePermissions(const ExtensionId& extension_id,
                         PermissionSet active_permissions,
                         PermissionSet withheld_permissions,
                         URLPatternSet policy_blocked_hosts,
                         URLPatternSet policy_allowed_hosts,
                         bool uses_default_policy_host_restrictions) override {}
  void UpdateDefaultPolicyHostRestrictions(
      URLPatternSet default_policy_blocked_hosts,
      URLPatternSet default_policy_allowed_hosts) override {}
  void UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                  URLPatternSet user_allowed_hosts) override {}
  void UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                    URLPatternSet new_hosts,
                                    int tab_id,
                                    bool update_origin_allowlist) override {}
  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id) override {
    updated_processes_.insert(renderer_receivers_.current_context());
  }
  void ClearTabSpecificPermissions(
      const std::vector<ExtensionId>& extension_ids,
      int tab_id,
      bool update_origin_allowlist) override {}
  void WatchPages(const std::vector<std::string>& css_selectors) override {}

  std::set<content::RenderProcessHost*> updated_processes_;
  mojo::AssociatedReceiverSet<mojom::Renderer, content::RenderProcessHost*>
      renderer_receivers_;
};

class UserScriptLoaderUnitTest : public ExtensionsTest {
 public:
  UserScriptLoaderUnitTest() = default;
  UserScriptLoaderUnitTest(const UserScriptLoaderUnitTest&) = delete;
  UserScriptLoaderUnitTest& operator=(const UserScriptLoaderUnitTest&) = delete;
  ~UserScriptLoaderUnitTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    helper_ = static_cast<TestRendererStartupHelper*>(
        RendererStartupHelperFactory::GetInstance()->SetTestingFactoryAndUse(
            browser_context(),
            base::BindRepeating(&TestRendererStartupHelper::Build)));
  }

  void TearDown() override {
    helper_ = nullptr;
    ExtensionsTest::TearDown();
  }

 protected:
  TestRendererStartupHelper* helper() { return helper_; }

  std::unique_ptr<content::MockRenderProcessHost> CreateAndInitializeProcess(
      bool is_for_guests_only) {
    auto process = std::make_unique<content::MockRenderProcessHost>(
        browser_context(), is_for_guests_only);
    helper_->OnRenderProcessHostCreated(process.get());
    if (is_for_guests_only) {
      helper_->OnRenderProcessLaunched(process.get());
    }
    return process;
  }

  void LoadScriptsAndWait(
      UserScriptLoader* loader,
      content::RenderProcessHost* embedder_process,
      std::string script_id = UserScript::GenerateUserScriptID()) {
    auto script = std::make_unique<UserScript>();
    script->set_id(script_id);
    script->set_host_id(loader->host_id());
    auto content = UserScript::Content::CreateInlineCode(
        GURL("https://embedder.example/inline.js"));
    content->set_content("/* content script body */");
    script->js_scripts().push_back(std::move(content));

    UserScriptList scripts;
    scripts.push_back(std::move(script));

    base::RunLoop run_loop;
    loader->AddScripts(
        std::move(scripts), embedder_process->GetDeprecatedID(),
        /*render_frame_id=*/0,
        base::BindLambdaForTesting(
            [&run_loop](UserScriptLoader*,
                        const std::optional<std::string>& error) {
              EXPECT_FALSE(error.has_value()) << *error;
              run_loop.Quit();
            }));
    run_loop.Run();
    // Flush any pending mojo messages so that UpdateUserScripts is delivered.
    helper_->FlushAllForTesting();
    base::RunLoop().RunUntilIdle();
  }

 private:
  raw_ptr<TestRendererStartupHelper> helper_ = nullptr;
};

// Test success case that embedder content script is sent to
// guest renderer.
TEST_F(UserScriptLoaderUnitTest, EmbedderScriptsSentToGuestRenderer) {
  std::unique_ptr<content::MockRenderProcessHost> guest_process =
      CreateAndInitializeProcess(/*is_for_guests_only=*/true);
  ASSERT_TRUE(helper()->IsProcessInitializedForTesting(guest_process.get()));

  const std::string owner_host = "isolated-app://embedder.example";
  const std::string script_id = UserScript::GenerateUserScriptID();
  WebViewRendererState::WebViewInfo web_view_info;
  web_view_info.owner_host = owner_host;
  web_view_info.content_script_ids.insert(script_id);
  const int dummy_routing_id = 1;
  WebViewRendererState::GetInstance()->AddGuestForTesting(
      guest_process->GetDeprecatedID(), dummy_routing_id, web_view_info);
  base::ScopedClosureRunner cleanup_guest(base::BindOnce(
      [](int process_id, int routing_id) {
        WebViewRendererState::GetInstance()->RemoveGuestForTesting(process_id,
                                                                   routing_id);
      },
      guest_process->GetDeprecatedID(), dummy_routing_id));

  EmbedderUserScriptLoader loader(
      browser_context(),
      mojom::HostID(mojom::HostID::HostType::kControlledFrameEmbedder,
                    owner_host));
  LoadScriptsAndWait(&loader, guest_process.get(), script_id);

  EXPECT_TRUE(loader.initial_load_complete());
  EXPECT_TRUE(helper()->ProcessReceivedUpdateUserScripts(guest_process.get()));
}

// Content scripts that an embedder adds to its own guests
// only ever inject into those guests, so they should not be shipped to
// unrelated renderer processes hosting ordinary web content in the same
// profile.
TEST_F(UserScriptLoaderUnitTest, EmbedderScriptsNotSentToNonGuestRenderer) {
  const struct {
    mojom::HostID::HostType host_type;
    const char* host;
  } test_cases[] = {
      {mojom::HostID::HostType::kControlledFrameEmbedder,
       "isolated-app://embedder.example"},
      {mojom::HostID::HostType::kWebUi, "chrome://embedder.example/"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "host=" << test_case.host);

    std::unique_ptr<content::MockRenderProcessHost> web_process =
        CreateAndInitializeProcess(/*is_for_guests_only=*/false);
    ASSERT_TRUE(helper()->IsProcessInitializedForTesting(web_process.get()));

    EmbedderUserScriptLoader loader(
        browser_context(), mojom::HostID(test_case.host_type, test_case.host));
    LoadScriptsAndWait(&loader, web_process.get());

    EXPECT_TRUE(loader.initial_load_complete());
    EXPECT_FALSE(helper()->ProcessReceivedUpdateUserScripts(web_process.get()));
  }
}

// Test that an embedder content script is NOT sent to a guest renderer
// if that guest's ID is not associated with the script.
// This prevents cross-guest script leaks in Controlled Frame.
TEST_F(UserScriptLoaderUnitTest,
       EmbedderScriptsNotSentToUnrelatedGuestProcess) {
  std::unique_ptr<content::MockRenderProcessHost> guest_process =
      CreateAndInitializeProcess(/*is_for_guests_only=*/true);
  ASSERT_TRUE(helper()->IsProcessInitializedForTesting(guest_process.get()));

  const std::string owner_host = "isolated-app://embedder.example";
  // Deliberately do NOT add the script_id to web_view_info.content_script_ids.
  // This simulates a sibling guest process that shouldn't receive the script.
  WebViewRendererState::WebViewInfo web_view_info;
  web_view_info.owner_host = owner_host;
  const int dummy_routing_id = 1;
  WebViewRendererState::GetInstance()->AddGuestForTesting(
      guest_process->GetDeprecatedID(), dummy_routing_id, web_view_info);
  base::ScopedClosureRunner cleanup_guest(base::BindOnce(
      [](int process_id, int routing_id) {
        WebViewRendererState::GetInstance()->RemoveGuestForTesting(process_id,
                                                                   routing_id);
      },
      guest_process->GetDeprecatedID(), dummy_routing_id));

  EmbedderUserScriptLoader loader(
      browser_context(),
      mojom::HostID(mojom::HostID::HostType::kControlledFrameEmbedder,
                    owner_host));
  LoadScriptsAndWait(&loader, guest_process.get());

  EXPECT_TRUE(loader.initial_load_complete());
  EXPECT_FALSE(helper()->ProcessReceivedUpdateUserScripts(guest_process.get()));
}

}  // namespace

}  // namespace extensions
