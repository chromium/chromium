// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/content_capture_device_browsertest_base.h"

#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/browser/media/capture/frame_sink_video_capture_device.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/video_types.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/display/display_switches.h"
#include "url/gurl.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace content {

ContentCaptureDeviceBrowserTestBase::ContentCaptureDeviceBrowserTestBase() =
    default;

ContentCaptureDeviceBrowserTestBase::~ContentCaptureDeviceBrowserTestBase() =
    default;

void ContentCaptureDeviceBrowserTestBase::ChangePageContentColor(
    SkColor color) {
  // See the HandleRequest() method for the original documents being modified
  // here.
  std::string script;
  const std::string color_string =
      base::StringPrintf("%02x%02x%02x", SkColorGetR(color), SkColorGetG(color),
                         SkColorGetB(color));
  if (IsCrossSiteCaptureTest()) {
    const GURL& inner_frame_url =
        embedded_test_server()->GetURL(kInnerFrameHostname, kInnerFramePath);
    script = base::StringPrintf(
        "document.getElementsByTagName('iframe')[0].src = '%s?color=%s';",
        inner_frame_url.spec().c_str(), color_string.c_str());
  } else {
    script = base::StringPrintf("document.body.style.backgroundColor = '#%s';",
                                color_string.c_str());
  }

  CHECK(ExecJs(shell()->web_contents(), script));
}

gfx::Size ContentCaptureDeviceBrowserTestBase::GetExpectedSourceSize() {
  const gfx::Size source_size = GetCapturedSourceSize();
  if (expected_source_size_) {
    EXPECT_EQ(*expected_source_size_, source_size)
        << "Sanity-check failed: Source size changed during this test.";
  } else {
    expected_source_size_.emplace(source_size);
    VLOG(1) << "Captured source size is " << expected_source_size_->ToString();
  }
  return source_size;
}

media::VideoCaptureParams
ContentCaptureDeviceBrowserTestBase::SnapshotCaptureParams() {
  constexpr gfx::Size kMaxCaptureSize = gfx::Size(320, 320);
  constexpr int kMaxFramesPerSecond = 60;

  gfx::Size capture_size = kMaxCaptureSize;
  if (IsFixedAspectRatioTest()) {
    // Half either the width or height, depending on the source size. The goal
    // is to force obvious letterboxing (or pillarboxing), regardless of how the
    // source is currently sized/oriented.
    const gfx::Size source_size = GetExpectedSourceSize();
    if (source_size.width() < source_size.height()) {
      capture_size.set_height(capture_size.height() / 2);
    } else {
      capture_size.set_width(capture_size.width() / 2);
    }
  }

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      capture_size, kMaxFramesPerSecond, GetVideoPixelFormat());
  params.resolution_change_policy =
      IsFixedAspectRatioTest()
          ? media::ResolutionChangePolicy::FIXED_ASPECT_RATIO
          : media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  return params;
}

media::VideoPixelFormat
ContentCaptureDeviceBrowserTestBase::GetVideoPixelFormat() const {
  return media::VideoPixelFormat::PIXEL_FORMAT_I420;
}

base::TimeDelta ContentCaptureDeviceBrowserTestBase::GetMinCapturePeriod() {
  return base::Microseconds(
      base::Time::kMicrosecondsPerSecond /
      device_->capture_params().requested_format.frame_rate);
}

void ContentCaptureDeviceBrowserTestBase::NavigateToInitialDocument() {
  // If doing a cross-site capture test, navigate to the more-complex document
  // that also contains an iframe (rendered in a separate process). Otherwise,
  // navigate to the simpler document.
  if (IsCrossSiteCaptureTest()) {
    ASSERT_TRUE(NavigateToURL(
        shell(),
        embedded_test_server()->GetURL(kOuterFrameHostname, kOuterFramePath)));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

    // Confirm the iframe is a cross-process child render frame.
    auto* const child_frame =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child_frame);
    ASSERT_TRUE(child_frame->IsCrossProcessSubframe());
  } else {
    ASSERT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL(
                                   kSingleFrameHostname, kSingleFramePath)));
    ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
}

void ContentCaptureDeviceBrowserTestBase::
    AllocateAndStartAndWaitForFirstFrame() {
  capture_stack()->Reset();

  device_ = CreateDevice();
  device_->AllocateAndStartWithReceiver(SnapshotCaptureParams(),
                                        capture_stack()->CreateFrameReceiver());
  RunUntilIdle();
  EXPECT_TRUE(capture_stack()->Started());
  EXPECT_FALSE(capture_stack()->ErrorOccurred());
  capture_stack()->ExpectNoLogMessages();

  WaitForFirstFrame();
}

