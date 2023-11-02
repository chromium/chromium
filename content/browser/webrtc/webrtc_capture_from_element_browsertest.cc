// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/mojo/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
// Remote mojo renderer does not send audio/video frames back to the renderer
// process and hence does not support capture: https://crbug.com/641559.
#define MAYBE_CaptureFromMediaElement DISABLED_CaptureFromMediaElement
#else
#define MAYBE_CaptureFromMediaElement CaptureFromMediaElement
#endif

namespace {

static const char kCanvasCaptureTestHtmlFile[] = "/media/canvas_capture.html";
static const char kCanvasCaptureColorTestHtmlFile[] =
    "/media/canvas_capture_color.html";
static const char kVideoAudioHtmlFile[] =
    "/media/video_audio_element_capture_test.html";

static struct FileAndTypeParameters {
  bool has_video;
  bool has_audio;
  bool use_audio_tag;
  std::string filename;
} const kFileAndTypeParameters[] = {
    {true, false, false, "bear-320x240-video-only.webm"},
    {false, true, false, "bear-320x240-audio-only.webm"},
    {true, true, false, "bear-320x240.webm"},
    {false, true, true, "bear-320x240-audio-only.webm"},
};

}  // namespace

namespace content {

// This class is the browser tests for Media Capture from DOM Elements API,
// which allows for creation of a MediaStream out of a <canvas>, <video> or
// <audio> element.
class WebRtcCaptureFromElementBrowserTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<struct FileAndTypeParameters> {
 public:
  WebRtcCaptureFromElementBrowserTest() {}

  WebRtcCaptureFromElementBrowserTest(
      const WebRtcCaptureFromElementBrowserTest&) = delete;
  WebRtcCaptureFromElementBrowserTest& operator=(
      const WebRtcCaptureFromElementBrowserTest&) = delete;

  ~WebRtcCaptureFromElementBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);

    // Allow <video>/<audio>.play() when not initiated by user gesture.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    // Allow experimental canvas features.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Allow window.internals for simulating context loss.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kExposeInternalsForTesting);
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       VerifyCanvas2DCaptureColor) {
  MakeTypicalCall("testCanvas2DCaptureColors(true);",
                  kCanvasCaptureColorTestHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       VerifyCanvasWebGLCaptureOpaqueColor) {
  MakeTypicalCall("testCanvasWebGLCaptureOpaqueColors(true);",
                  kCanvasCaptureColorTestHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       VerifyCanvasWebGLCaptureAlphaColor) {
  MakeTypicalCall("testCanvasWebGLCaptureAlphaColors(true);",
                  kCanvasCaptureColorTestHtmlFile);
}

// TODO(https://crbug.com/1350300): Flaky.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_VerifyCanvasCapture2DFrames DISABLED_VerifyCanvasCapture2DFrames
#else
#define MAYBE_VerifyCanvasCapture2DFrames VerifyCanvasCapture2DFrames
#endif
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       MAYBE_VerifyCanvasCapture2DFrames) {
  MakeTypicalCall("testCanvasCapture(draw2d);", kCanvasCaptureTestHtmlFile);
}

// TODO(https://crbug.com/1335032): Flaky.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_VerifyCanvasCaptureWebGLFrames \
  DISABLED_VerifyCanvasCaptureWebGLFrames
#else
#define MAYBE_VerifyCanvasCaptureWebGLFrames VerifyCanvasCaptureWebGLFrames
#endif
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       MAYBE_VerifyCanvasCaptureWebGLFrames) {
  MakeTypicalCall("testCanvasCapture(drawWebGL);", kCanvasCaptureTestHtmlFile);
}

// https://crbug.com/869723
// Flaky on Windows 10 with Viz (i.e. in viz_content_browsertests).
// https://crbug.com/989759
// Flaky on other platforms due to frame delivery for offscreen canvases.
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       DISABLED_VerifyCanvasCaptureOffscreenCanvasFrames) {
  MakeTypicalCall("testCanvasCapture(drawOffscreenCanvas);",
                  kCanvasCaptureTestHtmlFile);
}

// TODO(crbug.com/1334909): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       DISABLED_VerifyCanvasCaptureBitmapRendererFrames) {
  MakeTypicalCall("testCanvasCapture(drawBitmapRenderer);",
                  kCanvasCaptureTestHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcCaptureFromElementBrowserTest,
                       MAYBE_CaptureFromMediaElement) {
  MakeTypicalCall(JsReplace("testCaptureFromMediaElement($1, $2, $3, $4)",
                            GetParam().filename, GetParam().has_video,
                            GetParam().has_audio, GetParam().use_audio_tag),
                  kVideoAudioHtmlFile);
}

// https://crbug.com/986020.
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       DISABLED_CaptureFromCanvas2DHandlesContextLoss) {
  MakeTypicalCall("testCanvas2DContextLoss(true);",
                  kCanvasCaptureColorTestHtmlFile);
}

// Not supported on android https://crbug.com/898286.
// Not supported on accelerated canvases https://crbug.com/954142.
IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       DISABLED_CaptureFromOpaqueCanvas2DHandlesContextLoss) {
  MakeTypicalCall("testCanvas2DContextLoss(false);",
                  kCanvasCaptureColorTestHtmlFile);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcCaptureFromElementBrowserTest,
                         testing::ValuesIn(kFileAndTypeParameters));
}  // namespace content
