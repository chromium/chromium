// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_loader.h"

#include <memory>
#include <string>

#include "apps/app_lifetime_monitor_factory.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "components/crx_file/id_util.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/test_extension_dir.h"

#if defined(USE_AURA)
#include "extensions/browser/app_window/app_window.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "extensions/shell/browser/shell_native_app_window_aura.h"
#include "extensions/shell/test/shell_test_extensions_browser_client.h"
#include "extensions/shell/test/shell_test_helper_aura.h"
#endif

namespace extensions {

namespace OnLaunched = api::app_runtime::OnLaunched;

namespace {

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}

  TestExtensionSystem(const TestExtensionSystem&) = delete;
  TestExtensionSystem& operator=(const TestExtensionSystem&) = delete;

  ~TestExtensionSystem() override = default;

  AppSorting* app_sorting() override { return &app_sorting_; }

 private:
  NullAppSorting app_sorting_;
};

#if defined(USE_AURA)
// An AppWindowClient for use without a DesktopController.
class TestAppWindowClient : public ShellAppWindowClient {
 public:
  TestAppWindowClient() = default;

  TestAppWindowClient(const TestAppWindowClient&) = delete;
  TestAppWindowClient& operator=(const TestAppWindowClient&) = delete;

  ~TestAppWindowClient() override = default;

  // ShellAppWindowClient:
  std::unique_ptr<NativeAppWindow> CreateNativeAppWindow(
      AppWindow* window,
      AppWindow::CreateParams* params) override {
    return std::make_unique<ShellNativeAppWindowAura>(window, *params);
  }
};
#endif

}  // namespace

class ShellExtensionLoaderTest : public ExtensionsTest {
 public:
  ShellExtensionLoaderTest(const ShellExtensionLoaderTest&) = delete;
  ShellExtensionLoaderTest& operator=(const ShellExtensionLoaderTest&) = delete;

 protected:
  ShellExtensionLoaderTest() = default;
  ~ShellExtensionLoaderTest() override = default;

  void SetUp() override {
    // Register factory so it's created with the BrowserContext.
    apps::AppLifetimeMonitorFactory::GetInstance();

    ExtensionsTest::SetUp();
    extensions_browser_client()->set_extension_system_factory(&factory_);
    // ExtensionsTest sets up the ExtensionPrefs, but we still need to attach
    // the PrefService to the browser context.
    user_prefs::UserPrefs::Set(browser_context(), pref_service());
    event_router_ = CreateAndUseTestEventRouter(browser_context());
  }

  void TearDown() override {
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
    event_router_ = nullptr;
    ExtensionsTest::TearDown();
  }

  // Returns the path to a test directory.
  base::FilePath GetExtensionPath(const std::string& dir) {
    base::FilePath test_data_dir;
    base::PathService::Get(DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII(dir);
  }

  // Verifies the extension is correctly created and enabled.
  void ExpectEnabled(const Extension* extension) {
    ASSERT_TRUE(extension);
    EXPECT_EQ(mojom::ManifestLocation::kCommandLine, extension->location());
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
    EXPECT_TRUE(registry->enabled_extensions().Contains(extension->id()));
    EXPECT_FALSE(registry->GetExtensionById(
        extension->id(),
        ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::ENABLED));
  }

  // Verifies the extension with the given ID is disabled.
  void ExpectDisabled(const ExtensionId& extension_id) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
    EXPECT_TRUE(registry->disabled_extensions().Contains(extension_id));
    EXPECT_FALSE(registry->GetExtensionById(
        extension_id,
        ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::DISABLED));
  }

  TestEventRouter* event_router() { return event_router_; }

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;

  raw_ptr<TestEventRouter> event_router_ = nullptr;  // Created in SetUp().
};

// Tests with a non-existent directory.
TEST_F(ShellExtensionLoaderTest, NotFound) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("nonexistent"));
  ASSERT_FALSE(extension);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests loading and reloading an extension.
TEST_F(ShellExtensionLoaderTest, Extension) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("extension"));
  ExpectEnabled(extension);

  // Extensions shouldn't receive the onLaunched event.
  EXPECT_EQ(0, event_router()->GetEventCount(OnLaunched::kEventName));

  // No keep-alives are used for non-app extensions.
  const ExtensionId extension_id = extension->id();
  loader.ReloadExtension(extension->id());
  ExpectDisabled(extension_id);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Wait for load.
  content::RunAllTasksUntilIdle();
  extension = ExtensionRegistry::Get(browser_context())
                  ->GetInstalledExtension(extension_id);
  ExpectEnabled(extension);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Not an app.
  EXPECT_EQ(0, event_router()->GetEventCount(OnLaunched::kEventName));
}

// Tests that the extension is added as enabled even if is disabled in
// ExtensionPrefs. Unlike Chrome, AppShell doesn't have a UI surface for
// re-enabling a disabled extension.
TEST_F(ShellExtensionLoaderTest, LoadAfterReloadFailure) {
  base::FilePath extension_path = GetExtensionPath("extension");
  const ExtensionId extension_id =
      crx_file::id_util::GenerateIdForPath(extension_path);
  ExtensionPrefs::Get(browser_context())
      ->SetExtensionDisabled(extension_id, disable_reason::DISABLE_RELOAD);

  ShellExtensionLoader loader(browser_context());
  const Extension* extension = loader.LoadExtension(extension_path);
  ExpectEnabled(extension);
}