void ContentCaptureDeviceBrowserTestBase::StopAndDeAllocate() {
  device_->StopAndDeAllocate();
  device_.reset();
  RunUntilIdle();
}

void ContentCaptureDeviceBrowserTestBase::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void ContentCaptureDeviceBrowserTestBase::NavigateToAlternateSite() {
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         kAlternateHostname, kAlternatePath)));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

void ContentCaptureDeviceBrowserTestBase::CrashTheRenderer() {
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(blink::kChromeUICrashURL)));
  crash_observer.Wait();
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

void ContentCaptureDeviceBrowserTestBase::ReloadAfterCrash() {
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

bool ContentCaptureDeviceBrowserTestBase::IsSoftwareCompositingTest() const {
  return false;
}

bool ContentCaptureDeviceBrowserTestBase::IsFixedAspectRatioTest() const {
  return false;
}

bool ContentCaptureDeviceBrowserTestBase::IsCrossSiteCaptureTest() const {
  return false;
}

void ContentCaptureDeviceBrowserTestBase::SetUp() {
  // Screen capture requires readback from compositor output.
  EnablePixelOutput();

  // Conditionally force software compositing instead of GPU-accelerated
  // compositing.
  if (IsSoftwareCompositingTest()) {
    UseSoftwareCompositing();
  }

  ContentBrowserTest::SetUp();
}

void ContentCaptureDeviceBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);

  // Use a small window size to reduce test running time (since screen captures
  // must be analyzed).
  command_line->AppendSwitchASCII(switches::kContentShellHostWindowSize,
                                  "200x150");

  // Ensure the web renderer AND display compositor are both speaking "sRGB."
  command_line->AppendSwitchASCII(switches::kForceDisplayColorProfile, "srgb");
  command_line->AppendSwitchASCII(switches::kForceRasterColorProfile, "srgb");
}

void ContentCaptureDeviceBrowserTestBase::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();

  // Set-up and start the embedded test HTTP server.
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&ContentCaptureDeviceBrowserTestBase::HandleRequest,
                          base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
}

void ContentCaptureDeviceBrowserTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  ClearCapturedFramesQueue();

  // Run any left-over tasks (usually these are delete-soon's and orphaned
  // tasks).
  RunUntilIdle();

  ContentBrowserTest::TearDownOnMainThread();
}

std::unique_ptr<HttpResponse>
ContentCaptureDeviceBrowserTestBase::HandleRequest(const HttpRequest& request) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_content_type("text/html");
  const GURL& url = request.GetURL();
  if (url.path() == kOuterFramePath) {
    // A page with a solid white fill color, but containing an iframe in its
    // upper-left quadrant.
    const GURL& inner_frame_url =
        embedded_test_server()->GetURL(kInnerFrameHostname, kInnerFramePath);
    response->set_content(base::StringPrintf(
        "<!doctype html>\n"
        "<html>\n"
        "<style>\n"
        "* { border:none; margin:none; padding:none; }\n"
        "html, body { min-width:100vw; background-color:#ffffff; }\n"
        "iframe { position:absolute; top:0px; left:0px; "
        "border:none; margin:none; padding:none; }\n"
        "</style>\n"
        "<body>\n"
        "<iframe src='%s' width=50%% height=50%%>\n"
        "</iframe>\n"
        "</body>\n"
        "</html>\n",
        inner_frame_url.spec().c_str()));
  } else {
    // A page whose solid fill color is based on a query parameter, or
    // defaults to black.
    const std::string& query = url.query();
    std::string color = "#000000";
    const auto pos = query.find("color=");
    if (pos != std::string::npos) {
      color = "#" + query.substr(pos + 6, 6);
    }
    response->set_content(
        base::StringPrintf("<!doctype html>"
                           "<body style='background-color: %s;'></body>",
                           color.c_str()));
  }
  return std::move(response);
}

// static
constexpr char ContentCaptureDeviceBrowserTestBase::kInnerFrameHostname[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kInnerFramePath[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kOuterFrameHostname[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kOuterFramePath[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kSingleFrameHostname[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kSingleFramePath[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kAlternateHostname[];
// static
constexpr char ContentCaptureDeviceBrowserTestBase::kAlternatePath[];

}  // namespace content
