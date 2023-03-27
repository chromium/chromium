// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/shell/test/shell_apitest.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"  // nogncheck
#endif

namespace extensions {

// Test that we can load an extension.
IN_PROC_BROWSER_TEST_F(ShellApiTest, LoadExtension) {
  ASSERT_TRUE(RunExtensionTest("extension")) << message_;
}

// Test that we can open an app window and wait for it to load.
IN_PROC_BROWSER_TEST_F(ShellApiTest, LoadApp) {
  ASSERT_TRUE(RunAppTest("platform_app")) << message_;

  // A window was created.
  AppWindow* app_window =
      AppWindowRegistry::Get(browser_context())->app_windows().front();
  ASSERT_TRUE(app_window);

  // TODO(yoz): Test for focus on Cocoa.
  // app_window->GetBaseWindow()->IsActive() is possible, although on Mac,
  // focus changes are asynchronous, so interactive_ui_tests are required.
#if defined(USE_AURA)
  // The web contents have focus.
  EXPECT_TRUE(app_window->web_contents()->GetContentNativeView()->HasFocus());
#endif
}

}  // namespace extensions