// Tests that the extension is not added if it is disabled in ExtensionPrefs
// for reasons beyond reloading.
TEST_F(ShellExtensionLoaderTest, LoadDisabledExtension) {
  base::FilePath extension_path = GetExtensionPath("extension");
  const ExtensionId extension_id =
      crx_file::id_util::GenerateIdForPath(extension_path);
  ExtensionPrefs::Get(browser_context())
      ->SetExtensionDisabled(
          extension_id,
          disable_reason::DISABLE_RELOAD | disable_reason::DISABLE_USER_ACTION);

  ShellExtensionLoader loader(browser_context());
  const Extension* extension = loader.LoadExtension(extension_path);
  ExpectDisabled(extension->id());
}

#if defined(USE_AURA)
class ShellExtensionLoaderTestAura : public ShellExtensionLoaderTest {
 public:
  ShellExtensionLoaderTestAura(const ShellExtensionLoaderTestAura&) = delete;
  ShellExtensionLoaderTestAura& operator=(const ShellExtensionLoaderTestAura&) =
      delete;

 protected:
  ShellExtensionLoaderTestAura() = default;
  ~ShellExtensionLoaderTestAura() override = default;

  void SetUp() override {
    aura_helper_ = std::make_unique<ShellTestHelperAura>();
    aura_helper_->SetUp();

    std::unique_ptr<TestExtensionsBrowserClient>
        test_extensions_browser_client =
            std::make_unique<ShellTestExtensionsBrowserClient>();
    SetExtensionsBrowserClient(std::move(test_extensions_browser_client));
    AppWindowClient::Set(&app_window_client_);

    ShellExtensionLoaderTest::SetUp();
  }

  void TearDown() override {
    ShellExtensionLoaderTest::TearDown();
    AppWindowClient::Set(nullptr);
    aura_helper_->TearDown();
  }

  // Returns an initialized app window for the extension.
  // The app window deletes itself when its native window is closed.
  AppWindow* CreateAppWindow(const Extension* extension) {
    AppWindow* app_window =
        app_window_client_.CreateAppWindow(browser_context(), extension);
    aura_helper_->InitAppWindow(app_window);
    return app_window;
  }

 private:
  std::unique_ptr<ShellTestHelperAura> aura_helper_;
  TestAppWindowClient app_window_client_;
};

// Tests loading and launching a platform app.
TEST_F(ShellExtensionLoaderTestAura, AppLaunch) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("platform_app"));
  ExpectEnabled(extension);

  // A keep-alive is waiting for the app to launch its first window.
  // (Not strictly necessary in AppShell, because DesktopWindowControllerAura
  // doesn't consider quitting until an already-open window is closed, but
  // ShellExtensionLoader and ShellKeepAliveRequester don't make that
  // assumption.)
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  {
    // When AppShell launches the app window, the keep-alive is removed.
    AppWindow* app_window = CreateAppWindow(extension);
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
    app_window->GetBaseWindow()->Close();  // Deletes |app_window|.
  }
}

// Tests loading, launching and reloading a platform app.
TEST_F(ShellExtensionLoaderTestAura, AppLaunchAndReload) {
  ShellExtensionLoader loader(browser_context());

  const Extension* extension =
      loader.LoadExtension(GetExtensionPath("platform_app"));
  const ExtensionId extension_id = extension->id();
  ExpectEnabled(extension);
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  CreateAppWindow(extension);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Reload the app.
  loader.ReloadExtension(extension->id());
  ExpectDisabled(extension_id);
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Complete the reload. ShellExtensionLoader sends the onLaunched event.
  content::RunAllTasksUntilIdle();
  extension = ExtensionRegistry::Get(browser_context())
                  ->GetInstalledExtension(extension_id);
  ExpectEnabled(extension);
  EXPECT_EQ(1, event_router()->GetEventCount(OnLaunched::kEventName));
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  {
    // The keep-alive is destroyed when an app window is launched.
    AppWindow* app_window = CreateAppWindow(extension);
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

    app_window->GetBaseWindow()->Close();
  }
}

// Tests failing to reload an app.
// TODO(crbug.com/40742257): Flaky on Linux, Lacros, ChromeOS, and similar.
TEST_F(ShellExtensionLoaderTestAura, DISABLED_ReloadFailure) {
  ShellExtensionLoader loader(browser_context());
  ExtensionId extension_id;

  // Create an extension in a temporary directory so we can delete it before
  // trying to reload it.
  {
    TestExtensionDir extension_dir;
    extension_dir.WriteManifest(
        R"({
             "name": "Test Platform App",
             "version": "1",
             "manifest_version": 2,
              "app": {
                "background": {
                  "scripts": ["background.js"]
                }
              }
           })");
    extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");

    const Extension* extension =
        loader.LoadExtension(extension_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    extension_id = extension->id();
    ExpectEnabled(extension);
    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

    // Launch an app window.
    CreateAppWindow(extension);
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

    // Reload the app.
    loader.ReloadExtension(extension->id());
    EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  }

  // Wait for load (which will fail because the directory is missing).
  content::RunAllTasksUntilIdle();

  ExpectDisabled(extension_id);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // We can't reload an extension that is already disabled for reloading, so
  // trying to reload this extension shouldn't result in a dangling keep-alive.
  loader.ReloadExtension(extension_id);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}
#endif

}  // namespace extensions
