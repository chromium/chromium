// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// Disable FocusDistance test which fails with Logitech cameras.
// TODO(crbug.com/40624855): renable these tests when we have a way to detect
// which device is connected and hence avoid running it if the camera is
// Logitech.
#define MAYBE_ManipulateFocusDistance DISABLED_ManipulateFocusDistance

// TODO(crbug.com/40554182): Re-enable test on Android as soon as the cause for
// the bug is understood and fixed.
// TODO(crbug.com/40754212): Flaky on Linux/Windows.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_ManipulatePan DISABLED_ManipulatePan
#define MAYBE_ManipulateZoom DISABLED_ManipulateZoom
#else
#define MAYBE_ManipulatePan ManipulatePan
#define MAYBE_ManipulateZoom ManipulateZoom
#endif

// TODO(crbug.com/793859, crbug.com/986602): This test is broken on Android
// (see above) and flaky on Linux.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ManipulateExposureTime DISABLED_ManipulateExposureTime
#else
#define MAYBE_ManipulateExposureTime ManipulateExposureTime
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// See crbug/986470
#define MAYBE_GetPhotoSettings DISABLED_GetPhotoSettings
#define MAYBE_GetTrackSettings DISABLED_GetTrackSettings
#else
#define MAYBE_GetPhotoSettings GetPhotoSettings
#define MAYBE_GetTrackSettings GetTrackSettings
#endif

// These tests are flaky on all platforms: https://crbug.com/1515035,
// https://crbug.com/1187247
#define MAYBE_GrabFrame DISABLED_GrabFrame

namespace {

static const char kImageCaptureHtmlFile[] = "/media/image_capture_test.html";

enum class TargetCamera { REAL_WEBCAM, FAKE_DEVICE };

enum class TargetVideoCaptureImplementation {
  DEFAULT,
#if BUILDFLAG(IS_WIN)
  WIN_MEDIA_FOUNDATION
#endif
};
const TargetVideoCaptureImplementation
    kTargetVideoCaptureImplementationsForFakeDevice[] = {
        TargetVideoCaptureImplementation::DEFAULT};

}  // namespace

// This class is the content_browsertests for Image Capture API, which allows
// for capturing still images out of a MediaStreamTrack. Is a
// WebRtcWebcamBrowserTest to be able to use a physical camera.
class WebRtcImageCaptureBrowserTestBase
    : public UsingRealWebcam_WebRtcWebcamBrowserTest {
 public:
  WebRtcImageCaptureBrowserTestBase() = default;

  WebRtcImageCaptureBrowserTestBase(const WebRtcImageCaptureBrowserTestBase&) =
      delete;
  WebRtcImageCaptureBrowserTestBase& operator=(
      const WebRtcImageCaptureBrowserTestBase&) = delete;

  ~WebRtcImageCaptureBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(
        command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream));
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUp();
  }

  // Tries to run a |command| JS test, returning true if the test can be safely
  // skipped or it works as intended, or false otherwise.
  virtual bool RunImageCaptureTestCase(const std::string& command) {
    GURL url(embedded_test_server()->GetURL(kImageCaptureHtmlFile));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
      DVLOG(1) << "No video device; skipping test...";
      return true;
    }

    LookupAndLogNameAndIdOfFirstCamera();

    return ExecJs(shell(), command);
  }
};

// Test fixture for setting up a capture device (real or fake) that successfully
// serves all image capture requests.
class WebRtcImageCaptureSucceedsBrowserTest
    : public WebRtcImageCaptureBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<TargetCamera,
                     TargetVideoCaptureImplementation>> {
 public:
  WebRtcImageCaptureSucceedsBrowserTest() {
    std::vector<base::test::FeatureRef> features_to_enable;
    std::vector<base::test::FeatureRef> features_to_disable;
#if BUILDFLAG(IS_WIN)
    if (std::get<1>(GetParam()) ==
        TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION) {
      features_to_enable.push_back(media::kMediaFoundationVideoCapture);
    } else {
      features_to_disable.push_back(media::kMediaFoundationVideoCapture);
    }
#endif
    scoped_feature_list_.InitWithFeatures(features_to_enable,
                                          features_to_disable);
  }

  WebRtcImageCaptureSucceedsBrowserTest(
      const WebRtcImageCaptureSucceedsBrowserTest&) = delete;
  WebRtcImageCaptureSucceedsBrowserTest& operator=(
      const WebRtcImageCaptureSucceedsBrowserTest&) = delete;

  ~WebRtcImageCaptureSucceedsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcImageCaptureBrowserTestBase::SetUpCommandLine(command_line);

    if (std::get<0>(GetParam()) == TargetCamera::FAKE_DEVICE) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeDeviceForMediaStream);
      ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/41478485): Flaky on Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GetPhotoCapabilities DISABLED_GetPhotoCapabilities
