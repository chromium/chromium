// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/stl_util.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

class RendererStartupHelperTest : public ExtensionsTest {
 public:
  RendererStartupHelperTest() {}
  ~RendererStartupHelperTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    helper_ = std::make_unique<RendererStartupHelper>(browser_context());
    registry_ =
        ExtensionRegistryFactory::GetForBrowserContext(browser_context());
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    incognito_render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(incognito_context());
    extension_ = CreateExtension("ext_1");
  }

  void TearDown() override {
    render_process_host_.reset();
    incognito_render_process_host_.reset();
    helper_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  void SimulateRenderProcessCreated(content::RenderProcessHost* rph) {
    helper_->OnRenderProcessHostCreated(rph);
  }

  void SimulateRenderProcessTerminated(content::RenderProcessHost* rph) {
    helper_->RenderProcessHostDestroyed(rph);
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& id_input) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "extension")
            .Set("description", "an extension")
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  scoped_refptr<const Extension> CreateTheme(const std::string& id_input) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "theme")
            .Set("description", "a theme")
            .Set("theme", DictionaryBuilder().Build())
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  scoped_refptr<const Extension> CreatePlatformApp(
      const std::string& id_input) {
    std::unique_ptr<base::Value> background =
        DictionaryBuilder()
            .Set("scripts", ListBuilder().Append("background.js").Build())
            .Build();
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "platform_app")
            .Set("description", "a platform app")
            .Set("app", DictionaryBuilder()
                            .Set("background", std::move(background))
                            .Build())
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  void AddExtensionToRegistry(scoped_refptr<const Extension> extension) {
    registry_->AddEnabled(extension);
  }

  void RemoveExtensionFromRegistry(scoped_refptr<const Extension> extension) {
    registry_->RemoveEnabled(extension->id());
  }

  bool IsProcessInitialized(content::RenderProcessHost* rph) {
    return base::Contains(helper_->initialized_processes_, rph);
  }

  bool IsExtensionLoaded(const Extension& extension) {
    return base::Contains(helper_->extension_process_map_, extension.id());
  }

  bool IsExtensionLoadedInProcess(const Extension& extension,
                                  content::RenderProcessHost* rph) {
    return IsExtensionLoaded(extension) &&
           base::Contains(helper_->extension_process_map_[extension.id()], rph);
  }

  bool IsExtensionPendingActivationInProcess(const Extension& extension,
                                             content::RenderProcessHost* rph) {
    return base::Contains(helper_->pending_active_extensions_, rph) &&
           base::Contains(helper_->pending_active_extensions_[rph],
                          extension.id());
  }

  std::unique_ptr<RendererStartupHelper> helper_;
  ExtensionRegistry* registry_;  // Weak.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  std::unique_ptr<content::MockRenderProcessHost>
      incognito_render_process_host_;
  scoped_refptr<const Extension> extension_;

 private:
  scoped_refptr<const Extension> CreateExtension(
      const std::string& id_input,
      std::unique_ptr<base::DictionaryValue> manifest) {
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(crx_file::id_util::GenerateId(id_input))
        .Build();
  }

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelperTest);
};

// Tests extension loading, unloading and activation and render process creation
// and termination.
TEST_F(RendererStartupHelperTest, NormalExtensionLifecycle) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(0)->type());

  // Activate extension.
  sink.ClearMessages();
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_ActivateExtension::ID),
            sink.GetMessageAt(0)->type());

  // Disable extension.
  sink.ClearMessages();
  RemoveExtensionFromRegistry(extension_);
  helper_->OnExtensionUnloaded(*extension_);
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Unloaded::ID),
            sink.GetMessageAt(0)->type());

  // Extension enabled again.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(0)->type());

  // Render Process terminated.
  SimulateRenderProcessTerminated(render_process_host_.get());
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that activating an extension in an uninitialized render process works
// fine.
TEST_F(RendererStartupHelperTest, EnabledBeforeProcessInitialized) {
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension. The render process isn't initialized yet, so the
  // extension should be added to the list of extensions awaiting activation.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_TRUE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));

  // Initialize the render process.
  SimulateRenderProcessCreated(render_process_host_.get());
  // The renderer would have been sent multiple initialization messages
  // including the loading and activation messages for the extension.
  EXPECT_LE(2u, sink.message_count());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that themes aren't loaded in a renderer process.
TEST_F(RendererStartupHelperTest, LoadTheme) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  scoped_refptr<const Extension> extension(CreateTheme("theme"));
  ASSERT_TRUE(extension->is_theme());

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable the theme. Verify it isn't loaded and activated in the renderer.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension, render_process_host_.get()));

  helper_->ActivateExtensionInProcess(*extension, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension, render_process_host_.get()));

  helper_->OnExtensionUnloaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionLoaded(*extension));
}

