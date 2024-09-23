// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

namespace {

static const char kImageCaptureStressHtmlFile[] =
    "/media/image_capture_stress_test.html";

enum class TargetVideoCaptureImplementation {
  DEFAULT,
#if BUILDFLAG(IS_WIN)
  WIN_MEDIA_FOUNDATION
#endif
};

}  // namespace

class WebRtcImageCaptureStressBrowserTest
    : public UsingRealWebcam_WebRtcWebcamBrowserTest,
      public testing::WithParamInterface<TargetVideoCaptureImplementation> {
 public:
  WebRtcImageCaptureStressBrowserTest() {
    std::vector<base::test::FeatureRef> features_to_enable;
    std::vector<base::test::FeatureRef> features_to_disable;
#if BUILDFLAG(IS_WIN)
    if (GetParam() == TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION) {
      features_to_enable.push_back(media::kMediaFoundationVideoCapture);
    } else {
      features_to_disable.push_back(media::kMediaFoundationVideoCapture);
    }
#endif
    scoped_feature_list_.InitWithFeatures(features_to_enable,
                                          features_to_disable);
  }

  WebRtcImageCaptureStressBrowserTest(
      const WebRtcImageCaptureStressBrowserTest&) = delete;
  WebRtcImageCaptureStressBrowserTest& operator=(
      const WebRtcImageCaptureStressBrowserTest&) = delete;

  ~WebRtcImageCaptureStressBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(
        command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream));
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUp();
  }

  // Tries to run a |command| JS test, returning true if the test can be safely
  // skipped or it works as intended, or false otherwise.
  virtual bool RunImageCaptureTestCase(const std::string& command) {
    GURL url(embedded_test_server()->GetURL(kImageCaptureStressHtmlFile));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
      LOG(WARNING) << "No video device; skipping test...";
      return true;
    }

    LookupAndLogNameAndIdOfFirstCamera();

    return ExecJs(shell(), command);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests on real webcam can only run on platforms for which the image capture
// API has already been implemented.
// Note, these tests must be run sequentially, since multiple parallel test runs
// competing for a single physical webcam typically causes failures.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureStressBrowserTest,
                       MANUAL_Take10Photos) {
  ASSERT_TRUE(RunImageCaptureTestCase("testTake10PhotosSucceeds()"));
}

const TargetVideoCaptureImplementation
    kTargetVideoCaptureImplementationsForRealWebcam[] = {
        TargetVideoCaptureImplementation::DEFAULT,
#if BUILDFLAG(IS_WIN)
        TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION
#endif
};

INSTANTIATE_TEST_SUITE_P(
    UsingRealWebcam,  // This prefix can be used with --gtest_filter to
                      // distinguish the tests using a real camera from the ones
                      // that don't.
    WebRtcImageCaptureStressBrowserTest,
    testing::ValuesIn(kTargetVideoCaptureImplementationsForRealWebcam));
#endif

}  // namespace content
