// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

// TODO(crbug.com/40625383): Move the baselines to skia gold for easier
//   rebaselining when all platforms are supported.

// To rebaseline this test on all platforms:
// 1. Run a CQ+1 dry run.
// 2. Click the failing bots for android, windows, mac, and linux.
// 3. Find the failing content_browsertests step.
// 4. Click the "Deterministic failure" link for the failing test case.
// 5. Copy the "Actual pixels" data url and paste into browser.
// 6. Save the image into your chromium checkout in content/test/data/forms/.

namespace content {

class FormControlsBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput(/*force_device_scale_factor=*/1.f);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

    std::string platform_suffix;
#if BUILDFLAG(IS_MAC)
    platform_suffix = "_mac";
#elif BUILDFLAG(IS_WIN)
    platform_suffix = "_win";
#elif BUILDFLAG(IS_LINUX)
    platform_suffix = "_linux";
#elif BUILDFLAG(IS_CHROMEOS)
    platform_suffix = "_chromeos";
#elif BUILDFLAG(IS_ANDROID)
    int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
    if (sdk_int >= base::android::SDK_VERSION_T) {
      platform_suffix = "_android_T";
    } else {
      platform_suffix = "_android";
    }
#elif BUILDFLAG(IS_FUCHSIA)
    platform_suffix = "_fuchsia";
#elif BUILDFLAG(IS_IOS)
    platform_suffix = "_ios";
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

#if BUILDFLAG(IS_APPLE)
    // This fuzzy pixel comparator handles several mac behaviors:
    // - Different font rendering after 10.14
    // - Slight differences in radio and checkbox rendering in 10.15
    // TODO(wangxianzhu): Tighten these parameters.
    auto comparator = cc::FuzzyPixelComparator()
                          .DiscardAlpha()
                          .SetErrorPixelsPercentageLimit(26.f)
                          .SetAvgAbsErrorLimit(20.f)
                          .SetAbsErrorLimit(120);
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || (OS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
    // Different versions of android may have slight differences in rendering.
    // Some versions have more significant differences than others, which are
    // tracked separately in separate baseline image files. The less significant
    // differences are accommodated for with this fuzzy pixel comparator.
    // This also applies to different versions of other OSes.
    auto comparator = cc::FuzzyPixelComparator()
                          .DiscardAlpha()
                          .SetErrorPixelsPercentageLimit(11.f)
                          .SetAvgAbsErrorLimit(5.f)
                          .SetAbsErrorLimit(140);
#else
    cc::AlphaDiscardingExactPixelComparator comparator;
#endif
    EXPECT_TRUE(CompareWebContentsOutputToReference(
        shell()->web_contents(), golden_filepath,
        gfx::Size(screenshot_width, screenshot_height), comparator));
  }

  // Check if the test can run on the current system.
  bool SkipTestForOldAndroidVersions() const {
#if BUILDFLAG(IS_ANDROID)
    // Lower versions of android running on older devices, ex Nexus 5, render
    // form controls with a too large of a difference -- >20% error -- to
    // pixel compare.
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_OREO) {
      return true;
    }
#endif  // BUILDFLAG(IS_ANDROID)
    return false;
  }
};

// Checkbox renders differently on Android x86. crbug.com/1238283
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
#define MAYBE_Checkbox DISABLED_Checkbox
#else
#define MAYBE_Checkbox Checkbox
#endif

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_Checkbox) {
  if (SkipTestForOldAndroidVersions())
    return;

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

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Radio) {
  if (SkipTestForOldAndroidVersions())
    return;

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

#if BUILDFLAG(IS_MAC)
#define MAYBE_DarkModeTextSelection DISABLED_DarkModeTextSelection
#else
#define MAYBE_DarkModeTextSelection DarkModeTextSelection
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_DarkModeTextSelection) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_dark_mode_text_selection",
          "<meta name=\"color-scheme\" content=\"dark\">"
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

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Input) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_input",
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

#if (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS))
#define MAYBE_Textarea DISABLED_Textarea
#else
#define MAYBE_Textarea Textarea
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_Textarea) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_textarea",
          R"HTML(
           <style>
             body {margin: 8px} textarea {width: 150px; margin-bottom: 18px}
           </style>
           <textarea></textarea><br>
           <textarea style="border: 3px solid lime"></textarea><br>
           <!-- shadow -->
           <textarea style="box-shadow: 4px 4px 10px rgba(255,0,0,0.5),
            inset 4px 4px 4px rgba(0,255,0,0.5);"></textarea><br>
           <!-- disabled -->
           <textarea disabled></textarea>)HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 260);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Button) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_button",
          R"HTML(
            <style>body {margin: 8px} input {margin-bottom: 18px;}</style>
            <input type="button" value="button"/><br>
            <input type="submit" /><br>
            <input type="reset" /><br>
            <input type="file" /><br>
            <!-- border -->
            <input type="button" value="button"
             style="border: 3px solid lime;"/><br>
            <!-- shadow -->
            <input type="button" value="button"
             style="box-shadow: 4px 4px 10px
             rgba(255,0,0,0.5), inset 4px 4px 4px rgba(0,255,0,0.5);"/><br>
            <!-- disabled -->
            <input type="button" value="button" disabled/>)HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 300);
}

