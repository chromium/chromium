// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_webcam_browsertest.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const base::CommandLine::StringType FAKE_DEVICE_FLAG =
#if BUILDFLAG(IS_WIN)
    base::ASCIIToWide(switches::kUseFakeDeviceForMediaStream);
#else
    switches::kUseFakeDeviceForMediaStream;
#endif

bool IsUseFakeDeviceForMediaStream(const base::CommandLine::StringType& arg) {
  return arg.find(FAKE_DEVICE_FLAG) != std::string::npos;
}

void RemoveFakeDeviceFromCommandLine(base::CommandLine* command_line) {
  base::CommandLine::StringVector argv = command_line->argv();
  std::erase_if(argv, IsUseFakeDeviceForMediaStream);
  command_line->InitFromArgv(argv);
}

}  // namespace

namespace content {

// The prefix "UsingRealWebcam" is used to indicate that this test must not
// be run in parallel with other tests using a real webcam.
void UsingRealWebcam_WebRtcWebcamBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Allows for accessing capture devices without prompting for permission.
  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);

  // The content_browsertests run with this flag by default, so remove the flag
  // --use-fake-device-for-media-stream here.
  // TODO(chfremer): A better solution would be to promote
  // |kUseFakeDeviceForMediaStream| from a switch to a base::Feature and then
  // use ScopedFeatureList::InitAndEnableFeature() in the base class and
  // ScopedFeatureList::InitAndDisableFeature() here and wherever else we want
  // to use a real webcam.
  RemoveFakeDeviceFromCommandLine(command_line);
}

void UsingRealWebcam_WebRtcWebcamBrowserTest::SetUp() {
  EnablePixelOutput();
  ContentBrowserTest::SetUp();
}

// Tests that GetUserMedia acquires VGA by default.
// The MANUAL prefix is used to only run this tests on certain bots for which
// we can guarantee that tests are executed sequentially. TODO(chfremer): Is
// this still needed or is the prefix "UsingRealWebcam" sufficient?
IN_PROC_BROWSER_TEST_F(UsingRealWebcam_WebRtcWebcamBrowserTest,
                       MANUAL_CanAcquireVga) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/media/getusermedia-real-webcam.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
    DVLOG(0) << "No video device; skipping test...";
    return;
  }

  std::string result =
      EvalJs(shell(), "getUserMediaAndReturnVideoDimensions({video: true})")
          .ExtractString();

  if (result == "640x480" || result == "480x640") {
    // Don't care if the device happens to be in landscape or portrait mode
    // since we don't know how it is oriented in the lab :)
    return;
  }
  FAIL() << "Expected resolution to be 640x480 or 480x640, got: " << result;
}

}  // namespace content
