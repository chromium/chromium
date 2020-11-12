// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace extensions {

using AppWindowApiTest = PlatformAppBrowserTest;
using ExperimentalAppWindowApiTest = ExperimentalPlatformAppBrowserTest;

// Tests chrome.app.window.setIcon.
IN_PROC_BROWSER_TEST_F(ExperimentalAppWindowApiTest, SetIcon) {
  ExtensionTestMessageListener listener("ready", true);

  // Launch the app and wait for it to be ready.
  LoadAndLaunchPlatformApp("windows_api_set_icon", &listener);
  listener.Reply("");

  AppWindow* app_window = GetFirstAppWindow();
  ASSERT_TRUE(app_window);

  // Now wait until the WebContent has decoded the icon and chrome has
  // processed it. This needs to be in a loop since the renderer runs in a
  // different process.
  while (app_window->custom_app_icon().IsEmpty())
    base::RunLoop().RunUntilIdle();

  EXPECT_NE(std::string::npos,
            app_window->app_icon_url().spec().find("icon.png"));
}

// TODO(crbug.com/794771): These fail on Linux with HEADLESS env var set.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_OnMinimizedEvent DISABLED_OnMinimizedEvent
#define MAYBE_OnMaximizedEvent DISABLED_OnMaximizedEvent
#define MAYBE_OnRestoredEvent DISABLED_OnRestoredEvent
#else
#define MAYBE_OnMinimizedEvent OnMinimizedEvent
#define MAYBE_OnMaximizedEvent OnMaximizedEvent
#define MAYBE_OnRestoredEvent OnRestoredEvent
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnMinimizedEvent) {
#if defined(OS_MAC)
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582,
#endif
  EXPECT_TRUE(RunExtensionTestWithArg("platform_apps/windows_api_properties",
                                      "minimized"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnMaximizedEvent) {
#if defined(OS_MAC)
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582,
#endif
  EXPECT_TRUE(RunExtensionTestWithArg("platform_apps/windows_api_properties",
                                      "maximized"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnRestoredEvent) {
#if defined(OS_MAC)
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582,
#endif
  EXPECT_TRUE(RunExtensionTestWithArg("platform_apps/windows_api_properties",
                                      "restored"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, OnBoundsChangedEvent) {
  EXPECT_TRUE(RunExtensionTestWithArg("platform_apps/windows_api_properties",
                                      "boundsChanged"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopWithPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/has_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopWithOldPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/has_old_permissions"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopNoPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, Get) {
  EXPECT_TRUE(RunPlatformAppTest("platform_apps/windows_api_get"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, SetShapeHasPerm) {
  EXPECT_TRUE(
      RunPlatformAppTest("platform_apps/windows_api_shape/has_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, SetShapeNoPerm) {
  EXPECT_TRUE(
      RunPlatformAppTest("platform_apps/windows_api_shape/no_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledHasPermissions) {
  const char kNoAlphaDir[] =
      "platform_apps/windows_api_alpha_enabled/has_permissions_no_alpha";
  const char kHasAlphaDir[] =
      "platform_apps/windows_api_alpha_enabled/has_permissions_has_alpha";
  ALLOW_UNUSED_LOCAL(kHasAlphaDir);
  const char* test_dir = kNoAlphaDir;

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(USE_AURA) && !(defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  test_dir = kHasAlphaDir;

#if defined(OS_WIN)
  if (!ui::win::IsAeroGlassEnabled()) {
    test_dir = kNoAlphaDir;
  }
#endif  // OS_WIN
#endif  // USE_AURA && !(OS_LINUX || IS_CHROMEOS_LACROS)

  EXPECT_TRUE(RunPlatformAppTest(test_dir)) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledNoPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_alpha_enabled/no_permissions"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledInStable) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  EXPECT_TRUE(RunPlatformAppTestWithFlags(
      "platform_apps/windows_api_alpha_enabled/in_stable",
      // Ignore manifest warnings because the extension will not load at all
      // in stable.
      kFlagIgnoreManifestWarnings, kFlagNone))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledWrongFrameType) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_alpha_enabled/wrong_frame_type"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, VisibleOnAllWorkspacesInStable) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_visible_on_all_workspaces/in_stable"))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowHasPermissions) {
  EXPECT_TRUE(RunComponentExtensionTest(
      "platform_apps/windows_api_ime/has_permissions_whitelisted"))
      << message_;

  EXPECT_TRUE(RunPlatformAppTestWithFlags(
      "platform_apps/windows_api_ime/has_permissions_platform_app",
      kFlagIgnoreManifestWarnings, kFlagNone))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowNoPermissions) {
  EXPECT_TRUE(RunComponentExtensionTest(
      "platform_apps/windows_api_ime/no_permissions_whitelisted"))
      << message_;

  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_ime/no_permissions_platform_app"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowNotFullscreen) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kForceAppMode);
  command_line->AppendSwitchASCII(switches::kAppId,
                                  "jkghodnilhceideoidjikpgommlajknk");

  EXPECT_TRUE(RunComponentExtensionTest(
      "platform_apps/windows_api_ime/forced_app_mode_not_fullscreen"))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
