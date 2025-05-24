// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/filename_util.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

using AppWindowApiTest = PlatformAppBrowserTest;
using ExperimentalAppWindowApiTest = ExperimentalPlatformAppBrowserTest;

// Tests chrome.app.window.setIcon.
IN_PROC_BROWSER_TEST_F(ExperimentalAppWindowApiTest, SetIcon) {
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

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

  EXPECT_TRUE(base::Contains(app_window->app_icon_url().spec(), "icon.png"));
}

// TODO(crbug.com/40554643): These fail on Linux with HEADLESS env var set.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_OnMinimizedEvent DISABLED_OnMinimizedEvent
#define MAYBE_OnMaximizedEvent DISABLED_OnMaximizedEvent
#define MAYBE_OnRestoredEvent DISABLED_OnRestoredEvent
#else
#define MAYBE_OnMinimizedEvent OnMinimizedEvent
#define MAYBE_OnMaximizedEvent OnMaximizedEvent
#define MAYBE_OnRestoredEvent OnRestoredEvent
#endif  // BUILDFLAG(IS_LINUX)

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnMinimizedEvent) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_properties",
                               {.custom_arg = "minimized"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnMaximizedEvent) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_properties",
                               {.custom_arg = "maximized"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_OnRestoredEvent) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_properties",
                               {.custom_arg = "restored"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, OnBoundsChangedEvent) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_properties",
                               {.custom_arg = "boundsChanged"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopWithPermissions) {
  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_always_on_top/has_permissions",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopWithOldPermissions) {
  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_always_on_top/has_old_permissions",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlwaysOnTopNoPermissions) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_always_on_top/no_permissions",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, Get) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_get",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, SetShapeHasPerm) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_shape/has_permission",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, SetShapeNoPerm) {
  EXPECT_TRUE(RunExtensionTest("platform_apps/windows_api_shape/no_permission",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Fails on Ozone/X11.  https://crbug.com/1109112
#if BUILDFLAG(IS_OZONE)
#define MAYBE_AlphaEnabledHasPermissions DISABLED_AlphaEnabledHasPermissions
#else
#define MAYBE_AlphaEnabledHasPermissions AlphaEnabledHasPermissions
#endif
IN_PROC_BROWSER_TEST_F(AppWindowApiTest, MAYBE_AlphaEnabledHasPermissions) {
  const char kNoAlphaDir[] =
      "platform_apps/windows_api_alpha_enabled/has_permissions_no_alpha";
  [[maybe_unused]] const char kHasAlphaDir[] =
      "platform_apps/windows_api_alpha_enabled/has_permissions_has_alpha";
  const char* test_dir = kNoAlphaDir;

#if defined(USE_AURA) && !BUILDFLAG(IS_LINUX)
  test_dir = kHasAlphaDir;
#endif  // USE_AURA && !BUILDFLAG(IS_LINUX)

  EXPECT_TRUE(RunExtensionTest(test_dir, {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledNoPermissions) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_alpha_enabled/no_permissions",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledInStable) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_alpha_enabled/in_stable",
                       {.launch_as_platform_app = true},
                       // Ignore manifest warnings because the extension will
                       // not load at all in stable.
                       {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, AlphaEnabledWrongFrameType) {
  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_alpha_enabled/wrong_frame_type",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, CrossOriginIsolation) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/window_api_cross_origin_isolation",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, VisibleOnAllWorkspacesInStable) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_visible_on_all_workspaces/in_stable",
      {.launch_as_platform_app = true}))
      << message_;
}

// Tests that component extensions are allowed to open chrome:-scheme URLs in
// app windows, but not other absolute URLs.
IN_PROC_BROWSER_TEST_F(AppWindowApiTest, OpeningAbsoluteURLs) {
  base::FilePath file_path = test_data_dir_.AppendASCII("body1.html");
  GURL file_url = net::FilePathToFileURL(file_path);
  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_opening_absolute_urls",
      {.custom_arg = file_url.spec().c_str(), .launch_as_platform_app = true},
      {.load_as_component = true}))
      << message_;
  // The app should have successfully opened the chrome://version page.
  AppWindowRegistry::AppWindowList app_windows =
      AppWindowRegistry::Get(profile())->GetAppWindowsForApp(
          last_loaded_extension_id());
  ASSERT_EQ(1u, app_windows.size());
  content::WebContents* app_window_contents =
      (*app_windows.begin())->web_contents();
  EXPECT_EQ(GURL("chrome://version"),
            app_window_contents->GetLastCommittedURL());
  EXPECT_FALSE(app_window_contents->GetPrimaryMainFrame()->IsErrorDocument());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowHasPermissions) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_ime/has_permissions_allowed",
                       {}, {.load_as_component = true}))
      << message_;

  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_ime/has_permissions_platform_app",
      {.launch_as_platform_app = true}, {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowNoPermissions) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_ime/no_permissions_allowed",
                       {}, {.load_as_component = true}))
      << message_;

  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_ime/no_permissions_platform_app",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowApiTest, ImeWindowNotFullscreen) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kForceAppMode);
  command_line->AppendSwitchASCII(switches::kAppId,
                                  "jkghodnilhceideoidjikpgommlajknk");

  EXPECT_TRUE(RunExtensionTest(
      "platform_apps/windows_api_ime/forced_app_mode_not_fullscreen", {},
      {.load_as_component = true}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
