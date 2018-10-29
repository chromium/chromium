// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

static const char kGetDepthStreamAndCallCreateImageBitmap[] =
    "getDepthStreamAndCallCreateImageBitmap";
static const char kGetDepthStreamAndCameraCalibration[] =
    "getDepthStreamAndCameraCalibration";
static const char kGetBothStreamsAndCheckForFeaturesPresence[] =
    "getBothStreamsAndCheckForFeaturesPresence";
static const char kGetStreamsByVideoKind[] = "getStreamsByVideoKind";
static const char kGetStreamsByVideoKindNoDepth[] =
    "getStreamsByVideoKindNoDepth";

void RemoveSwitchFromCommandLine(base::CommandLine* command_line,
                                 const std::string& switch_value) {
  base::CommandLine::StringVector argv = command_line->argv();
  const base::CommandLine::StringType switch_string =
#if defined(OS_WIN)
      base::ASCIIToUTF16(switch_value);
#else
      switch_value;
#endif
  base::EraseIf(argv,
                [switch_string](const base::CommandLine::StringType& value) {
                  return value.find(switch_string) != std::string::npos;
                });
  command_line->InitFromArgv(argv);
}

}  // namespace

namespace content {

template <int device_count>
class WebRtcDepthCaptureBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcDepthCaptureBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcDepthCaptureBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By default, command line argument is present with no value.  We need to
    // remove it and then add the value defining two video capture devices.
    const std::string fake_device_switch =
        switches::kUseFakeDeviceForMediaStream;
    ASSERT_TRUE(command_line->HasSwitch(fake_device_switch) &&
                command_line->GetSwitchValueASCII(fake_device_switch).empty());
    RemoveSwitchFromCommandLine(command_line, fake_device_switch);
    command_line->AppendSwitchASCII(
        fake_device_switch,
        base::StringPrintf("device-count=%d", device_count));
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
  }
};

// Test using two video capture devices - a color and a 16-bit depth device.
using WebRtcTwoDeviceDepthCaptureBrowserTest = WebRtcDepthCaptureBrowserTest<2>;

// Test using only a color device.
using WebRtcOneDeviceDepthCaptureBrowserTest = WebRtcDepthCaptureBrowserTest<1>;

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureBrowserTest,
                       GetDepthStreamAndCallCreateImageBitmap) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true});", kGetDepthStreamAndCallCreateImageBitmap));
}

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureBrowserTest,
                       GetDepthStreamAndCameraCalibration) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("--enable-blink-features",
                                  "MediaCaptureDepth");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true});", kGetDepthStreamAndCameraCalibration));
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/734558
#define MAYBE_GetBothStreamsAndCheckForFeaturesPresence \
  DISABLED_GetBothStreamsAndCheckForFeaturesPresence
#else
#define MAYBE_GetBothStreamsAndCheckForFeaturesPresence \
  GetBothStreamsAndCheckForFeaturesPresence
#endif

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureBrowserTest,
                       MAYBE_GetBothStreamsAndCheckForFeaturesPresence) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("--enable-blink-features",
                                  "MediaCaptureDepth");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true});", kGetBothStreamsAndCheckForFeaturesPresence));
}

IN_PROC_BROWSER_TEST_F(WebRtcTwoDeviceDepthCaptureBrowserTest,
                       GetStreamsByVideoKind) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("--enable-blink-features",
                                  "MediaCaptureDepthVideoKind");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetStreamsByVideoKind));
}

IN_PROC_BROWSER_TEST_F(WebRtcOneDeviceDepthCaptureBrowserTest,
                       GetStreamsByVideoKindNoDepth) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("--enable-blink-features",
                                  "MediaCaptureDepthVideoKind");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetStreamsByVideoKindNoDepth));
}

}  // namespace content
