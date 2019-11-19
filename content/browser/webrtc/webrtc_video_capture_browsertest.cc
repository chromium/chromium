// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"

namespace content {

namespace {

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kStartVideoCaptureAndVerifySize[] =
    "startVideoCaptureAndVerifySize(320, 200)";
static const char kWaitForVideoToTurnBlack[] = "waitForVideoToTurnBlack()";
static const char kVerifyHasReceivedTrackEndedEvent[] =
    "verifyHasReceivedTrackEndedEvent()";

}  // anonymous namespace

// Integration test that exercises video capture functionality from the
// JavaScript level.
class WebRtcVideoCaptureBrowserTest : public ContentBrowserTest {
 protected:
  WebRtcVideoCaptureBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
  }

  ~WebRtcVideoCaptureBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    EnablePixelOutput();
    embedded_test_server()->StartAcceptingConnections();
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCaptureBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureBrowserTest,
                       RecoverFromCrashInVideoCaptureProcess) {
  // This test only makes sense if the video capture service runs in a
  // separate process.
  if (!features::IsVideoCaptureServiceEnabledForOutOfProcess())
    return;

  GURL url(embedded_test_server()->GetURL(kVideoCaptureHtmlFile));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string result;
  // Start video capture and wait until it started rendering
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), kStartVideoCaptureAndVerifySize, &result));
  ASSERT_EQ("OK", result);

  // Simulate crash in video capture process
  video_capture::mojom::TestingControlsPtr service_controls;
  GetVideoCaptureService().BindControlsForTesting(
      mojo::MakeRequest(&service_controls));
  service_controls->Crash();

  // Wait for video element to turn black
  ASSERT_TRUE(ExecuteScriptAndExtractString(shell(), kWaitForVideoToTurnBlack,
                                            &result));
  ASSERT_EQ("OK", result);
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), kVerifyHasReceivedTrackEndedEvent, &result));
  ASSERT_EQ("OK", result);

  // Start capturing again and expect it to work.
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), kStartVideoCaptureAndVerifySize, &result));
  ASSERT_EQ("OK", result);
}

}  // namespace content