// Tests that only incognito-enabled extensions are loaded in an incognito
// context.
TEST_F(RendererStartupHelperTest, ExtensionInIncognitoRenderer) {
  // Initialize the incognito renderer.
  EXPECT_FALSE(IsProcessInitialized(incognito_render_process_host_.get()));
  SimulateRenderProcessCreated(incognito_render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(incognito_render_process_host_.get()));

  IPC::TestSink& sink = render_process_host_->sink();
  IPC::TestSink& incognito_sink = incognito_render_process_host_->sink();

  // Enable the extension. It should not be loaded in the initialized incognito
  // renderer.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension_->id(), browser_context()));
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_EQ(0u, incognito_sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(IsExtensionLoadedInProcess(
      *extension_, incognito_render_process_host_.get()));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));

  // Initialize the normal renderer. The extension should get loaded in it.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionLoadedInProcess(
      *extension_, incognito_render_process_host_.get()));
  // Multiple initialization messages including the extension load message
  // should be dispatched to the non-incognito renderer.
  EXPECT_LE(1u, sink.message_count());
  EXPECT_EQ(0u, incognito_sink.message_count());

  // Enable the extension in incognito mode. This will reload the extension.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(extension_->id(), true);
  helper_->OnExtensionUnloaded(*extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(IsExtensionLoadedInProcess(*extension_,
                                         incognito_render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  // The extension would not have been unloaded from the incognito renderer
  // since it wasn't loaded.
  ASSERT_EQ(1u, incognito_sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            incognito_sink.GetMessageAt(0)->type());
  // The extension would be first unloaded and then loaded from the normal
  // renderer.
  ASSERT_EQ(2u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Unloaded::ID),
            sink.GetMessageAt(0)->type());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(1)->type());
}

// Tests that platform apps are always loaded in an incognito renderer.
TEST_F(RendererStartupHelperTest, PlatformAppInIncognitoRenderer) {
  // Initialize the incognito renderer.
  EXPECT_FALSE(IsProcessInitialized(incognito_render_process_host_.get()));
  SimulateRenderProcessCreated(incognito_render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(incognito_render_process_host_.get()));

  IPC::TestSink& incognito_sink = incognito_render_process_host_->sink();

  scoped_refptr<const Extension> platform_app(
      CreatePlatformApp("platform_app"));
  ASSERT_TRUE(platform_app->is_platform_app());
  EXPECT_FALSE(util::IsIncognitoEnabled(platform_app->id(), browser_context()));
  EXPECT_FALSE(util::CanBeIncognitoEnabled(platform_app.get()));

  // Enable the app. It should get loaded in the incognito renderer even though
  // IsIncognitoEnabled returns false for it, since it can't be enabled for
  // incognito.
  incognito_sink.ClearMessages();
  AddExtensionToRegistry(platform_app);
  helper_->OnExtensionLoaded(*platform_app);
  EXPECT_TRUE(IsExtensionLoadedInProcess(*platform_app,
                                         incognito_render_process_host_.get()));
  ASSERT_EQ(1u, incognito_sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            incognito_sink.GetMessageAt(0)->type());
}

}  // namespace extensions
