// Copyright 2016 The Chromium Authors. All rights reserved.
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

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

// Disable FocusDistance test which fails with Logitec cameras.
// TODO(crbug.com/957020): renable these tests when we have a way to detect
// which device is connected and hence avoid running it if the camera is
// Logitech.
#define MAYBE_ManipulateFocusDistance DISABLED_ManipulateFocusDistance

#if defined(OS_ANDROID)
// TODO(crbug.com/793859): Re-enable test on Android as soon as the cause for
// the bug is understood and fixed.
#define MAYBE_ManipulatePan DISABLED_ManipulatePan
#define MAYBE_ManipulateZoom DISABLED_ManipulateZoom
#else
#define MAYBE_ManipulatePan ManipulatePan
#define MAYBE_ManipulateZoom ManipulateZoom
#endif

// TODO(crbug.com/793859, crbug.com/986602): This test is broken on Android
// (see above) and flaky on Linux.
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_ManipulateExposureTime DISABLED_ManipulateExposureTime
#else
#define MAYBE_ManipulateExposureTime ManipulateExposureTime
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// See crbug/986470
#define MAYBE_GetPhotoSettings DISABLED_GetPhotoSettings
#define MAYBE_GetTrackSettings DISABLED_GetTrackSettings
#else
#define MAYBE_GetPhotoSettings GetPhotoSettings
#define MAYBE_GetTrackSettings GetTrackSettings
#endif

namespace {

static const char kImageCaptureHtmlFile[] = "/media/image_capture_test.html";

enum class TargetCamera { REAL_WEBCAM, FAKE_DEVICE };

static struct TargetVideoCaptureStack {
  bool use_video_capture_service;
} const kTargetVideoCaptureStacks[] = {{false},
// Mojo video capture is currently not supported on Android
// TODO(chfremer): Remove this as soon as https://crbug.com/720500 is
// resolved.
#if !defined(OS_ANDROID)
                                       {true}
#endif
};

enum class TargetVideoCaptureImplementation {
  DEFAULT,
#if defined(OS_WIN)
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
  ~WebRtcImageCaptureBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));

    // Enable Pan/Tilt for testing.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaCapturePanTilt");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    UsingRealWebcam_WebRtcWebcamBrowserTest::SetUp();
  }

  // Tries to run a |command| JS test, returning true if the test can be safely
  // skipped or it works as intended, or false otherwise.
  virtual bool RunImageCaptureTestCase(const std::string& command) {
#if defined(OS_ANDROID)
    // TODO(mcasas): fails on Lollipop devices: https://crbug.com/634811
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_MARSHMALLOW) {
      return true;
    }
#endif

    GURL url(embedded_test_server()->GetURL(kImageCaptureHtmlFile));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
      DVLOG(1) << "No video device; skipping test...";
      return true;
    }

    LookupAndLogNameAndIdOfFirstCamera();

    std::string result;
    if (!ExecuteScriptAndExtractString(shell(), command, &result))
      return false;
    DLOG_IF(ERROR, result != "OK") << result;
    return result == "OK";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcImageCaptureBrowserTestBase);
};

// Test fixture for setting up a capture device (real or fake) that successfully
// serves all image capture requests.
class WebRtcImageCaptureSucceedsBrowserTest
    : public WebRtcImageCaptureBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<TargetCamera,
                     TargetVideoCaptureStack,
                     TargetVideoCaptureImplementation>> {
 public:
  WebRtcImageCaptureSucceedsBrowserTest() {
    std::vector<base::Feature> features_to_enable;
    std::vector<base::Feature> features_to_disable;
    if (std::get<1>(GetParam()).use_video_capture_service)
      features_to_enable.push_back(features::kMojoVideoCapture);
    else
      features_to_disable.push_back(features::kMojoVideoCapture);
#if defined(OS_WIN)
    if (std::get<2>(GetParam()) ==
        TargetVideoCaptureImplementation::WIN_MEDIA_FOUNDATION) {
      features_to_enable.push_back(media::kMediaFoundationVideoCapture);
    } else {
      features_to_disable.push_back(media::kMediaFoundationVideoCapture);
    }
#endif
    scoped_feature_list_.InitWithFeatures(features_to_enable,
                                          features_to_disable);
  }

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

  DISALLOW_COPY_AND_ASSIGN(WebRtcImageCaptureSucceedsBrowserTest);
};

// TODO(crbug.com/998305): Flaky on Linux.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

// Flaky. crbug.com/998116
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

// TODO(crbug.com/998304): Flaky on Linux.
// TODO(crbug.com/793859): Re-enable test on Android as soon as the cause for
// the bug is understood and fixed.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
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
        testing::ValuesIn(kTargetVideoCaptureStacks),
        testing::ValuesIn(kTargetVideoCaptureImplementationsForFakeDevice)));

// Tests on real webcam can only run on platforms for which the image capture
// API has already been implemented.
// Note, these tests must be run sequentially, since multiple parallel test runs
// competing for a single physical webcam typically causes failures.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_ANDROID) || defined(OS_WIN)

const TargetVideoCaptureImplementation
    kTargetVideoCaptureImplementationsForRealWebcam[] = {
        TargetVideoCaptureImplementation::DEFAULT,
#if defined(OS_WIN)
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
        testing::ValuesIn(kTargetVideoCaptureStacks),
        testing::ValuesIn(kTargetVideoCaptureImplementationsForRealWebcam)));
#endif

// Test fixture template for setting up a fake device with a custom
// configuration. We are going to use this to set up fake devices that respond
// to invocation of various ImageCapture API calls with a failure response.
template <typename FakeDeviceConfigTraits>
class WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest
    : public WebRtcImageCaptureBrowserTestBase,
      public testing::WithParamInterface<TargetVideoCaptureStack> {
 public:
  WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest() {
    if (GetParam().use_video_capture_service) {
      scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
    }
  }

  ~WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcImageCaptureBrowserTestBase::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        std::string("config=") + FakeDeviceConfigTraits::config());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest);
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

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       GetCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  // When the fake device faile, we expect an empty set of capabilities to
  // reported back to JS.
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoCapabilitiesSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                         testing::ValuesIn(kTargetVideoCaptureStacks));

struct SetPhotoOptionsFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::
        kDeviceConfigForSetPhotoOptionsFails;
  }
};

using WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        SetPhotoOptionsFailsConfigTraits>;

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                         testing::ValuesIn(kTargetVideoCaptureStacks));

struct TakePhotoFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::kDeviceConfigForTakePhotoFails;
  }
};

using WebRtcImageCaptureTakePhotoFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        TakePhotoFailsConfigTraits>;

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureTakePhotoFailsBrowserTest, TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureTakePhotoFailsBrowserTest, GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcImageCaptureTakePhotoFailsBrowserTest,
                         testing::ValuesIn(kTargetVideoCaptureStacks));

}  // namespace content
