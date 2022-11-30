// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/native_app_window.h"

namespace extensions {

namespace {

typedef PlatformAppBrowserTest AppWindowBrowserTest;

// This test is disabled on Linux because of the unpredictable nature of native
// windows. We cannot assume that the window manager will insert any title bar
// at all, so the test may fail on certain window managers.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_FrameInsetsForDefaultFrame DISABLED_FrameInsetsForDefaultFrame
#else
#define MAYBE_FrameInsetsForDefaultFrame FrameInsetsForDefaultFrame
#endif

// Verifies that the NativeAppWindows implement GetFrameInsets() correctly.
// See http://crbug.com/346115
IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, MAYBE_FrameInsetsForDefaultFrame) {
  AppWindow* app_window = CreateTestAppWindow("{}");
  NativeAppWindow* native_window = app_window->GetBaseWindow();
  gfx::Insets insets = native_window->GetFrameInsets();

  // It is a reasonable assumption that the top padding must be greater than
  // the bottom padding due to the title bar.
  EXPECT_GT(insets.top(), insets.bottom());

  CloseAppWindow(app_window);
}

// Verifies that the NativeAppWindows implement GetFrameInsets() correctly.
// See http://crbug.com/346115
IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, FrameInsetsForColoredFrame) {
  AppWindow* app_window =
      CreateTestAppWindow("{ \"frame\": { \"color\": \"#ffffff\" } }");
  NativeAppWindow* native_window = app_window->GetBaseWindow();
  gfx::Insets insets = native_window->GetFrameInsets();

  // It is a reasonable assumption that the top padding must be greater than
  // the bottom padding due to the title bar.
  EXPECT_GT(insets.top(), insets.bottom());

  CloseAppWindow(app_window);
}

// Verifies that the NativeAppWindows implement GetFrameInsets() correctly for
// frameless windows.
IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, FrameInsetsForNoFrame) {
  AppWindow* app_window = CreateTestAppWindow("{ \"frame\": \"none\" }");
  NativeAppWindow* native_window = app_window->GetBaseWindow();
  gfx::Insets insets = native_window->GetFrameInsets();

  // All insets must be zero.
  EXPECT_EQ(0, insets.top());
  EXPECT_EQ(0, insets.bottom());
  EXPECT_EQ(0, insets.left());
  EXPECT_EQ(0, insets.right());

  CloseAppWindow(app_window);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, ShouldShowStaleContentOnEviction) {
  AppWindow* app_window = CreateTestAppWindow("{}");
  // Make sure that a surface gets embedded in the frame evictor of the
  // DelegateFrameHost.
  app_window->Show(AppWindow::SHOW_ACTIVE);
  ASSERT_TRUE(app_window->web_contents());
  content::WaitForResizeComplete(app_window->web_contents());

  // Make sure the renderer submits at least one frame before we test frame
  // eviction.
  content::RenderFrameSubmissionObserver submission_observer(
      app_window->web_contents());
  if (!submission_observer.render_frame_count())
    submission_observer.WaitForAnyFrameSubmission();

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  content::VerifyStaleContentOnFrameEviction(
      app_window->web_contents()->GetRenderWidgetHostView());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

}  // namespace extensions
