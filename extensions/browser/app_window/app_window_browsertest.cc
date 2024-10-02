// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace extensions {

namespace {

using AppWindowBrowserTest = PlatformAppBrowserTest;

// This test is disabled on Linux because of the unpredictable nature of native
// windows. We cannot assume that the window manager will insert any title bar
// at all, so the test may fail on certain window managers.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX)
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

IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, IncognitoOpenUrl) {
  AppWindow* app_window = CreateTestAppWindow("{}");

  content::WebContents* app_contents =
      app_window->app_window_contents_for_test()->GetWebContents();

  content::OpenURLParams params(GURL(url::kAboutBlankURL), {},
                                WindowOpenDisposition::OFF_THE_RECORD,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* new_contents =
      app_contents->OpenURL(params, /*navigation_handle_callback=*/{});

  Profile* profile =
      Profile::FromBrowserContext(new_contents->GetBrowserContext());
  EXPECT_TRUE(profile->IsOffTheRecord());

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest, DraggableFramelessWindow) {
  AppWindow* app_window = CreateTestAppWindow(R"({ "frame": "none" })");

  base::RunLoop run_loop;
  app_window->SetOnDraggableRegionsChangedForTesting(run_loop.QuitClosure());

  static constexpr char kTestScript[] =
      "window.document.body.style.height = '50px';"
      "window.document.body.style.width = '100px';"
      "window.document.body.style.appRegion = 'drag';";
  content::WebContents* app_contents =
      app_window->app_window_contents_for_test()->GetWebContents();
  EXPECT_TRUE(ExecJs(app_contents->GetPrimaryMainFrame(), kTestScript));

  run_loop.Run();

  NativeAppWindow* native_window = GetNativeAppWindowForAppWindow(app_window);
  SkRegion* draggable_region = native_window->GetDraggableRegion();
  ASSERT_TRUE(draggable_region);
  EXPECT_FALSE(draggable_region->isEmpty());
}

#if BUILDFLAG(IS_CHROMEOS)

// Disabled due to flake. https://crbug.com/1416579
IN_PROC_BROWSER_TEST_F(AppWindowBrowserTest,
                       DISABLED_ShouldShowStaleContentOnEviction) {
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
  if (!submission_observer.render_frame_count()) {
    submission_observer.WaitForAnyFrameSubmission();
  }

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  content::VerifyStaleContentOnFrameEviction(
      app_window->web_contents()->GetRenderWidgetHostView());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

}  // namespace extensions
