// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

static const char kGetDepthStreamAndCallCreateImageBitmap[] =
    "getDepthStreamAndCallCreateImageBitmap";
static const char kGetStreamsByVideoKind[] = "getStreamsByVideoKind";
static const char kGetStreamsByVideoKindNoDepth[] =
    "getStreamsByVideoKindNoDepth";

}  // namespace

namespace content {

template <int device_count, bool enable_video_kind>
class WebRtcDepthCaptureBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcDepthCaptureBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcDepthCaptureBrowserTest() override {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    ASSERT_FALSE(
        command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream));
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("device-count=%d", device_count));
    if (enable_video_kind) {
      command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                      "MediaCaptureDepthVideoKind");
    }
    WebRtcContentBrowserTestBase::SetUp();
  }
};

// Command lines must be configured in SetUpCommandLine, before the test is
// multi-threaded, so any variations must be embedded in the test fixture.

// Test using two video capture devices - a color and a 16-bit depth device.
using WebRtcTwoDeviceDepthCaptureBrowserTest =
    WebRtcDepthCaptureBrowserTest<2, false>;
using WebRtcTwoDeviceDepthCaptureVideoKindBrowserTest =
    WebRtcDepthCaptureBrowserTest<2, true>;

// Test using only a color device.
using WebRtcOneDeviceDepthCaptureVideoKindBrowserTest =
    WebRtcDepthCaptureBrowserTest<1, true>;

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureBrowserTest,
                       GetDepthStreamAndCallCreateImageBitmap) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true});", kGetDepthStreamAndCallCreateImageBitmap));
}

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureVideoKindBrowserTest,
                       GetStreamsByVideoKind) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetStreamsByVideoKind));
}

IN_PROC_BROWSER_TEST_F(WebRtcOneDeviceDepthCaptureVideoKindBrowserTest,
                       GetStreamsByVideoKindNoDepth) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetStreamsByVideoKindNoDepth));
}

}  // namespace content
