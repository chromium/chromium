// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_CAPTURE_DEVICE_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_CAPTURE_DEVICE_BROWSERTEST_BASE_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/media/capture/fake_video_capture_stack.h"
#include "content/public/test/content_browser_test.h"
#include "media/base/video_types.h"
#include "media/capture/video_capture_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace test_server
}  // namespace net

namespace content {

class FrameSinkVideoCaptureDevice;

// Common base class for screen capture browser tests. Since this is a
// ContentBrowserTest, it assumes the test environment consists of a content
// shell and a single WebContents.
class ContentCaptureDeviceBrowserTestBase : public ContentBrowserTest {
 public:
  ContentCaptureDeviceBrowserTestBase();

  ContentCaptureDeviceBrowserTestBase(
      const ContentCaptureDeviceBrowserTestBase&) = delete;
  ContentCaptureDeviceBrowserTestBase& operator=(
      const ContentCaptureDeviceBrowserTestBase&) = delete;

  ~ContentCaptureDeviceBrowserTestBase() override;

  FakeVideoCaptureStack* capture_stack() { return &capture_stack_; }
  FrameSinkVideoCaptureDevice* device() const { return device_.get(); }

  // Alters the solid fill color making up the page content. This will trigger a
  // compositor update, which will trigger a frame capture.
  void ChangePageContentColor(SkColor color);

  // Returns the captured source size, but also sanity-checks that it is not
  // changing during the test. Prefer to use this method instead of
  // GetCapturedSourceSize() to improve test stability.
  gfx::Size GetExpectedSourceSize();

  // Returns capture parameters based on the captured source size.
  // Capture format can be customized by the subclasses, see
  // |GetVideoPixelFormat()|.
  media::VideoCaptureParams SnapshotCaptureParams();

  // Returns the actual minimum capture period the device is using. This should
  // not be called until after AllocateAndStartAndWaitForFirstFrame().
  base::TimeDelta GetMinCapturePeriod();

  // Navigates to the initial document, according to the current test
  // parameters, and waits for page load completion. All test fixtures should
  // call this before any of the other methods.
  void NavigateToInitialDocument();

  // Creates and starts the device for frame capture, and checks that the
  // initial refresh frame is delivered.
  void AllocateAndStartAndWaitForFirstFrame();

  // Stops and destroys the device.
  void StopAndDeAllocate();

  // Runs the message loop until idle.
  void RunUntilIdle();

  void ClearCapturedFramesQueue() { capture_stack_.ClearCapturedFramesQueue(); }

  bool HasCapturedFramesInQueue() const {
    return capture_stack_.HasCapturedFrames();
  }

  // Navigates to the test document using a different domain (host). This will
  // force a new render process to be spun-up, and that is used to test
  // re-targetting logic.
  void NavigateToAlternateSite();

  // Crashes the renderer by asking it to navigate to chrome://crash.
  void CrashTheRenderer();

  // Executes a page reload, assuming this is for a previously-crashed renderer.
  void ReloadAfterCrash();

 protected:
  // These all return false, but can be overridden for parameterized tests to
  // change the behavior of this base class.
  virtual bool IsSoftwareCompositingTest() const;
  virtual bool IsFixedAspectRatioTest() const;
  virtual bool IsCrossSiteCaptureTest() const;

  // Used to customize the video pixel format that will be used for capture.
  virtual media::VideoPixelFormat GetVideoPixelFormat() const;

  // Returns the size of the original content (i.e., not including any
  // stretching/scaling being done to fit it within a video frame).
  virtual gfx::Size GetCapturedSourceSize() const = 0;

  // Returns a new FrameSinkVideoCaptureDevice instance.
  virtual std::unique_ptr<FrameSinkVideoCaptureDevice> CreateDevice() = 0;

  // Called to wait for the first frame with expected content.
  virtual void WaitForFirstFrame() = 0;

  // ContentBrowserTest overrides to enable pixel output and set-up/tear-down
  // the embedded HTTP server that provides test content.
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  // Called by the embedded test HTTP server to provide the document resources.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  FakeVideoCaptureStack capture_stack_;
  std::optional<gfx::Size> expected_source_size_;
  std::unique_ptr<FrameSinkVideoCaptureDevice> device_;

  // Arbitrary string constants used to refer to each document by
  // host+path. Note that the "inner frame" and "outer frame" must have
  // different hostnames to engage the cross-site process isolation logic in the
  // browser.
  static constexpr char kInnerFrameHostname[] = "innerframe.com";
  static constexpr char kInnerFramePath[] = "/inner.html";
  static constexpr char kOuterFrameHostname[] = "outerframe.com";
  static constexpr char kOuterFramePath[] = "/outer.html";
  static constexpr char kSingleFrameHostname[] = "singleframe.com";
  static constexpr char kSingleFramePath[] = "/single.html";
  static constexpr char kAlternateHostname[] = "alternate.com";
  static constexpr char kAlternatePath[] = "/alternate.html";
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_CAPTURE_DEVICE_BROWSERTEST_BASE_H_