#else
#define MAYBE_GetPhotoCapabilities GetPhotoCapabilities
#endif
IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetPhotoCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoCapabilitiesSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetPhotoSettings) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoSettingsSucceeds()"));
}

// TODO(crbug.com/40754212): Flaky on Linux/Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_TakePhoto DISABLED_TakePhoto
#else
#define MAYBE_TakePhoto TakePhoto
#endif
IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, MAYBE_TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

// Flaky. crbug.com/998116
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GetTrackCapabilities DISABLED_GetTrackCapabilities
#else
#define MAYBE_GetTrackCapabilities GetTrackCapabilities
#endif
IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetTrackCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGetTrackCapabilities()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetTrackSettings) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGetTrackSettings()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulatePan) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulatePan()"));
}

// TODO(crbug.com/41478484): Flaky on Linux.
// TODO(crbug.com/40554182): Re-enable test on Android as soon as the cause for
// the bug is understood and fixed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#define MAYBE_ManipulateTilt DISABLED_ManipulateTilt
#else
#define MAYBE_ManipulateTilt ManipulateTilt
#endif
IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulateTilt) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulateTilt()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulateZoom) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulateZoom()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulateExposureTime) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulateExposureTime()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulateFocusDistance) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulateFocusDistance()"));
}

INSTANTIATE_TEST_SUITE_P(
    All,  // Use no prefix, so that these get picked up when using
          // --gtest_filter=WebRtc*
    WebRtcImageCaptureSucceedsBrowserTest,
    testing::Combine(
        testing::Values(TargetCamera::FAKE_DEVICE),
        testing::ValuesIn(kTargetVideoCaptureImplementationsForFakeDevice)));

// Tests on real webcam can only run on platforms for which the image capture
// API has already been implemented.
// Note, these tests must be run sequentially, since multiple parallel test runs
// competing for a single physical webcam typically causes failures.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)

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
    WebRtcImageCaptureSucceedsBrowserTest,
    testing::Combine(
        testing::Values(TargetCamera::REAL_WEBCAM),
        testing::ValuesIn(kTargetVideoCaptureImplementationsForRealWebcam)));
#endif

// Test fixture template for setting up a fake device with a custom
// configuration. We are going to use this to set up fake devices that respond
// to invocation of various ImageCapture API calls with a failure response.
template <typename FakeDeviceConfigTraits>
class WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest
    : public WebRtcImageCaptureBrowserTestBase {
 public:
  WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest() = default;
  WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest(
      const WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest&) = delete;
  WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest& operator=(
      const WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest&) = delete;

  ~WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcImageCaptureBrowserTestBase::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        std::string("config=") + FakeDeviceConfigTraits::config());
  }
};

struct GetPhotoStateFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::
        kDeviceConfigForGetPhotoStateFails;
  }
};

using WebRtcImageCaptureGetPhotoStateFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        GetPhotoStateFailsConfigTraits>;

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       GetCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  // When the fake device faile, we expect an empty set of capabilities to
  // reported back to JS.
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoCapabilitiesSucceeds()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

struct SetPhotoOptionsFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::
        kDeviceConfigForSetPhotoOptionsFails;
  }
};

using WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        SetPhotoOptionsFailsConfigTraits>;

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

struct TakePhotoFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::kDeviceConfigForTakePhotoFails;
  }
};

using WebRtcImageCaptureTakePhotoFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        TakePhotoFailsConfigTraits>;

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureTakePhotoFailsBrowserTest, TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcImageCaptureTakePhotoFailsBrowserTest,
                       MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

}  // namespace content
