// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

// Tests that spin up the ShellDesktopControllerAura and run async tasks like
// launching and reloading apps.
class ShellDesktopControllerAuraBrowserTest : public ShellApiTest {
 public:
  ShellDesktopControllerAuraBrowserTest() = default;
  ~ShellDesktopControllerAuraBrowserTest() override = default;

  // Loads and launches a platform app that opens an app window.
  void LoadAndLaunchApp() {
    ASSERT_FALSE(app_);
    app_ = LoadApp("platform_app");
    ASSERT_TRUE(app_);

    // Wait for app window to load.
    ResultCatcher catcher;
    EXPECT_TRUE(catcher.GetNextResult());

    // A window was created.
    EXPECT_EQ(1u,
              AppWindowRegistry::Get(browser_context())->app_windows().size());
  }

 protected:
  // Returns an open app window.
  AppWindow* GetAppWindow() {
    EXPECT_GT(AppWindowRegistry::Get(browser_context())->app_windows().size(),
              0u);
    return AppWindowRegistry::Get(browser_context())->app_windows().front();
  }

  // ShellApiTest:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    desktop_controller_ =
        static_cast<ShellDesktopControllerAura*>(DesktopController::instance());
    ASSERT_TRUE(desktop_controller_);
  }

  void TearDownOnMainThread() override {
    EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
    ShellApiTest::TearDownOnMainThread();
  }

  ShellDesktopControllerAura* desktop_controller_ = nullptr;
  scoped_refptr<const Extension> app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAuraBrowserTest);
};

// Test that closing the app window stops the DesktopController.
IN_PROC_BROWSER_TEST_F(ShellDesktopControllerAuraBrowserTest, CloseAppWindow) {
  bool test_succeeded = false;

  // Post a task so everything runs after the DesktopController starts.
  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE,
      // Asynchronously launch the app.
      base::BindOnce(&ShellDesktopControllerAuraBrowserTest::LoadAndLaunchApp,
                     base::Unretained(this)),

      // Once the app launches, run the test.
      base::BindLambdaForTesting([this, &test_succeeded]() {
        // Close the app window so DesktopController quits.
        GetAppWindow()->OnNativeClose();
        test_succeeded = true;
      }));

  // Start DesktopController. It should run until the last app window closes.
  desktop_controller_->Run();
  EXPECT_TRUE(test_succeeded)
      << "DesktopController quit before test completed.";
}

// Test that the DesktopController runs until all app windows close.
IN_PROC_BROWSER_TEST_F(ShellDesktopControllerAuraBrowserTest, TwoAppWindows) {
  bool test_succeeded = false;

  // Post a task so everything runs after the DesktopController starts.
  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE,
      // Asynchronously launch the app.
      base::BindOnce(&ShellDesktopControllerAuraBrowserTest::LoadAndLaunchApp,
                     base::Unretained(this)),

      // Once the app launches, run the test.
      base::BindLambdaForTesting([this, &test_succeeded]() {
        // Create a second app window.
        ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
            browser_context(), app_->id(),
            "chrome.app.window.create('/hello.html');"));
        ResultCatcher catcher;
        ASSERT_TRUE(catcher.GetNextResult());

        // Close the first app window.
        GetAppWindow()->OnNativeClose();

        // One window is still open, so the DesktopController should still be
        // running. Post a task to close the last window.
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::BindLambdaForTesting([this, &test_succeeded]() {
              GetAppWindow()->OnNativeClose();
              test_succeeded = true;
            }),
            // A regression might cause DesktopController to quit before the
            // last window closes. To ensure we catch this, wait a while before
            // closing the last window. If DesktopController::Run() finishes
            // before we close the last window and update |test_succeeded|, the
            // test fails.
            base::TimeDelta::FromMilliseconds(500));
      }));

  desktop_controller_->Run();
  EXPECT_TRUE(test_succeeded)
      << "DesktopController quit before test completed.";
}

// Test that the DesktopController stays open while an app reloads, even though
// the app window closes.
IN_PROC_BROWSER_TEST_F(ShellDesktopControllerAuraBrowserTest, ReloadApp) {
  bool test_succeeded = false;

  // Post a task so everything runs after the DesktopController starts.
  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE,
      // Asynchronously launch the app.
      base::BindOnce(&ShellDesktopControllerAuraBrowserTest::LoadAndLaunchApp,
                     base::Unretained(this)),

      // Once the app launches, run the test.
      base::BindLambdaForTesting([this, &test_succeeded]() {
        // Reload the app.
        ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
            browser_context(), app_->id(), "chrome.runtime.reload();"));

        // Wait for the app window to re-open.
        ResultCatcher catcher;
        ASSERT_TRUE(catcher.GetNextResult());

        // Close the new window after a delay. DesktopController should remain
        // open until the window closes.
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::BindLambdaForTesting([this, &test_succeeded]() {
              AppWindow* app_window = AppWindowRegistry::Get(browser_context())
                                          ->app_windows()
                                          .front();
              app_window->OnNativeClose();
              test_succeeded = true;
            }),
            base::TimeDelta::FromMilliseconds(500));
      }));

  desktop_controller_->Run();
  EXPECT_TRUE(test_succeeded)
      << "DesktopController quit before test completed.";
}

}  // namespace extensions
