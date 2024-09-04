// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "build/build_config.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "url/gurl.h"

namespace content {

namespace {
GURL HtmlAsDataUrl(std::string viewport_content) {
  const std::string placeholder = "viewport_content_placeholder";
  std::string html_with_placeholder = R""""(data:text/html,
    <!DOCTYPE html>
<html>
<head>
<script>
      internals.settings.setViewportEnabled(true);
      internals.settings.setViewportMetaEnabled(true);
      internals.settings.setViewportStyle("mobile");
</script>
<meta name="viewport" content="viewport_content_placeholder">
<style>
body {
      width: 2000px;
      height: 2000px;
}
div {
      width: 200px;
      border: 1px solid black;
}
</style>
</head>
<body>
<div id="text">
<p>The meta viewport tag will vary between tests</p>
<p>Scrollable horizontally and vertically</p>
</div>
<div id="click_number">
</div>
)"""";
  std::size_t start = html_with_placeholder.find(placeholder);
  std::string final_html = html_with_placeholder.replace(
      start, placeholder.length(), viewport_content);
  return GURL(final_html);
}
}  // namespace

// This test class verifies usages of meta viewport tag for which double tap
// to zoom is disabled/enabled.
// The input to this parameterized test is a tuple of the form:
// - viewport_content
// - expected value for is viewport mobile optimized
// - description of test
class DoubleTapToZoomBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, bool, std::string>> {
 public:
  DoubleTapToZoomBrowserTest() {}
  ~DoubleTapToZoomBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // We need to use internals.settings to enable android settings related to
    // the meta viewport tag.
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void LoadURL() {
    EXPECT_TRUE(NavigateToURL(shell(), HtmlAsDataUrl(std::get<0>(GetParam()))));
  }
};

IN_PROC_BROWSER_TEST_P(DoubleTapToZoomBrowserTest, MobileOptimizedStatus) {
  bool expected_is_viewport_mobile_optimized = std::get<1>(GetParam());
  LoadURL();
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);
  frame_observer.WaitForMetadataChange();
  const cc::RenderFrameMetadata& last_metadata =
      frame_observer.LastRenderFrameMetadata();
  EXPECT_EQ(expected_is_viewport_mobile_optimized,
            last_metadata.is_mobile_optimized)
      << std::get<2>(GetParam());
}

// TODO(crbug.com/40805444): Flaky on mac and linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_TapDelayEnabled DISABLED_TapDelayEnabled
#else
#define MAYBE_TapDelayEnabled TapDelayEnabled
#endif
IN_PROC_BROWSER_TEST_P(DoubleTapToZoomBrowserTest, MAYBE_TapDelayEnabled) {
  bool expected_is_viewport_mobile_optimized = std::get<1>(GetParam());
  LoadURL();
  WebContents* main_contents = shell()->web_contents();
  RenderFrameSubmissionObserver frame_observer(main_contents);
  frame_observer.WaitForMetadataChange();

  ASSERT_EQ(!expected_is_viewport_mobile_optimized,
            EvalJs(shell(),
                   "let kTapDelayEnabled = 3965;"
                   "internals.isUseCounted(document, kTapDelayEnabled)"))
      << std::get<2>(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    /* No prefix. */,
    DoubleTapToZoomBrowserTest,
    ::testing::Values(
        std::make_tuple("width=device-width",
                        true,
                        "device-width alone in meta viewport tag content"),
        std::make_tuple("",
                        false,
                        "nothing specified in meta viewport tag content"),
        std::make_tuple("initial-scale=1",
                        true,
                        "initial-scale=1 alone in meta viewport tag content"),
        std::make_tuple(
            "minimum-scale=2, maximum-scale=2",
            true,
            "minimum-scale equals maximum-scale so no zooming allowed"),
        std::make_tuple("width=device-width, initial-scale=1",
                        true,
                        "both width=device-width and initial-scale=1 in meta "
                        "viewport tag content"),
        std::make_tuple("width=device-width, initial-scale=0.5",
                        false,
                        "initial-scale=0.5 and width=device-width DTZ enabled"),
        std::make_tuple(
            "width=device-width, height=device-height",
            true,
            "width is device-width and height is set to device-height"),
        std::make_tuple(
            "initial-scale=1, height=device-height",
            true,
            "initial scale set to 1 and height set to device-height"),
        std::make_tuple("minimum-scale=1",
                        true,
                        "minimum scale set to 1 implying initial-scale >=1"),
        std::make_tuple(
            "initial-scale=1.5",
            true,
            "initial scale set to >=1 implying the site is readable"),
        std::make_tuple(
            "initial-scale=1.5, minimum-scale=2",
            true,
            "minimum scale set to >=1 implying the site is readable"),
        std::make_tuple(
            "initial-scale=0.5, minimum-scale=2",
            true,
            "minimum scale set to >=1 implying the site is readable"),
        std::make_tuple("initial-scale=0.5, minimum-scale=0.75",
                        false,
                        "minimum scale set to <1 and initial-scale <1"),
        std::make_tuple("width=500", false, "fixed width"),
        std::make_tuple("width=500, minimum-scale=1",
                        false,
                        "fixed width with minimum scale"),
        std::make_tuple("maximum-scale=1",
                        false,
                        "maximum scale seeting does not disable DTZ")));
}  // namespace content
