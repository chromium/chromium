// Copyright 2017 The Chromium Authors
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

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
 public:
  WebRtcVideoCaptureBrowserTest(const WebRtcVideoCaptureBrowserTest&) = delete;
  WebRtcVideoCaptureBrowserTest& operator=(
      const WebRtcVideoCaptureBrowserTest&) = delete;

 protected:
  WebRtcVideoCaptureBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
  }

  ~WebRtcVideoCaptureBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
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
};

#if BUILDFLAG(IS_MAC)
// TODO(https://crbug.com/1235254): This test is flakey on macOS.
#define MAYBE_RecoverFromCrashInVideoCaptureProcess \
  DISABLED_RecoverFromCrashInVideoCaptureProcess
#else
#define MAYBE_RecoverFromCrashInVideoCaptureProcess \
  RecoverFromCrashInVideoCaptureProcess
#endif
IN_PROC_BROWSER_TEST_F(WebRtcVideoCaptureBrowserTest,
                       MAYBE_RecoverFromCrashInVideoCaptureProcess) {
  // This test only makes sense if the video capture service runs in a
  // separate process.
  if (!features::IsVideoCaptureServiceEnabledForOutOfProcess())
    return;

  GURL url(embedded_test_server()->GetURL(kVideoCaptureHtmlFile));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Start video capture and wait until it started rendering
  ASSERT_EQ("OK", EvalJs(shell(), kStartVideoCaptureAndVerifySize,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // Simulate crash in video capture process
  mojo::Remote<video_capture::mojom::TestingControls> service_controls;
  GetVideoCaptureService().BindControlsForTesting(
      service_controls.BindNewPipeAndPassReceiver());
  service_controls->Crash();

  // Wait for video element to turn black
  ASSERT_EQ("OK", EvalJs(shell(), kWaitForVideoToTurnBlack,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));
  ASSERT_EQ("OK", EvalJs(shell(), kVerifyHasReceivedTrackEndedEvent,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // Start capturing again and expect it to work.
  ASSERT_EQ("OK", EvalJs(shell(), kStartVideoCaptureAndVerifySize,
                         EXECUTE_SCRIPT_USE_MANUAL_REPLY));
}

}  // namespace content
