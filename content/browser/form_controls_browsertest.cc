// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// TODO(crbug.com/958242): Move the baselines to skia gold for easier
//   rebaselining when all platforms are supported.

// To rebaseline this test on all platforms:
// 1. Run a CQ+1 dry run.
// 2. Click the failing bots for android, windows, mac, and linux.
// 3. Find the failing interactive_ui_browsertests step.
// 4. Click the "Deterministic failure" link for the failing test case.
// 5. Copy the "Actual pixels" data url and paste into browser.
// 6. Save the image into your chromium checkout in content/test/data/forms/.

namespace content {

class FormControlsBrowserTest : public ContentBrowserTest {
 public:
  FormControlsBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kFormControlsRefresh, features::kCSSColorSchemeUARendering},
        {});
  }

  void SetUp() override {
    EnablePixelOutput(/*force_device_scale_factor=*/1.f);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    // The --disable-lcd-text flag helps text render more similarly on
    // different bots and platform.
    command_line->AppendSwitch(switches::kDisableLCDText);

    // This is required to allow dark mode to be used on some platforms.
    command_line->AppendSwitch(switches::kForceDarkMode);
  }

  void RunTest(const std::string& screenshot_filename,
               const std::string& body_html,
               int screenshot_width,
               int screenshot_height) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(features::IsFormControlsRefreshEnabled());
    ASSERT_TRUE(features::IsCSSColorSchemeUARenderingEnabled());

    std::string platform_suffix;
#if defined(OS_MAC)
    platform_suffix = "_mac";
#elif defined(OS_WIN)
    platform_suffix = "_win";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
    platform_suffix = "_chromeos";
#elif defined(OS_ANDROID)
    int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
    if (sdk_int == base::android::SDK_VERSION_KITKAT) {
      platform_suffix = "_android_kitkat";
    } else {
      platform_suffix = "_android";
    }
#endif

    base::FilePath dir_test_data;
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
    base::FilePath golden_filepath =
        dir_test_data.AppendASCII("forms").AppendASCII(screenshot_filename +
                                                       ".png");

    base::FilePath golden_filepath_platform =
        golden_filepath.InsertBeforeExtensionASCII(platform_suffix);
    if (base::PathExists(golden_filepath_platform)) {
      golden_filepath = golden_filepath_platform;
    }

    ASSERT_TRUE(
        NavigateToURL(shell()->web_contents(),
                      GURL("data:text/html,<!DOCTYPE html>" + body_html)));

#if defined(OS_MAC)
    // This fuzzy pixel comparator handles several mac behaviors:
    // - Different font rendering after 10.14
    // - 10.12 subpixel rendering differences: crbug.com/1037971
    // - Slight differences in radio and checkbox rendering in 10.15
    cc::FuzzyPixelComparator comparator(
        /* discard_alpha */ true,
        /* error_pixels_percentage_limit */ 11.f,
        /* small_error_pixels_percentage_limit */ 0.f,
        /* avg_abs_error_limit */ 20.f,
        /* max_abs_error_limit */ 79.f,
        /* small_error_threshold */ 0);
#elif defined(OS_ANDROID)
    // Different versions of android may have slight differences in rendering.
    // Some versions have more significant differences than others, which are
    // tracked separately in separate baseline image files. The less significant
    // differences are accommodated for with this fuzzy pixel comparator.
    cc::FuzzyPixelComparator comparator(
        /* discard_alpha */ true,
        /* error_pixels_percentage_limit */ 6.f,
        /* small_error_pixels_percentage_limit */ 0.f,
        /* avg_abs_error_limit */ 2.f,
        /* max_abs_error_limit */ 3.f,
        /* small_error_threshold */ 0);
#else
    cc::ExactPixelComparator comparator(/* disard_alpha */ true);
#endif
    EXPECT_TRUE(CompareWebContentsOutputToReference(
        shell()->web_contents(), golden_filepath,
        gfx::Size(screenshot_width, screenshot_height), comparator));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Disabled due to https://crbug.com/1172370
#if defined(OS_ANDROID)
#define MAYBE_Checkbox DISABLED_Checkbox
#else
#define MAYBE_Checkbox Checkbox
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_Checkbox) {
  RunTest("form_controls_browsertest_checkbox",
          "<input type=checkbox>"
          "<input type=checkbox checked>"
          "<input type=checkbox disabled>"
          "<input type=checkbox checked disabled>"
          "<input type=checkbox id=\"indeterminate\">"
          "<script>"
          "  document.getElementById('indeterminate').indeterminate = true"
          "</script>",
          /* screenshot_width */ 130,
          /* screenshot_height */ 40);
}

// Disabled due to https://crbug.com/1172370
#if defined(OS_ANDROID)
#define MAYBE_Radio DISABLED_Radio
#else
#define MAYBE_Radio Radio
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_Radio) {
  RunTest("form_controls_browsertest_radio",
          "<input type=radio>"
          "<input type=radio checked>"
          "<input type=radio disabled>"
          "<input type=radio checked disabled>"
          "<input type=radio id=\"indeterminate\">"
          "<script>"
          "  document.getElementById('indeterminate').indeterminate = true"
          "</script>",
          /* screenshot_width */ 140,
          /* screenshot_height */ 40);
}

// TODO(crbug.com/1165919): Re-enable test when there is a resolution for
// android-bfcache-rel builder producing different results.
#if defined(OS_ANDROID) || defined(OS_MAC)
#define MAYBE_DarkModeTextSelection DISABLED_DarkModeTextSelection
#else
#define MAYBE_DarkModeTextSelection DarkModeTextSelection
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_DarkModeTextSelection) {
  RunTest("form_controls_browsertest_dark_mode_text_selection",
          "<meta name=\"color-scheme\" content=\"dark\">"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
          "<div id=\"target\">This is some basic text that we are going to "
          "select.</div>"
          "<script>"
          "  let container = document.getElementById('target');"
          "  container.focus();"
          "  let targetText = container.firstChild;"
          "  let selectionRange = window.getSelection();"
          "  selectionRange.setBaseAndExtent(targetText, 5, targetText, 35);"
          "</script>",
          /* screenshot_width */ 400,
          /* screenshot_height */ 40);
}

// TODO(crbug.com/1165919) skip this test until there is a resolution for
// android-bfcache-rel builder producing different results.
#if defined(OS_ANDROID)
#define MAYBE_Input DISABLED_Input
#else
#define MAYBE_Input Input
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_Input) {
  RunTest("form_controls_browsertest_input",
          "<!-- text inputs -->"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
          "<style>body {margin: 8px} input {width: 150px; "
          "margin-bottom: 18px}</style>"
          "<input type=\"text\" /><br>"
          "<input type=\"number\" /><br>"
          "<input type=\"search\" /><br>"
          "<input type=\"email\" /><br>"
          "<input type=\"password\" /><br>"
          "<!-- border -->"
          "<input type=\"text\" style=\"border: 3px solid lime;\"/><br>"
          "<!-- shadow -->"
          "<input type=\"text\" style=\"box-shadow: 4px 4px 10px "
          "rgba(255,0,0,0.5), inset 4px 4px 4px rgba(0,255,0,0.5);\"/><br>"
          "<!-- disabled -->"
          "<input type=\"text\" disabled/>",
          /* screenshot_width */ 200,
          /* screenshot_height */ 330);
}

// TODO(jarhar): Add tests for other elements from
//   https://concrete-hardboard.glitch.me

}  // namespace content
