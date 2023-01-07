// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_keep_alive_requester.h"

#include <memory>

#include "apps/app_lifetime_monitor_factory.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ShellKeepAliveRequesterTest : public ExtensionsTest {
 public:
  ShellKeepAliveRequesterTest(const ShellKeepAliveRequesterTest&) = delete;
  ShellKeepAliveRequesterTest& operator=(const ShellKeepAliveRequesterTest&) =
      delete;

 protected:
  ShellKeepAliveRequesterTest() = default;
  ~ShellKeepAliveRequesterTest() override = default;

  void SetUp() override {
    // Register factory so it's created with the BrowserContext.
    apps::AppLifetimeMonitorFactory::GetInstance();

    ExtensionsTest::SetUp();

    keep_alive_requester_ =
        std::make_unique<ShellKeepAliveRequester>(browser_context());
  }

  void TearDown() override {
    keep_alive_requester_.reset();

    ExtensionsTest::TearDown();
  }

 protected:
  std::unique_ptr<ShellKeepAliveRequester> keep_alive_requester_;
};

// Tests with an extension.
TEST_F(ShellKeepAliveRequesterTest, Extension) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension", ExtensionBuilder::Type::EXTENSION).Build();

  // No keep-alive is used for extensions that aren't platform apps.
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  ExtensionPrefs::Get(browser_context())
      ->AddDisableReason(extension->id(), disable_reason::DISABLE_RELOAD);
  keep_alive_requester_->OnExtensionUnloaded(browser_context(), extension.get(),
                                             UnloadedExtensionReason::DISABLE);
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app.
TEST_F(ShellKeepAliveRequesterTest, PlatformApp) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // No keep-alives are registered if the extension stops running.
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app that doesn't open a window.
TEST_F(ShellKeepAliveRequesterTest, PlatformAppNoWindow) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Eventually, the app's background host is destroyed.
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app that is reloaded.
TEST_F(ShellKeepAliveRequesterTest, PlatformAppReload) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Disable the app for a reload.
  keep_alive_requester_->StartTrackingReload(extension.get());
  ExtensionPrefs::Get(browser_context())
      ->AddDisableReason(extension->id(), disable_reason::DISABLE_RELOAD);
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  keep_alive_requester_->OnExtensionUnloaded(browser_context(), extension.get(),
                                             UnloadedExtensionReason::DISABLE);

  // Expect a keep-alive while waiting for the app to finish reloading.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());
  keep_alive_requester_->StopTrackingReload(extension->id());

  // Expect a keep-alive while waiting for the app to launch a window again.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app that is reloaded, but fails to load.
TEST_F(ShellKeepAliveRequesterTest, PlatformAppReloadFailure) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Disable the app for a reload.
  keep_alive_requester_->StartTrackingReload(extension.get());
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  ExtensionPrefs::Get(browser_context())
      ->AddDisableReason(extension->id(), disable_reason::DISABLE_RELOAD);
  keep_alive_requester_->OnExtensionUnloaded(browser_context(), extension.get(),
                                             UnloadedExtensionReason::DISABLE);

  // Expect a keep-alive while waiting for the app to finish reloading that is
  // removed when the app fails to load.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->StopTrackingReload(extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app that reloads before opening a window.
TEST_F(ShellKeepAliveRequesterTest, PlatformAppNoWindowReload) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Disable the app for a reload.
  keep_alive_requester_->StartTrackingReload(extension.get());
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  ExtensionPrefs::Get(browser_context())
      ->AddDisableReason(extension->id(), disable_reason::DISABLE_RELOAD);
  keep_alive_requester_->OnExtensionUnloaded(browser_context(), extension.get(),
                                             UnloadedExtensionReason::DISABLE);

  // Expect a keep-alive while waiting for the app to finish reloading.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());
  keep_alive_requester_->StopTrackingReload(extension->id());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

// Tests with a platform app that is reloaded, but doesn't open a window again.
TEST_F(ShellKeepAliveRequesterTest, PlatformAppReloadNoWindow) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("platform_app", ExtensionBuilder::Type::PLATFORM_APP)
          .Build();
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());

  // Expect a keep-alive while waiting for the app to launch a window.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnAppActivated(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Disable the app for a reload.
  keep_alive_requester_->StartTrackingReload(extension.get());
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  ExtensionPrefs::Get(browser_context())
      ->AddDisableReason(extension->id(), disable_reason::DISABLE_RELOAD);
  keep_alive_requester_->OnExtensionUnloaded(browser_context(), extension.get(),
                                             UnloadedExtensionReason::DISABLE);

  // Expect a keep-alive while waiting for the app to finish reloading.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
  keep_alive_requester_->OnExtensionLoaded(browser_context(), extension.get());
  keep_alive_requester_->StopTrackingReload(extension->id());

  // Expect a keep-alive while waiting for the app to launch a window again.
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

  // Eventually the app stops.
  keep_alive_requester_->OnAppStop(browser_context(), extension->id());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
}

}  // namespace extensions