// TODO(crbug.com/1160104/#25) This test creates large average_error_rate on
// Android FYI SkiaRenderer Vulkan. Disable it until a resolution for is
// found.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ColorInput DISABLED_ColorInput
#else
#define MAYBE_ColorInput ColorInput
#endif
IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MAYBE_ColorInput) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_color_input",
          R"HTML(
            <style>body {margin: 8px} input {margin-bottom: 18px;}</style>
            <input type="color" /><br>
            <input type="color" value='%2300ff00' /><br>
            <input type="color" list /><br>
            <!-- border -->
            <input type="color" value="%2300ff00"
             style="border: 3px solid lime;"/><br>
            <!-- disabled -->
            <input type="color" disabled/>)HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 250);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Select) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_select",
          R"HTML(
          <style>
              body {margin: 8px}
              select {margin-bottom: 18px;  width: 170px;}
          </style>
          <select></select><br>
          <select style="color:darkturquoise"></select><br>
          <!-- border -->
          <select style="border: 3px solid lime;"></select><br>
          <!-- shadow -->
          <select style="box-shadow: 4px 4px 10px rgba(255,0,0,0.5),
           inset 4px 4px 4px rgba(0,255,0,0.5);"></select><br>
          <!-- disabled -->
          <select disabled></select><br>)HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 200);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, MultiSelect) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_multi_select",
          R"HTML(
            <style>
              body {margin: 8px}
              select {margin-bottom: 18px; width: 170px; }
            </style>
            <select multiple autofocus size=5>
             <optgroup label="unstyled select"></optgroup>
          </select> <br>
          <!-- border -->
          <select multiple style="border: 3px solid lime;" size=5>
            <optgroup label="thick lime border"></optgroup>
          </select><br>
          <!-- disabled -->
          <select multiple disabled size=5>
            <optgroup label="disabled select">
            </optgroup>
          </select>)HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 330);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Progress) {
  if (SkipTestForOldAndroidVersions())
    return;

#if BUILDFLAG(IS_MAC) && !defined(ARCH_CPU_ARM64)
  // The pixel comparison fails on Mac Intel GPUs with Graphite due to MSAA
  // issues.
  // TODO(crbug.com/40940637): Re-enable test if possible.
  if (features::IsSkiaGraphiteEnabled(base::CommandLine::ForCurrentProcess())) {
    return;
  }
#endif

  RunTest("form_controls_browsertest_progress",
          R"HTML(
            <style>
              body {margin: 8px} progress {margin-bottom: 18px}
            </style>
            <progress max="100" value="0"></progress><br>
            <progress max="100" value="5"></progress><br>
            <progress max="100" value="25"></progress><br><br>
            <progress max="100" value="50"></progress><br><br>
            <progress max="100" value="100"></progress><br><br>
            <progress max="100" value="50" style="height:30px"></progress>
          )HTML",
          /* screenshot_width */ 200,
          /* screenshot_height */ 300);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Meter) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_meter",
          R"HTML(
            <style>
              body {margin: 8px} meter {margin-bottom: 10px}
            </style>
            <meter min="0" max="100" low="33" high="66" optimum="100"
              value="20"></meter><br>
            <meter min="0" max="100" low="33" high="66" optimum="100"
             value="50"></meter><br>
            <meter min="0" max="100" low="33" high="66" optimum="100"
              value="66"></meter><br>
            <meter min="0" max="100" low="33" high="66" optimum="100"
             value="90"></meter><br>
            <!-- border -->
            <meter style="border-color: %23000000; border-style: solid;
              border-width: 5px;" min="0" max="100" low="30" high="60"
              optimum="100" value="80" ></meter><br>
            <meter style="box-shadow: 4px 4px 10px rgba(255,0,0,0.5),
            inset 4px 4px 4px rgba(0,255,0,0.5);"></meter>)HTML",
          /* screenshot_width */ 150,
          /* screenshot_height */ 200);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Range) {
  if (SkipTestForOldAndroidVersions())
    return;

  RunTest("form_controls_browsertest_range",
          R"HTML(
            <style>
              body {margin: 8px} input {margin-bottom: 18px}
            </style>
            <input type="range"><br>
           )HTML",
          /* screenshot_width */ 150,
          /* screenshot_height */ 150);
}

// TODO(jarhar): Add tests for other elements from
//   https://concrete-hardboard.glitch.me

}  // namespace content
