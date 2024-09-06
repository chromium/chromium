// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/devtools/protocol/system_info.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_doh_server.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/origin.h"

#if BUILDFLAG(IS_POSIX)
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/tracing/perfetto_task_runner.h"
#include "services/tracing/perfetto/system_test_utils.h"
#endif

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

using testing::ElementsAre;
using testing::Eq;

namespace content {

namespace {

const int kBudgetAllowed = 12;

class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager() {}

  TestJavaScriptDialogManager(const TestJavaScriptDialogManager&) = delete;
  TestJavaScriptDialogManager& operator=(const TestJavaScriptDialogManager&) =
      delete;

  ~TestJavaScriptDialogManager() override {}

  void Handle() {
    if (!callback_.is_null()) {
      std::move(callback_).Run(true, std::u16string());
    } else {
      handle_ = true;
    }
  }

  // WebContentsDelegate
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  // JavaScriptDialogManager
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    if (handle_) {
      handle_ = false;
      std::move(callback).Run(true, std::u16string());
    } else {
      callback_ = std::move(callback);
    }
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {}

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override {
    is_handled_ = true;
    return true;
  }

  void CancelDialogs(WebContents* web_contents,
                     bool reset_state) override {}

  bool is_handled() { return is_handled_; }

 private:
  DialogClosedCallback callback_;
  bool handle_ = false;
  bool is_handled_ = false;
};

}  // namespace

class SitePerProcessDevToolsProtocolTest : public DevToolsProtocolTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }
};

class SyntheticKeyEventTest : public DevToolsProtocolTest {
 protected:
  void SendKeyEvent(const std::string& type,
                    int modifier,
                    int windowsKeyCode,
                    int nativeKeyCode,
                    const std::string& key,
                    bool wait) {
    base::Value::Dict params;
    params.Set("type", type);
    params.Set("modifiers", modifier);
    params.Set("windowsVirtualKeyCode", windowsKeyCode);
    params.Set("nativeVirtualKeyCode", nativeKeyCode);
    params.Set("key", key);
    SendCommand("Input.dispatchKeyEvent", std::move(params), wait);
  }
};

class PrerenderDevToolsProtocolTest : public DevToolsProtocolTest {
 public:
  PrerenderDevToolsProtocolTest() {
    prerender_helper_ = std::make_unique<test::PrerenderTestHelper>(
        base::BindRepeating(&PrerenderDevToolsProtocolTest::web_contents,
                            base::Unretained(this)));
  }

  GURL GetUrl(const std::string& path) {
    return embedded_test_server()->GetURL("a.test", path);
  }

  bool HasHostForUrl(const GURL& url) {
    FrameTreeNodeId host_id = prerender_helper_->GetHostForUrl(url);
    return !!host_id;
  }

  FrameTreeNodeId AddPrerender(const GURL& prerendering_url) {
    return prerender_helper_->AddPrerender(prerendering_url);
  }

  RenderFrameHostImpl* GetPrerenderedMainFrameHost(FrameTreeNodeId host_id) {
    return static_cast<RenderFrameHostImpl*>(
        prerender_helper_->GetPrerenderedMainFrameHost(host_id));
  }

  void NavigatePrimaryPage(const GURL& url) {
    prerender_helper_->NavigatePrimaryPage(url);
  }

  // WebContentsDelegate overrides.
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  std::string AttachToTabTargetAndGetSessionId() {
    AttachToTabTarget(shell()->web_contents());
    shell()->web_contents()->SetDelegate(this);

    {
      base::Value::Dict params;
      params.Set("discover", true);
      SendCommandSync("Target.setDiscoverTargets", std::move(params));
    }

    std::string frame_target_id;
    for (int targetCount = 1; true; targetCount++) {
      base::Value::Dict result;
      result = WaitForNotification("Target.targetCreated", true);
      if (*result.FindStringByDottedPath("targetInfo.type") == "page") {
        frame_target_id =
            std::string(*result.FindStringByDottedPath("targetInfo.targetId"));
        break;
      }
      CHECK_LT(targetCount, 2);
    }

    {
      base::Value::Dict params;
      params.Set("targetId", frame_target_id);
      params.Set("flatten", true);
      const base::Value::Dict* result =
          SendCommandSync("Target.attachToTarget", std::move(params));
      CHECK(result);
      std::string session_id(*result->FindString("sessionId"));
      CHECK(session_id != "");
      return session_id;
    }
  }

 private:
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;
};

class SyntheticMouseEventTest : public DevToolsProtocolTest {
 public:
  SyntheticMouseEventTest() {
// On Android, zoom level is set to 0 in
// WebContentsImpl::GetPendingPageZoomLevel unless the kAccessibilityPageZoom
// feature is enabled. We enable it to be able to test mouse events across all
// platforms.
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndEnableFeature(features::kAccessibilityPageZoom);
#endif
  }

 protected:
  void SendMouseEvent(const std::string& type,
                      int x,
                      int y,
                      const std::string& button,
                      bool wait) {
    base::Value::Dict params;
    params.Set("type", type);
    params.Set("x", x);
    params.Set("y", y);
    if (!button.empty()) {
      params.Set("button", button);
      params.Set("clickCount", 1);
    }
    SendCommand("Input.dispatchMouseEvent", std::move(params), wait);
  }

  void InitMouseDownLog() {
    ASSERT_TRUE(
        content::ExecJs(shell()->web_contents(),
                        "logs = []; window.addEventListener('mousedown', e => "
                        "logs.push(`${e.type},${e.clientX},${e.clientY}`));"));
  }

  std::string GetMouseDownLog() {
    return content::EvalJs(shell()->web_contents(), "window.logs.join(';')")
        .ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyntheticKeyEventTest, KeyEventSynthesizeKey) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "function handleKeyEvent(event) {"
      "domAutomationController.send(event.key);"
      "}"
      "document.body.addEventListener('keydown', handleKeyEvent);"
      "document.body.addEventListener('keyup', handleKeyEvent);"));

  DOMMessageQueue dom_message_queue(shell()->web_contents());

  // Send enter (keycode 13).
  SendKeyEvent("rawKeyDown", 0, 13, 13, "Enter", true);
  SendKeyEvent("keyUp", 0, 13, 13, "Enter", true);

  std::string key;
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Enter\"", key);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Enter\"", key);

  // Send escape (keycode 27).
  SendKeyEvent("rawKeyDown", 0, 27, 27, "Escape", true);
  SendKeyEvent("keyUp", 0, 27, 27, "Escape", true);

  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Escape\"", key);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key));
  EXPECT_EQ("\"Escape\"", key);
}

// Flaky: https://crbug.com/889878
IN_PROC_BROWSER_TEST_F(SyntheticKeyEventTest, DISABLED_KeyboardEventAck) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "document.body.addEventListener('keydown', () => {debugger;});"));

  auto filter = std::make_unique<InputMsgWatcher>(
      RenderWidgetHostImpl::From(shell()
                                     ->web_contents()
                                     ->GetPrimaryMainFrame()
                                     ->GetRenderViewHost()
                                     ->GetWidget()),
      blink::WebInputEvent::Type::kRawKeyDown);

  SendCommandSync("Debugger.enable");
  SendKeyEvent("rawKeyDown", 0, 13, 13, "Enter", false);

  // We expect that the debugger message event arrives *before* the input
  // event ack, and the subsequent command response for Input.dispatchKeyEvent.
  WaitForNotification("Debugger.paused");
  EXPECT_FALSE(filter->HasReceivedAck());
  EXPECT_EQ(1, received_responses_count());

  SendCommandSync("Debugger.resume");
  filter->WaitForAck();
  EXPECT_EQ(3, received_responses_count());
}

// Flaky: https://crbug.com/1263461
IN_PROC_BROWSER_TEST_F(SyntheticMouseEventTest, DISABLED_MouseEventAck) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "document.body.addEventListener('mousedown', () => {debugger;});"));

  auto filter = std::make_unique<InputMsgWatcher>(
      RenderWidgetHostImpl::From(shell()
                                     ->web_contents()
                                     ->GetPrimaryMainFrame()
                                     ->GetRenderViewHost()
                                     ->GetWidget()),
      blink::WebInputEvent::Type::kMouseDown);

  SendCommandSync("Debugger.enable");
  SendMouseEvent("mousePressed", 15, 15, "left", false);

  // We expect that the debugger message event arrives *before* the input
  // event ack, and the subsequent command response for
  // Input.dispatchMouseEvent.
  WaitForNotification("Debugger.paused");
  EXPECT_FALSE(filter->HasReceivedAck());
  EXPECT_EQ(1, received_responses_count());

  SendCommandSync("Debugger.resume");
  filter->WaitForAck();
  EXPECT_EQ(3, received_responses_count());
}

IN_PROC_BROWSER_TEST_F(SyntheticMouseEventTest, MouseEventCoordinates) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/zoom.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  InitMouseDownLog();
  // In about 1 out of 1000 runs, the event gets lost on the way to the
  // renderer. We repeat the event dispatch until it succeeds since we want to
  // test event coordinates.
  while (GetMouseDownLog() == "") {
    SendMouseEvent("mousePressed", 15, 15, "left", true);
  }
  ASSERT_EQ("mousedown,15,15", GetMouseDownLog());
}

IN_PROC_BROWSER_TEST_F(SyntheticMouseEventTest, MouseEventCoordinatesWithZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/zoom.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  SendCommandSync("Page.enable");
  InitMouseDownLog();

  HostZoomMap* host_zoom_map =
      HostZoomMap::GetForWebContents(shell()->web_contents());
  host_zoom_map->SetZoomLevelForHost(test_url.host(),
                                     blink::ZoomFactorToZoomLevel(2.5));
  WaitForNotification("Page.frameResized", true);

  // In about 1 out of 1000 runs, the event gets lost on the way to the
  // renderer. We repeat the event dispatch until it succeeds since we want to
  // test event coordinates.
  while (GetMouseDownLog() == "") {
    SendMouseEvent("mousePressed", 15, 15, "left", true);
  }
  ASSERT_EQ("mousedown,15,15", GetMouseDownLog());
}

namespace {
bool DecodePNG(std::string base64_data, SkBitmap* bitmap) {
  std::string png_data;
  if (!base::Base64Decode(base64_data, &png_data))
    return false;
  return gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(png_data.data()), png_data.size(),
      bitmap);
}

std::unique_ptr<SkBitmap> DecodeJPEG(std::string base64_data) {
  std::string jpeg_data;
  if (!base::Base64Decode(base64_data, &jpeg_data))
    return nullptr;
  return gfx::JPEGCodec::Decode(
      reinterpret_cast<unsigned const char*>(jpeg_data.data()),
      jpeg_data.size());
}

int ColorsSquareDiff(SkColor color1, SkColor color2) {
  auto a_diff = static_cast<int>(SkColorGetA(color1)) -
                static_cast<int>(SkColorGetA(color2));
  auto r_diff = static_cast<int>(SkColorGetR(color1)) -
                static_cast<int>(SkColorGetR(color2));
  auto g_diff = static_cast<int>(SkColorGetG(color1)) -
                static_cast<int>(SkColorGetG(color2));
  auto b_diff = static_cast<int>(SkColorGetB(color1)) -
                static_cast<int>(SkColorGetB(color2));
  return a_diff * a_diff + r_diff * r_diff + g_diff * g_diff +
             b_diff * b_diff;
}

bool ColorsMatchWithinLimit(SkColor color1, SkColor color2, int max_collor_diff) {
  return ColorsSquareDiff(color1, color2) <= max_collor_diff * max_collor_diff;
}

// Adapted from cc::ExactPixelComparator.
bool MatchesBitmap(const SkBitmap& expected_bmp,
                   const SkBitmap& actual_bmp,
                   const gfx::Rect& matching_mask,
                   float device_scale_factor,
                   int max_collor_diff) {
  // Number of pixels with an error
  int error_pixels_count = 0;

  gfx::Rect error_bounding_rect = gfx::Rect();

  // Scale expectations along with the mask.
  device_scale_factor = device_scale_factor ? device_scale_factor : 1;

  // Check that bitmaps have identical dimensions.
  int expected_width = round(expected_bmp.width() * device_scale_factor);
  int expected_height = round(expected_bmp.height() * device_scale_factor);
  EXPECT_EQ(expected_width, actual_bmp.width());
  EXPECT_EQ(expected_height, actual_bmp.height());
  if (expected_width != actual_bmp.width() ||
      expected_height != actual_bmp.height()) {
    return false;
  }

  DCHECK(gfx::SkIRectToRect(actual_bmp.bounds()).Contains(matching_mask));

  for (int x = matching_mask.x(); x < matching_mask.right(); ++x) {
    for (int y = matching_mask.y(); y < matching_mask.bottom(); ++y) {
      SkColor actual_color =
          actual_bmp.getColor(x * device_scale_factor, y * device_scale_factor);
      SkColor expected_color = expected_bmp.getColor(x, y);
      if (!ColorsMatchWithinLimit(actual_color, expected_color, max_collor_diff)) {
        if (error_pixels_count < 10) {
          LOG(ERROR) << "Pixel (" << x << "," << y
                     << "). Expected: " << std::hex << expected_color
                     << ", actual: " << actual_color << std::dec
                     << ", square diff: " << ColorsSquareDiff(expected_color, actual_color);
        }
        error_pixels_count++;
        error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
      }
    }
  }

  if (error_pixels_count != 0) {
    LOG(ERROR) << "Number of pixel with an error: " << error_pixels_count;
    LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
    return false;
  }

  return true;
}
}  // namespace

enum class ScreenshotEncoding { PNG, JPEG, WEBP };

std::string EncodingEnumToString(ScreenshotEncoding encoding) {
  switch (encoding) {
    case ScreenshotEncoding::PNG:
      return "png";
    case ScreenshotEncoding::JPEG:
      return "jpeg";
    case ScreenshotEncoding::WEBP:
      return "webp";
    default:
      return "";
  }
}

class CaptureScreenshotTest : public DevToolsProtocolTest {
 protected:
  std::unique_ptr<SkBitmap> CaptureScreenshot(
      ScreenshotEncoding encoding,
      bool from_surface,
      const gfx::RectF& clip = gfx::RectF(),
      float clip_scale = 0,
      bool capture_beyond_viewport = false,
      bool expect_error = false) {
    base::Value::Dict params;
    params.Set("format", EncodingEnumToString(encoding));
    params.Set("quality", 100);
    params.Set("fromSurface", from_surface);
    if (capture_beyond_viewport) {
      params.Set("captureBeyondViewport", true);
    }
    if (clip_scale) {
      base::Value::Dict clip_value;
      clip_value.Set("x", clip.x());
      clip_value.Set("y", clip.y());
      clip_value.Set("width", clip.width());
      clip_value.Set("height", clip.height());
      clip_value.Set("scale", clip_scale);
      params.Set("clip", std::move(clip_value));
    }
    SendCommandSync("Page.captureScreenshot", std::move(params));

    std::unique_ptr<SkBitmap> result_bitmap;
    if (expect_error && error()) {
      EXPECT_THAT(error()->FindInt("code"),
                  testing::Optional(
                      static_cast<int>(crdtp::DispatchCode::SERVER_ERROR)));
    } else {
      const std::string* base64 = result()->FindString("data");
      if (encoding == ScreenshotEncoding::PNG) {
        result_bitmap = std::make_unique<SkBitmap>();
        EXPECT_TRUE(DecodePNG(*base64, result_bitmap.get()));
      } else if (encoding == ScreenshotEncoding::JPEG) {
        result_bitmap = DecodeJPEG(*base64);
      } else {
        // Decode not implemented.
      }
      EXPECT_TRUE(result_bitmap);
    }
    return result_bitmap;
  }

  void CaptureScreenshotAndCompareTo(const SkBitmap& expected_bitmap,
                                     ScreenshotEncoding encoding,
                                     bool from_surface,
                                     float device_scale_factor = 0,
                                     const gfx::RectF& clip = gfx::RectF(),
                                     float clip_scale = 0,
                                     bool capture_beyond_viewport = false) {
    std::unique_ptr<SkBitmap> result_bitmap = CaptureScreenshot(
        encoding, from_surface, clip, clip_scale, capture_beyond_viewport);

    gfx::Rect matching_mask(gfx::SkIRectToRect(expected_bitmap.bounds()));
#if BUILDFLAG(IS_MAC)
    // Mask out the corners, which may be drawn differently on Mac because of
    // rounded corners.
    matching_mask.Inset(4);
#endif

    // A color profile can be installed on the host that could affect
    // pixel colors. Also JPEG compression could further distort the color.
    // Allow some error between actual and expected pixel values.
    // That assumes there is no shift in pixel positions, so it only works
    // reliably if all pixels have equal values.
    int max_collor_diff = 20;

    EXPECT_TRUE(MatchesBitmap(expected_bitmap, *result_bitmap, matching_mask,
                              device_scale_factor, max_collor_diff));
  }

  gfx::Size GetPageContentSize() {
    const base::Value::Dict* content_size =
        SendCommandSync("Page.getLayoutMetrics")->FindDict("cssContentSize");
    return gfx::Size(content_size->FindInt("width").value(),
                     content_size->FindInt("height").value());
  }

  // We compare against the actual physical backing size rather than the
  // view size, because the view size is stored adjusted for DPI and only in
  // integer precision.
  gfx::Size GetViewSize() {
    return static_cast<RenderWidgetHostViewBase*>(
               shell()->web_contents()->GetRenderWidgetHostView())
        ->GetCompositorViewportPixelSize();
  }

  // We compare the bitmap with the captured screenshot to verify the
  // size and color of the screenshot.
  SkBitmap GenerateBitmap(gfx::Size size, SkColor color) {
    SkBitmap expected_bitmap;
    expected_bitmap.allocN32Pixels(size.width(), size.height());
    expected_bitmap.eraseColor(color);
    return expected_bitmap;
  }

  void SetDefaultBackgroundColorOverride(int r, int g, int b, float a) {
    auto params = base::Value::Dict();
    base::Value::Dict color;
    color.Set("r", r);
    color.Set("g", g);
    color.Set("b", b);
    color.Set("a", a);
    params.Set("color", std::move(color));
    SendCommandSync("Emulation.setDefaultBackgroundColorOverride",
                    std::move(params));
  }

  void SetDeviceMetricsOverride(int width,
                                int height,
                                float device_scale_factor,
                                bool mobile,
                                std::optional<bool> fitWindow) {
    auto params = base::Value::Dict();
    params.Set("width", width);
    params.Set("height", height);
    params.Set("deviceScaleFactor", device_scale_factor);
    params.Set("mobile", mobile);
    if (fitWindow.has_value()) {
      params.Set("fitWindow", fitWindow.value());
    }
    SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params));
  }

  // Takes a screenshot of a colored box that is positioned inside the frame.
  void PlaceAndCaptureBox(const gfx::Size& frame_size,
                          const gfx::Size& box_size,
                          float screenshot_scale,
                          float device_scale_factor) {
    static const int kBoxOffsetHeight = 100;
    const gfx::Size scaled_box_size =
        ScaleToFlooredSize(box_size, screenshot_scale);
    base::Value::Dict params;

    VLOG(1) << "Testing screenshot of box with size " << box_size.width() << "x"
            << box_size.height() << "px at scale " << screenshot_scale
            << " ...";

    // Draw a blue box of provided size in the horizontal center of the page.
    EXPECT_TRUE(content::ExecJs(
        shell()->web_contents(),
        base::StringPrintf(
            "var style = document.body.style;                             "
            "style.overflow = 'hidden';                                   "
            "style.minHeight = '%dpx';                                    "
            "style.backgroundImage = 'linear-gradient(#0000ff, #0000ff)'; "
            "style.backgroundSize = '%dpx %dpx';                          "
            "style.backgroundPosition = '50%% %dpx';                      "
            "style.backgroundRepeat = 'no-repeat';                        ",
            box_size.height() + kBoxOffsetHeight, box_size.width(),
            box_size.height(), kBoxOffsetHeight)));

    // Force frame size: The offset of the blue box within the frame shouldn't
    // change during screenshotting. This verifies that the page doesn't observe
    // a change in frame size as a side effect of screenshotting.
    SetDeviceMetricsOverride(frame_size.width(), frame_size.height(),
                             device_scale_factor, false, std::nullopt);

    // Resize frame to scaled blue box size.
    gfx::RectF clip;
    clip.set_width(box_size.width());
    clip.set_height(box_size.height());
    clip.set_x((frame_size.width() - box_size.width()) / 2.);
    clip.set_y(kBoxOffsetHeight);

    // Capture screenshot and verify that it is indeed blue.
    SkBitmap expected_bitmap =
        GenerateBitmap(scaled_box_size, SkColorSetRGB(0x00, 0x00, 0xff));

    // If the device scale factor is 0,
    // get the original device scale factor to compare with
    if (!device_scale_factor) {
      device_scale_factor = display::Screen::GetScreen()
                                ->GetPrimaryDisplay()
                                .device_scale_factor();
    }

    CaptureScreenshotAndCompareTo(expected_bitmap, ScreenshotEncoding::PNG,
                                  /*from_surface=*/true, device_scale_factor,
                                  clip, screenshot_scale);

    // Reset for next screenshot.
    SendCommandSync("Emulation.clearDeviceMetricsOverride");
  }

  bool IsTrusted() override { return is_trusted_; }

  bool is_trusted_ = true;

 private:
#if !BUILDFLAG(IS_ANDROID)
  void SetUp() override {
    EnablePixelOutput();
    DevToolsProtocolTest::SetUp();
  }
#endif
};

IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       CaptureScreenshotBeyondViewport_OutOfView) {
  // TODO(crbug.com/40488022) This test fails consistently on low-end Android
  // devices.
  if (base::SysInfo::IsLowEndDevice())
    return;

  // Load dummy page before getting the view size.
  shell()->LoadURL(GURL("data:text/html,"));
  gfx::Size view_size = GetViewSize();

  // Make a page a bit bigger than the view to force scrollbars to be shown.
  shell()->LoadURL(
      GURL(base::StringPrintf("data:text/html,"
                              R"(<body style='background:%%23123456;height:%dpx;
                         width:%dpx'></body>)",
                              /*content_height*/ view_size.height() + 10,
                              /*content_width*/ view_size.width() + 10)));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  // Generate expected screenshot without any scrollbars.
  gfx::Size actual_page_size = GetPageContentSize();
  SkBitmap expected_bitmap =
      GenerateBitmap(actual_page_size, SkColorSetRGB(0x12, 0x34, 0x56));

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  // Verify there are no scrollbars on the screenshot.
  CaptureScreenshotAndCompareTo(
      expected_bitmap, ScreenshotEncoding::PNG, /*from_surface=*/true,
      device_scale_factor,
      /*clip=*/
      gfx::RectF(0, 0, actual_page_size.width(), actual_page_size.height()),
      /*clip_scale=*/1, true);
  CaptureScreenshotAndCompareTo(expected_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);
}

IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       CaptureScreenshotBeyondViewport_IFrame) {
  // TODO(crbug.com/40488022) This test fails consistently on low-end Android
  // devices.
  if (base::SysInfo::IsLowEndDevice())
    return;

  // Load dummy page before getting the view size.
  shell()->LoadURL(GURL("data:text/html,"));
  gfx::Size view_size = GetViewSize();

  // Make a page a bit bigger than the view to force scrollbars to be shown.
  int content_height = view_size.height() + 50;
  int content_width = view_size.width() + 50;
  int margin = 100;
  shell()->LoadURL(
      GURL(base::StringPrintf("data:text/html,"
                              R"(
            <body style=' height:%dpx;
                          width:%dpx;'>
            <iframe style=" height:%dpx;
                            width:%dpx;
                            background:%%23123456;
                            border:none;">
            </iframe></body>
          )",
                              content_height + margin, content_width + margin,
                              content_height, content_width)));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  // Generate expected screenshot without any scrollbars.
  SkBitmap expected_bitmap =
      GenerateBitmap(gfx::Size(content_width, content_height),
                     SkColorSetRGB(0x12, 0x34, 0x56));

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  // Verify there are no scrollbars on the screenshot.
  // Even if margin is 0 then the iframe appears 8px away from beginning of the
  // page
  CaptureScreenshotAndCompareTo(
      expected_bitmap, ScreenshotEncoding::PNG, /*from_surface=*/true,
      device_scale_factor,
      /*clip=*/gfx::RectF(8, 8, content_width, content_height),
      /*clip_scale=*/1, /*capture_beyond_viewport=*/true);
}

// ChromeOS and Android has fading out scrollbars, which makes the test flacky.
// TODO(crbug.com/40157725) Android has a problem with changing scale.
// TODO(crbug.com/40156819) Android Lollipop has a problem with capturing
// screenshot.
// TODO(crbug.com/40736077) Flaky on linux-lacros-tester-rel
// TODO(crbug.com/40815512): Failing on MacOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_CaptureScreenshotBeyondViewport_InnerScrollbarsAreShown \
  DISABLED_CaptureScreenshotBeyondViewport_InnerScrollbarsAreShown
#else
#define MAYBE_CaptureScreenshotBeyondViewport_InnerScrollbarsAreShown \
  CaptureScreenshotBeyondViewport_InnerScrollbarsAreShown
#endif
IN_PROC_BROWSER_TEST_F(
    CaptureScreenshotTest,
    MAYBE_CaptureScreenshotBeyondViewport_InnerScrollbarsAreShown) {
  // TODO(crbug.com/40488022) This test fails consistently on low-end Android
  // devices.
  if (base::SysInfo::IsLowEndDevice())
    return;

  shell()->LoadURL(GURL(
      "data:text/html,<body><div style='width: 50px; height: 50px; overflow: "
      "scroll;'><h3>test</h3><h3>test</h3><h3>test</h3></div></body>"));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  gfx::Size view_size = GetViewSize();

  // Capture a screenshot not "from surface", meaning without emulation and
  // without changing preferences, as-is.
  std::unique_ptr<SkBitmap> expected_bitmap =
      CaptureScreenshot(ScreenshotEncoding::PNG, false);

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  // Compare the captured screenshot with one made "from_surface", where actual
  // scrollbar magic happened, and verify it looks the same, meaning the
  // internal scrollbars are rendered.
  CaptureScreenshotAndCompareTo(
      *expected_bitmap, ScreenshotEncoding::PNG, /*from_surface=*/true,
      device_scale_factor,
      /*clip=*/gfx::RectF(0, 0, view_size.width(), view_size.height()),
      /*clip_scale=*/1, /*capture_beyond_viewport=*/true);
}

// ChromeOS and Android don't support software compositing.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

class NoGPUCaptureScreenshotTest : public CaptureScreenshotTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CaptureScreenshotTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableGpuCompositing);
  }
};

// Tests that large screenshots are composited fine with software compositor.
// Regression test for https://crbug.com/1137291.
// Flaky on Linux.  http://crbug.com/1301176
#if BUILDFLAG(IS_LINUX)
#define MAYBE_LargeScreenshot DISABLED_LargeScreenshot
#else
#define MAYBE_LargeScreenshot LargeScreenshot
#endif
IN_PROC_BROWSER_TEST_F(NoGPUCaptureScreenshotTest, MAYBE_LargeScreenshot) {
  // This test fails consistently on low-end Android devices.
  // See crbug.com/653637.
  // TODO(eseckler): Reenable with error limit if necessary.
  if (base::SysInfo::IsLowEndDevice())
    return;
  // If disabling software compositing is disabled by the test caller,
  // we're out of luck.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareCompositingFallback)) {
    return;
  }
  shell()->LoadURL(
      GURL("data:text/html,"
           "<style>body,html { padding: 0; margin: 0; }</style>"
           "<div style='width: 1250px; height: 8440px; "
           "     background: linear-gradient(red, blue)'></div>"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDeviceMetricsOverride(/*width=*/1280, /*height=*/8440,
                           /*device_scale_factor=*/1,
                           /*mobile=*/false,
                           /*fitWindow=*/std::nullopt);
  auto bitmap = CaptureScreenshot(ScreenshotEncoding::PNG, true,
                                  gfx::RectF(0, 0, 1280, 8440), 1);
  SendCommandSync("Emulation.clearDeviceMetricsOverride");

  EXPECT_EQ(1280, bitmap->width());
  EXPECT_EQ(8440, bitmap->height());

  // Top-left is red-ish.
  SkColor top_left = bitmap->getColor(0, 0);
  EXPECT_GT(static_cast<int>(SkColorGetR(top_left)), 128);
  EXPECT_LT(static_cast<int>(SkColorGetB(top_left)), 128);

  // Bottom-left is blue-ish.
  SkColor bottom_left = bitmap->getColor(0, 8339);
  EXPECT_LT(static_cast<int>(SkColorGetR(bottom_left)), 128);
  EXPECT_GT(static_cast<int>(SkColorGetB(bottom_left)), 128);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

// Setting frame size (through RWHV) is not supported on Android.
// This test seems to be very flaky on all platforms: https://crbug.com/801173
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, DISABLED_CaptureScreenshotArea) {
  static const gfx::Size kFrameSize(800, 600);

  shell()->LoadURL(GURL("about:blank"));
  Attach();

  // Test capturing a subarea inside the emulated frame at different scales.
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 1.0, 1.);
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 2.0, 1.);
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 0.5, 1.);

  // Check non-1 device scale factor.
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 1.0, 2.);
  // Ensure not emulating device scale factor works.
  PlaceAndCaptureBox(kFrameSize, gfx::Size(100, 200), 1.0, 0.);
}

// Verifies that setDefaultBackgroundColorOverride changes the background color
// of a page that does not specify one.
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       SetDefaultBackgroundColorOverride) {
  if (base::SysInfo::IsLowEndDevice())
    return;

  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDefaultBackgroundColorOverride(/*r=*/0x00, /*g=*/0x00, /*b=*/0xff,
                                    /*a=*/1.0);

  gfx::Size view_size = GetViewSize();
  SkBitmap expected_bitmap =
      GenerateBitmap(view_size, SkColorSetRGB(0x00, 0x00, 0xff));
  CaptureScreenshotAndCompareTo(expected_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true);

  // Tests that resetting Emulation.setDefaultBackgroundColorOverride
  // clears the background color override.
  SendCommandSync("Emulation.setDefaultBackgroundColorOverride");
  expected_bitmap.eraseColor(SK_ColorWHITE);
  CaptureScreenshotAndCompareTo(expected_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true);
}

// Bellow tests verify that setDefaultBackgroundColor and captureScreenshot
// support a fully and semi-transparent background,
// and that setDeviceMetricsOverride doesn't affect it.
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, TransparentScreenshotsViewport) {
  if (base::SysInfo::IsLowEndDevice())
    return;

  shell()->LoadURL(
      GURL("data:text/html,<body style='background:transparent'></body>"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDefaultBackgroundColorOverride(/*r=*/0, /*g=*/0, /*b=*/0, /*a=*/0);

  gfx::Size view_size = GetViewSize();
  SkBitmap expected_viewport_bitmap =
      GenerateBitmap(view_size, SK_ColorTRANSPARENT);

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true);

#if !BUILDFLAG(IS_ANDROID)

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  // Check that device emulation does not affect the transparency.
  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0,
                           /*mobile=*/false,
                           /*fitWindow=*/false);
  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
#endif  // !BUILDFLAG(IS_ANDROID)

  SetDefaultBackgroundColorOverride(/*r=*/255, /*g=*/0, /*b=*/0,
                                    /*a=*/1.0 / 255 * 16);

  expected_viewport_bitmap.eraseColor(SkColorSetARGB(16, 255, 0, 0));

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true);

#if !BUILDFLAG(IS_ANDROID)
  // Check that device emulation does not affect the transparency.

  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0, /*mobile=*/false,
                           /*fitWindow=*/false);

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
#endif  // !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
// TODO(crbug.com/40876878): Fix this failing test
#if BUILDFLAG(IS_ANDROID)
                       DISABLED_TransparentScreenshotsBeyondViewport) {
#else
                       TransparentScreenshotsBeyondViewport) {
#endif
  if (base::SysInfo::IsLowEndDevice())
    return;

  shell()->LoadURL(
      GURL("data:text/html,<body style='background:transparent'></body>"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDefaultBackgroundColorOverride(/*r=*/0, /*g=*/0, /*b=*/0, /*a=*/0);

  gfx::Size view_size = GetViewSize();

  // When capturing full page screenshots, the page content size can differ
  // from the defined dimensions. Therefore, we need to check for the actual
  // layout metrics of the page and compare that with our result.
  SkBitmap expected_full_page_bitmap =
      GenerateBitmap(GetPageContentSize(), SK_ColorTRANSPARENT);
  SkBitmap expected_clip_bitmap =
      GenerateBitmap(view_size, SK_ColorTRANSPARENT);

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  gfx::RectF clip;
  clip.SetRect(0, 0, view_size.width(), view_size.height());

  // checks for beyond_viewport
  CaptureScreenshotAndCompareTo(expected_clip_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                clip, /*clip_scale=*/1,
                                /*capture_beyond_viewport=*/true);
  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

#if !BUILDFLAG(IS_ANDROID)

  // Check that device emulation does not affect the transparency.
  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0,
                           /*mobile=*/false,
                           /*fitWindow=*/false);

  // checks for beyond_viewport
  CaptureScreenshotAndCompareTo(expected_clip_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                clip,
                                /*clip_scale=*/1,
                                /*capture_beyond_viewport=*/true);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
#endif  // !BUILDFLAG(IS_ANDROID)

  SetDefaultBackgroundColorOverride(/*r=*/255, /*g=*/0, /*b=*/0,
                                    /*a=*/1.0 / 255 * 16);

  // When capturing full page screenshots, the page content size can differ
  // from the defined dimensions. Therefore, we need to check for the actual
  // layout metrics of the page and compare that with our result.
  expected_full_page_bitmap =
      GenerateBitmap(GetPageContentSize(), SkColorSetARGB(16, 255, 0, 0));

  expected_clip_bitmap =
      GenerateBitmap(view_size, SkColorSetARGB(16, 255, 0, 0));

  // Check for beyond-viewport
  CaptureScreenshotAndCompareTo(expected_clip_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                clip,
                                /*clip_scale=*/1,
                                /*capture_beyond_viewport=*/true);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

#if !BUILDFLAG(IS_ANDROID)
  // Check that device emulation does not affect the transparency.

  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0, /*mobile=*/false,
                           /*fitWindow=*/false);

  // Checks for beyond_viewport
  CaptureScreenshotAndCompareTo(expected_clip_bitmap, ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/clip,
                                /*clip_scale=*/1,
                                /*capture_beyond_viewport=*/true);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
#endif  // !BUILDFLAG(IS_ANDROID)
}

// TODO(crbug.com/40239673): Semi-transparent screenshots of viewport fail on
// android devices - a scrollbar is showing.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, TransparentScreenshotsFull) {
  if (base::SysInfo::IsLowEndDevice())
    return;

  shell()->LoadURL(
      GURL("data:text/html,<body style='background:transparent'></body>"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDefaultBackgroundColorOverride(/*r=*/0, /*g=*/0, /*b=*/0, /*a=*/0);

  gfx::Size view_size = GetViewSize();
  SkBitmap expected_viewport_bitmap =
      GenerateBitmap(view_size, SK_ColorTRANSPARENT);

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true);  //.

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  gfx::RectF clip;
  clip.SetRect(0, 0, view_size.width(), view_size.height());

  // checks for beyond_viewport
  CaptureScreenshotAndCompareTo(
      expected_viewport_bitmap, ScreenshotEncoding::PNG,
      /*from_surface=*/true, device_scale_factor, clip, /*clip_scale=*/1,
      /*capture_beyond_viewport=*/true);

  // When capturing full page screenshots, the page content size can differ
  // from the defined dimensions. Therefore, we need to check for the actual
  // layout metrics of the page and compare that with our result.
  SkBitmap expected_full_page_bitmap =
      GenerateBitmap(GetPageContentSize(), SK_ColorTRANSPARENT);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  // Check that device emulation does not affect the transparency.
  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0,
                           /*mobile=*/false,
                           /*fitWindow=*/false);

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor);

  // checks for beyond_viewport
  CaptureScreenshotAndCompareTo(
      expected_viewport_bitmap, ScreenshotEncoding::PNG,
      /*from_surface=*/true, device_scale_factor, clip,
      /*clip_scale=*/1,
      /*capture_beyond_viewport=*/true);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");

  SetDefaultBackgroundColorOverride(/*r=*/255, /*g=*/0, /*b=*/0,
                                    /*a=*/1.0 / 255 * 16);

  expected_viewport_bitmap.eraseColor(SkColorSetARGB(16, 255, 0, 0));

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true);

  // Check for beyond-viewport
  CaptureScreenshotAndCompareTo(
      expected_viewport_bitmap, ScreenshotEncoding::PNG,
      /*from_surface=*/true, device_scale_factor, clip,
      /*clip_scale=*/1,
      /*capture_beyond_viewport=*/true);

  // When capturing full page screenshots, the page content size can differ
  // from the defined dimensions. Therefore, we need to check for the actual
  // layout metrics of the page and compare that with our result.
  expected_full_page_bitmap =
      GenerateBitmap(GetPageContentSize(), SkColorSetARGB(16, 255, 0, 0));

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  // Check that device emulation does not affect the transparency.

  SetDeviceMetricsOverride(view_size.width(), view_size.height(),
                           /*device_scale_factor=*/0, /*mobile=*/false,
                           /*fitWindow=*/false);

  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor);

  // Checks for beyond_viewport
  CaptureScreenshotAndCompareTo(expected_viewport_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/clip,
                                /*clip_scale=*/1,
                                /*capture_beyond_viewport=*/true);

  CaptureScreenshotAndCompareTo(expected_full_page_bitmap,
                                ScreenshotEncoding::PNG,
                                /*from_surface=*/true, device_scale_factor,
                                /*clip=*/gfx::RectF(), /*clip_scale=*/0,
                                /*capture_beyond_viewport=*/true);

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Verifies that CaptureScreenshotsBeyondViewport supports emulation with the
// use of setDeviceMetricsOverride and setDefaultBackgroundColorOverride
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       CaptureScreenshotBeyondViewport_Emulation) {
  // TODO(crbug.com/40488022) This test fails consistently on low-end Android
  // devices.
  if (base::SysInfo::IsLowEndDevice())
    return;

  // Load dummy page before getting the view size.
  shell()->LoadURL(GURL("data:text/html,"));

  gfx::Size view_size = GetViewSize();

  // Make a page a bigger than the view to have fullpage behaviour.
  int content_height = view_size.height() + 100;
  int content_width = view_size.width() + 100;

  shell()->LoadURL(
      GURL(base::StringPrintf("data:text/html,"
                              R"(<body style='background:%%23123456;height:%dpx;
                              width:%dpx'></body>)",
                              content_height, content_width)));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  SetDefaultBackgroundColorOverride(/*r=*/0x12, /*g=*/0x34, /*b=*/0x56,
                                    /*a=*/1.0);

  float device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  // Check device emulation.
  // Additionally checks if emulation doesnt affect color change
  SetDeviceMetricsOverride(content_width, content_height,
                           /*device_scale_factor=*/0, /*mobile=*/false,
                           /*fitWindow=*/false);
  gfx::Size actual_page_size = GetPageContentSize();
  SkBitmap expected_bitmap =
      GenerateBitmap(actual_page_size, SkColorSetRGB(0x12, 0x34, 0x56));

  // Test for no Clip
  CaptureScreenshotAndCompareTo(
      expected_bitmap, ScreenshotEncoding::PNG,
      /*from_surface=*/true, device_scale_factor, /*clip=*/gfx::RectF(),
      /*clip_scale=*/0, /*capture_beyond_viewport=*/true);
  // Test for Clip
  CaptureScreenshotAndCompareTo(
      expected_bitmap, ScreenshotEncoding::PNG, /*from_surface=*/true,
      device_scale_factor,
      /*clip=*/
      gfx::RectF(0, 0, actual_page_size.width(), actual_page_size.height()),
      /*clip_scale=*/1, /*capture_beyond_viewport=*/true);
  SendCommandSync("Emulation.clearDeviceMetricsOverride");
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest,
                       OnlyScreenshotsFromSurfaceWhenUnsafeNotAllowed) {
  is_trusted_ = false;
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  Attach();

  CaptureScreenshot(ScreenshotEncoding::PNG, false, gfx::RectF(), 0, true,
                    true);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NoCrashDeviceMetricsOverrideAutoResize) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GURL("data:text/html,<body></body>"), 1);

  // Enable auto resize.
  gfx::Size min_size(10, 10);
  gfx::Size max_size(100, 100);

  RenderWidgetHostImpl* rwh = static_cast<RenderWidgetHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  rwh->GetView()->EnableAutoResize(min_size, max_size);

  Attach();

  // Send command.
  auto params = base::Value::Dict();
  params.Set("width", 50);
  params.Set("height", 50);
  params.Set("deviceScaleFactor", 1);
  params.Set("mobile", false);
  ASSERT_FALSE(
      SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params)));

  // Should not crash and should return an error.
  EXPECT_EQ(*error()->FindInt("code"),
            static_cast<int>(crdtp::DispatchCode::SERVER_ERROR));
  EXPECT_EQ(*error()->FindString("message"),
            "Target does not support metrics override");
}

#if BUILDFLAG(IS_ANDROID)
// Disabled, see http://crbug.com/469947.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizePinchGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int old_width = EvalJs(shell(), "window.innerWidth").ExtractInt();

  int old_height = EvalJs(shell(), "window.innerHeight").ExtractInt();

  base::Value::Dict params;
  params.Set("x", old_width / 2);
  params.Set("y", old_height / 2);
  params.Set("scaleFactor", 2.0);
  SendCommandSync("Input.synthesizePinchGesture", std::move(params));

  int new_width = EvalJs(shell(), "window.innerWidth").ExtractInt();
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_width) / new_width);

  int new_height = EvalJs(shell(), "window.innerHeight").ExtractInt();
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_height) / new_height);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizeScrollGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  ASSERT_EQ(0, EvalJs(shell(), "document.body.scrollTop"));

  base::Value::Dict params;
  params.Set("x", 0);
  params.Set("y", 0);
  params.Set("xDistance", 0);
  params.Set("yDistance", -100);
  SendCommandSync("Input.synthesizeScrollGesture", std::move(params));

  ASSERT_EQ(100, EvalJs(shell(), "document.body.scrollTop"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizeTapGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  ASSERT_EQ(0, EvalJs(shell(), "document.body.scrollTop"));

  base::Value::Dict params;
  params.Set("x", 16);
  params.Set("y", 16);
  params.Set("gestureSourceType", "touch");
  SendCommandSync("Input.synthesizeTapGesture", std::move(params));

  // The link that we just tapped should take us to the bottom of the page. The
  // new value of |document.body.scrollTop| will depend on the screen dimensions
  // of the device that we're testing on, but in any case it should be greater
  // than 0.
  ASSERT_GT(EvalJs(shell(), "document.body.scrollTop").ExtractInt(), 0);
}
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/40825729): Flaky on multiple bots.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_PageCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  base::Value::Dict command_params;
  command_params.Set("discover", true);
  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));

  base::Value::Dict params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  std::string target_id = *params.FindStringByDottedPath("targetInfo.targetId");

  ClearNotifications();
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    SendCommandAsync("Page.crash");
    params = WaitForNotification("Target.targetCrashed", true);
  }
  EXPECT_EQ(*params.FindString("targetId"), target_id);

  ClearNotifications();
  shell()->LoadURL(test_url);
  WaitForNotification("Inspector.targetReloadedAfterCrash", true);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PageCrashInFrame DISABLED_PageCrashInFrame
#else
#define MAYBE_PageCrashInFrame PageCrashInFrame
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsProtocolTest,
                       MAYBE_PageCrashInFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      embedded_test_server()->GetURL("/devtools/page-with-oopif.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  base::Value::Dict command_params;
  command_params.Set("discover", true);
  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));

  base::Value::Dict params;
  std::string frame_target_id;
  for (int targetCount = 1; true; targetCount++) {
    params = WaitForNotification("Target.targetCreated", true);
    if (*params.FindStringByDottedPath("targetInfo.type") == "iframe") {
      frame_target_id = *params.FindStringByDottedPath("targetInfo.targetId");
      break;
    }
    ASSERT_LT(targetCount, 2);
  }

  command_params = base::Value::Dict();
  command_params.Set("targetId", frame_target_id);
  command_params.Set("flatten", true);
  const base::Value::Dict* result =
      SendCommandSync("Target.attachToTarget", std::move(command_params));
  ASSERT_TRUE(result);
  const std::string* session_id = result->FindString("sessionId");
  ASSERT_TRUE(session_id);

  ClearNotifications();
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    SendSessionCommand("Page.crash", base::Value::Dict(), *session_id, false);
    params = WaitForNotification("Target.targetCrashed", true);
  }
  EXPECT_EQ(frame_target_id, *params.FindString("targetId"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, PageCrashClearsPendingCommands) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  base::Value::Dict command_params;
  command_params.Set("discover", true);

  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));

  base::Value::Dict params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  std::string target_id = *params.FindStringByDottedPath("targetInfo.targetId");

  SendCommandSync("Debugger.enable");

  command_params = base::Value::Dict();
  command_params.Set("expression", "console.log('first page'); debugger");
  SendCommandAsync("Runtime.evaluate", std::move(command_params));
  WaitForNotification("Debugger.paused");

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    params = WaitForNotification("Target.targetCrashed", true);
  }
  ClearNotifications();
  SendCommandAsync("Page.reload");
  WaitForNotification("Inspector.targetReloadedAfterCrash", true);
  command_params = base::Value::Dict();
  command_params.Set("expression", "console.log('second page')");
  SendCommandSync("Runtime.evaluate", std::move(command_params));
  EXPECT_THAT(console_messages_, ElementsAre("first page", "second page"));
}

// TODO(crbug.com/40811521): Disabled due to flakiness. Flaky on mac and linux
// la-cros
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       DISABLED_NavigationPreservesMessages) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  SendCommandAsync("Page.enable");

  base::Value::Dict params;
  test_url = GetTestUrl("devtools", "navigation.html");
  params.Set("url", test_url.spec());
  TestNavigationObserver navigation_observer(shell()->web_contents());
  SendCommandSync("Page.navigate", std::move(params));
  navigation_observer.Wait();

  EXPECT_GE(received_responses_count(), 2);
  EXPECT_TRUE(HasExistingNotification("Page.frameStartedLoading"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NavigationToFileUrlRequiresFileAccess) {
  Attach();

  base::Value::Dict params;
  GURL test_url = GetTestUrl("devtools", "navigation.html");
  params.Set("url", test_url.spec());
  ASSERT_TRUE(SendCommandSync("Page.navigate", params.Clone()));

  Detach();
  SetMayReadLocalFiles(false);

  Attach();

  ASSERT_FALSE(SendCommandSync("Page.navigate", params.Clone()));
  EXPECT_THAT(
      error()->FindInt("code"),
      testing::Optional(static_cast<int>(crdtp::DispatchCode::SERVER_ERROR)));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CrossSiteNoDetach) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url1 = embedded_test_server()->GetURL(
      "A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();

  GURL test_url2 = embedded_test_server()->GetURL(
      "B.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url2, 1);

  EXPECT_FALSE(HasExistingNotification());
}

// TODO(crbug.com/40811670): Flaky on MacOS.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_CrossSiteNavigation) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();
  SendCommandAsync("Page.enable");

  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html");
  base::Value::Dict params;
  params.Set("url", test_url2.spec());
  const base::Value::Dict* result =
      SendCommandSync("Page.navigate", std::move(params));
  const std::string* frame_id = result->FindString("frameId");

  base::Value::Dict frame_stopped =
      WaitForNotification("Page.frameStoppedLoading", true);
  EXPECT_EQ(*frame_stopped.FindString("frameId"), *frame_id);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CrossSiteCrash) {
  set_agent_host_can_close();
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();
  CrashTab(shell()->web_contents());

  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url2, 1);

  // Should not crash at this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, InspectorTargetCrashedNavigate) {
  set_agent_host_can_close();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");

  NavigateToURLBlockUntilNavigationsComplete(shell(), url_a, 1);
  Attach();
  SendCommandSync("Inspector.enable");

  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    WaitForNotification("Inspector.targetCrashed");
  }

  ClearNotifications();
  shell()->LoadURL(url_a);
  WaitForNotification("Inspector.targetReloadedAfterCrash", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, TargetGetTargetsAfterCrash) {
  set_agent_host_can_close();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");

  NavigateToURLBlockUntilNavigationsComplete(shell(), url_a, 1);
  Attach();
  SendCommandSync("Inspector.enable");
  SendCommandSync("Target.getTargets");
  EXPECT_EQ(1u, result()->FindList("targetInfos")->size());

  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    WaitForNotification("Inspector.targetCrashed");
  }

  SendCommandSync("Target.getTargets");
  EXPECT_EQ(1u, result()->FindList("targetInfos")->size());
}

// Same as in DevToolsProtocolTest.InspectorTargetCrashedNavigate, but with a
// cross-process navigation at the end.
// Regression test for https://crbug.com/990315
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       InspectorTargetCrashedNavigateCrossProcess) {
  set_agent_host_can_close();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");

  NavigateToURLBlockUntilNavigationsComplete(shell(), url_a, 1);
  Attach();
  SendCommandSync("Inspector.enable");

  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    WaitForNotification("Inspector.targetCrashed");
  }

  ClearNotifications();
  shell()->LoadURL(url_b);
  WaitForNotification("Inspector.targetReloadedAfterCrash", true);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InspectorTargetCrashedReload DISABLED_InspectorTargetCrashedReload
#else
#define MAYBE_InspectorTargetCrashedReload InspectorTargetCrashedReload
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       MAYBE_InspectorTargetCrashedReload) {
  set_agent_host_can_close();
  GURL url = GURL("data:text/html,<body></body>");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  Attach();
  SendCommandSync("Inspector.enable");

  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    WaitForNotification("Inspector.targetCrashed");
  }

  ClearNotifications();
  SendCommandAsync("Page.reload");
  WaitForNotification("Inspector.targetReloadedAfterCrash", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ReconnectPreservesState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);

  Shell* second = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(second, test_url, 1);

  Attach();
  SendCommandSync("Runtime.enable");

  agent_host_->DisconnectWebContents();
  agent_host_->ConnectWebContents(second->web_contents());
  WaitForNotification("Runtime.executionContextsCleared");
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CrossSitePauseInBeforeUnload) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html"), 1);
  Attach();
  SendCommandSync("Debugger.enable");

  ASSERT_TRUE(content::ExecJs(
      shell(),
      "window.onbeforeunload = function() { debugger; return null; }"));

  shell()->LoadURL(
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html"));
  WaitForNotification("Debugger.paused");
  TestNavigationObserver observer(shell()->web_contents(), 1);
  SendCommandSync("Debugger.resume");
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, InspectDuringFrameSwap) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell(), "window.open('about:blank','foo');"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  agent_host_ = DevToolsAgentHost::GetOrCreateFor(new_shell->web_contents());
  agent_host_->AttachClient(this);

  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html");

  // After this navigation, if the bug exists, the process will crash.
  NavigateToURLBlockUntilNavigationsComplete(new_shell, test_url2, 1);

  // Ensure that the A.com process is still alive by executing a script in the
  // original tab.
  //
  // TODO(alexmos, nasko):  A better way to do this is to navigate the original
  // tab to another site, watch for process exit, and check whether there was a
  // crash. However, currently there's no way to wait for process exit
  // regardless of whether it's a crash or not.  RenderProcessHostWatcher
  // should be fixed to support waiting on both WATCH_FOR_PROCESS_EXIT and
  // WATCH_FOR_HOST_DESTRUCTION, and then used here.
  EXPECT_EQ(true, EvalJs(shell(), "!!window.open('', 'foo');"));

  GURL test_url3 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");

  // After this navigation, if the bug exists, the process will crash.
  NavigateToURLBlockUntilNavigationsComplete(new_shell, test_url3, 1);

  // Ensure that the A.com process is still alive by executing a script in the
  // original tab.
  EXPECT_EQ(true, EvalJs(shell(), "!!window.open('', 'foo');"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DoubleCrash) {
  set_agent_host_can_close();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  SendCommandSync("ServiceWorker.enable");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  CrashTab(shell()->web_contents());
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  CrashTab(shell()->web_contents());
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  // Should not crash at this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ReloadBlankPage) {
  Shell* window =  Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(),
      GURL("javascript:x=1"),
      nullptr,
      gfx::Size());
  WaitForLoadStop(window->web_contents());
  Attach();
  SendCommandAsync("Page.reload");
  // Should not crash at this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, EvaluateInBlankPage) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  base::Value::Dict params;
  params.Set("expression", "window");
  SendCommandSync("Runtime.evaluate", std::move(params));
  EXPECT_FALSE(result()->Find("exceptionDetails"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
    EvaluateInBlankPageAfterNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  base::Value::Dict params;
  params.Set("expression", "window");
  SendCommandSync("Runtime.evaluate", std::move(params));
  EXPECT_FALSE(result()->Find("exceptionDetails"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, JavaScriptDialogNotifications) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  TestJavaScriptDialogManager dialog_manager;
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->SetDelegate(&dialog_manager);
  SendCommandSync("Page.enable");

  base::Value::Dict params;
  params.Set("expression", "prompt('hello?', 'default')");
  SendCommandAsync("Runtime.evaluate", std::move(params));

  base::Value::Dict notification =
      WaitForNotification("Page.javascriptDialogOpening");
  EXPECT_EQ(*notification.FindString("url"), "about:blank");
  EXPECT_EQ(*notification.FindString("message"), "hello?");
  EXPECT_EQ(*notification.FindString("type"), "prompt");
  EXPECT_EQ(*notification.FindString("defaultPrompt"), "default");

  params = base::Value::Dict();
  params.Set("accept", true);
  params.Set("promptText", "hi!");
  SendCommandAsync("Page.handleJavaScriptDialog", std::move(params));

  notification = WaitForNotification("Page.javascriptDialogClosed", true);
  EXPECT_THAT(notification.FindBool("result"), testing::Optional(true));

  EXPECT_TRUE(dialog_manager.is_handled());

  EXPECT_THAT(*notification.FindString("userInput"), Eq("hi!"));
  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, JavaScriptDialogInterop) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  TestJavaScriptDialogManager dialog_manager;
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->SetDelegate(&dialog_manager);
  SendCommandSync("Page.enable");
  SendCommandSync("Runtime.enable");

  base::Value::Dict params;
  params.Set("expression", "alert('42')");
  SendCommandAsync("Runtime.evaluate", std::move(params));
  WaitForNotification("Page.javascriptDialogOpening");

  dialog_manager.Handle();
  WaitForNotification("Page.javascriptDialogClosed", true);
  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, PageDisableWithOpenedDialog) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  TestJavaScriptDialogManager dialog_manager;
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->SetDelegate(&dialog_manager);

  SendCommandSync("Page.enable");
  SendCommandSync("Runtime.enable");

  base::Value::Dict params;
  params.Set("expression", "alert('42')");
  SendCommandAsync("Runtime.evaluate", std::move(params));
  WaitForNotification("Page.javascriptDialogOpening");
  EXPECT_TRUE(wc->IsJavaScriptDialogShowing());

  EXPECT_FALSE(dialog_manager.is_handled());
  SendCommandAsync("Page.disable");
  EXPECT_TRUE(wc->IsJavaScriptDialogShowing());
  EXPECT_FALSE(dialog_manager.is_handled());
  dialog_manager.Handle();
  EXPECT_FALSE(wc->IsJavaScriptDialogShowing());

  params = base::Value::Dict();
  params.Set("expression", "42");
  SendCommandSync("Runtime.evaluate", std::move(params));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, PageDisableWithNoDialogManager) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->SetDelegate(nullptr);

  SendCommandSync("Page.enable");
  SendCommandSync("Runtime.enable");

  base::Value::Dict params;
  params.Set("expression", "alert('42');");
  SendCommandAsync("Runtime.evaluate", std::move(params));
  WaitForNotification("Page.javascriptDialogOpening");
  EXPECT_TRUE(wc->IsJavaScriptDialogShowing());

  SendCommandSync("Page.disable");
  EXPECT_FALSE(wc->IsJavaScriptDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, BeforeUnloadDialog) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  TestJavaScriptDialogManager dialog_manager;

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  wc->SetDelegate(&dialog_manager);
  SendCommandSync("Runtime.enable");

  base::Value::Dict params;

  params = base::Value::Dict();
  params.Set("expression", "window.onbeforeunload=()=>{return 'prompt';}");
  params.Set("userGesture", true);
  SendCommandSync("Runtime.evaluate", std::move(params));

  SendCommandSync("Page.enable");
  SendCommandAsync("Page.reload");

  base::Value::Dict notification =
      WaitForNotification("Page.javascriptDialogOpening", true);

  EXPECT_THAT(*notification.FindString("url"), Eq("about:blank"));
  EXPECT_THAT(*notification.FindString("type"), Eq("beforeunload"));

  params = base::Value::Dict();
  params.Set("accept", true);
  SendCommandAsync("Page.handleJavaScriptDialog", std::move(params));
  WaitForNotification("Page.javascriptDialogClosed", true);
  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, BrowserCreateAndCloseTarget) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  EXPECT_EQ(1u, shell()->windows().size());
  base::Value::Dict params;
  params.Set("url", "about:blank");
  SendCommandSync("Target.createTarget", std::move(params));
  const std::string* target_id = result()->FindString("targetId");
  ASSERT_TRUE(target_id);
  EXPECT_EQ(2u, shell()->windows().size());

  // TODO(eseckler): Since the `blink::WebView` is closed asynchronously, we
  // currently don't verify that the command actually closes the shell.
  params = base::Value::Dict();
  params.Set("targetId", *target_id);
  SendCommandSync("Target.closeTarget", std::move(params));

  EXPECT_THAT(result()->FindBool("success"), testing::Optional(true));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, BrowserGetTargets) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  SendCommandSync("Target.getTargets");
  const base::Value::List* target_infos = result()->FindList("targetInfos");
  ASSERT_TRUE(target_infos);
  EXPECT_EQ(1u, target_infos->size());
  const base::Value& target_info_value = target_infos->front();
  const base::Value::Dict* target_info = target_info_value.GetIfDict();
  ASSERT_TRUE(target_info);
  const std::string* target_id = target_info->FindString("target_id");
  const std::string* type = target_info->FindString("type");
  const std::string* title = target_info->FindString("title");
  const std::string* url = target_info->FindString("url");
  EXPECT_FALSE(target_id);
  ASSERT_TRUE(type);
  ASSERT_TRUE(title);
  ASSERT_TRUE(url);
  EXPECT_EQ("page", *type);
  EXPECT_EQ("about:blank", *title);
  EXPECT_EQ("about:blank", *url);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, VirtualTimeTest) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  base::Value::Dict params;
  params.Set("policy", "pause");
  SendCommandSync("Emulation.setVirtualTimePolicy", std::move(params));

  params = base::Value::Dict();
  params.Set("expression",
             "setTimeout(function(){console.log('before')}, 999);"
             "setTimeout(function(){console.log('at')}, 1000);"
             "setTimeout(function(){console.log('after')}, 1001);");
  SendCommandSync("Runtime.evaluate", std::move(params));

  // Let virtual time advance for one second.
  params = base::Value::Dict();
  params.Set("policy", "advance");
  params.Set("budget", 1000);
  SendCommandSync("Emulation.setVirtualTimePolicy", std::move(params));

  WaitForNotification("Emulation.virtualTimeBudgetExpired");

  params = base::Value::Dict();
  params.Set("expression", "console.log('done')");
  SendCommandSync("Runtime.evaluate", std::move(params));

  // The third timer should not fire.
  EXPECT_THAT(console_messages_, ElementsAre("before", "at", "done"));

  // Let virtual time advance for another second, which should make the third
  // timer fire.
  params = base::Value::Dict();
  params.Set("policy", "advance");
  params.Set("budget", 1000);
  SendCommandSync("Emulation.setVirtualTimePolicy", std::move(params));

  WaitForNotification("Emulation.virtualTimeBudgetExpired");

  EXPECT_THAT(console_messages_, ElementsAre("before", "at", "done", "after"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/devtools/navigation.html");
  base::Value::Dict command_params;

  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  Attach();
  SendCommandSync("Network.enable");
  SendCommandAsync("Security.enable");
  command_params = base::Value::Dict();
  command_params.Set("override", true);
  SendCommandSync("Security.setOverrideCertificateErrors",
                  std::move(command_params));

  // Test cancel.
  SendCommandSync("Network.clearBrowserCache");
  SendCommandSync("Network.clearBrowserCookies");
  TestNavigationObserver cancel_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  base::Value::Dict params =
      WaitForNotification("Security.certificateError", false);
  EXPECT_TRUE(shell()->web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(
      test_url,
      shell()->web_contents()->GetController().GetPendingEntry()->GetURL());
  command_params = base::Value::Dict();
  command_params.Set("eventId", *params.FindInt("eventId"));
  command_params.Set("action", "cancel");
  SendCommandAsync("Security.handleCertificateError",
                   std::move(command_params));
  cancel_observer.Wait();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());
  EXPECT_EQ(GURL("about:blank"), shell()
                                     ->web_contents()
                                     ->GetController()
                                     .GetLastCommittedEntry()
                                     ->GetURL());

  // Test continue.
  SendCommandSync("Network.clearBrowserCache");
  SendCommandSync("Network.clearBrowserCookies");
  TestNavigationObserver continue_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  params = WaitForNotification("Security.certificateError", false);
  command_params = base::Value::Dict();
  command_params.Set("eventId", *params.FindInt("eventId"));
  command_params.Set("action", "continue");
  SendCommandAsync("Security.handleCertificateError",
                   std::move(command_params));
  WaitForNotification("Network.loadingFinished", true);
  continue_observer.Wait();
  EXPECT_EQ(test_url, shell()
                          ->web_contents()
                          ->GetController()
                          .GetLastCommittedEntry()
                          ->GetURL());

  // Reset override.
  SendCommandSync("Security.disable");

  // Test ignoring all certificate errors.
  command_params = base::Value::Dict();
  command_params.Set("ignore", true);
  SendCommandSync("Security.setIgnoreCertificateErrors",
                  std::move(command_params));

  SendCommandSync("Network.clearBrowserCache");
  SendCommandSync("Network.clearBrowserCookies");
  TestNavigationObserver continue_observer2(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  WaitForNotification("Network.loadingFinished", true);
  continue_observer2.Wait();
  EXPECT_EQ(test_url, shell()
                          ->web_contents()
                          ->GetController()
                          .GetLastCommittedEntry()
                          ->GetURL());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       CertificateErrorRequestInterception) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/devtools/navigation.html");

  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  Attach();
  SendCommandSync("Network.enable");
  SendCommandAsync("Security.enable");
  SendCommandSync("Network.setRequestInterception",
                  std::move(base::JSONReader::Read(
                                "{\"patterns\": [{\"urlPattern\": \"*\"}]}")
                                ->GetDict()));

  SendCommandSync(
      "Security.setIgnoreCertificateErrors",
      std::move(base::JSONReader::Read("{\"ignore\": true}")->GetDict()));

  SendCommandSync("Network.clearBrowserCache");
  SendCommandSync("Network.clearBrowserCookies");
  TestNavigationObserver continue_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  base::Value::Dict params =
      WaitForNotification("Network.requestIntercepted", false);
  std::string interceptionId = *params.FindString("interceptionId");
  SendCommandAsync("Network.continueInterceptedRequest",
                   std::move(base::JSONReader::Read("{\"interceptionId\": \"" +
                                                    interceptionId + "\"}")
                                 ->GetDict()));
  continue_observer.Wait();
  EXPECT_EQ(test_url, shell()
                          ->web_contents()
                          ->GetController()
                          .GetLastCommittedEntry()
                          ->GetURL());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CertificateErrorBrowserTarget) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/devtools/navigation.html");
  base::Value::Dict params;
  base::Value::Dict command_params;

  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Clear cookies and cache to avoid interference with cert error events.
  Attach();
  SendCommandSync("Network.enable");
  SendCommandSync("Network.clearBrowserCache");
  SendCommandSync("Network.clearBrowserCookies");
  Detach();

  // Test that browser target can ignore cert errors.
  AttachToBrowserTarget();
  command_params = base::Value::Dict();
  command_params.Set("ignore", true);
  SendCommandSync("Security.setIgnoreCertificateErrors",
                  std::move(command_params));

  TestNavigationObserver continue_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  continue_observer.Wait();
  EXPECT_EQ(test_url, shell()
                          ->web_contents()
                          ->GetController()
                          .GetLastCommittedEntry()
                          ->GetURL());
}

class CertificateErrorIgnoredBrowserTargetTest : public DevToolsProtocolTest {
 public:
  CertificateErrorIgnoredBrowserTargetTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    DevToolsProtocolTest::SetUpOnMainThread();
    net::SSLServerConfig ssl_config;
    // ssl_config.client_cert_type =
    //     net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    // https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
    GURL test_url = https_server_.GetURL("/devtools/navigation.html");

    shell()->LoadURL(GURL("about:blank"));
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    // Create a second client to concurrently connect to browser target,
    // as the DevToolsProtocolTest class only has one client available to
    // connect.
    browser_client.AttachToBrowserTarget();
    base::Value::Dict command_params;
    command_params = base::Value::Dict();
    command_params.Set("ignore", true);
    browser_client.SendCommandSync("Security.setIgnoreCertificateErrors",
                                   std::move(command_params));

    // Connect the default client to the page.
    Attach();
    SendCommandSync("Debugger.enable");
    // Clear cookies and cache to avoid interference with cert error events.
    SendCommandSync("Network.enable");
    SendCommandSync("Network.clearBrowserCache");
    SendCommandSync("Network.clearBrowserCookies");

    TestNavigationObserver continue_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    continue_observer.Wait();

    EXPECT_EQ(test_url, shell()
                            ->web_contents()
                            ->GetController()
                            .GetLastCommittedEntry()
                            ->GetURL());
  }

  void TearDownOnMainThread() override {
    // Detach the additional client
    browser_client.DetachProtocolClient();
    DevToolsProtocolTest::TearDownOnMainThread();
  }

  ~CertificateErrorIgnoredBrowserTargetTest() override = default;

 protected:
  TestDevToolsProtocolClient browser_client;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(CertificateErrorIgnoredBrowserTargetTest,
                       CertificateErrorBrowserTargetServiceWorkerFetch) {
  // Install a service worker over bad HTTPS cert and wait for the controller to
  // change.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "navigator.serviceWorker.register('/devtools/service_worker.js');"
      "navigator.serviceWorker.oncontrollerchange = () => {debugger;};"));
  WaitForNotification("Debugger.paused");
  SendCommandSync("Debugger.resume");
  // Reload the page so that request is intercepted by SW.
  SendCommandSync("Page.reload");
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("intercepted",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(
    CertificateErrorIgnoredBrowserTargetTest,
    CertificateErrorBrowserTargetServiceWorkerImportScripts) {
  // Install a service worker over bad HTTPS cert and wait for the controller to
  // change.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "navigator.serviceWorker.register('/devtools/"
      "service_worker_import_classic.js');"
      "navigator.serviceWorker.oncontrollerchange = () => {debugger;};"));
  WaitForNotification("Debugger.paused");

  SendCommandSync("Debugger.resume");

  // Reload the page so that request is intercepted by SW.
  SendCommandSync("Page.reload");
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ("imported",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(CertificateErrorIgnoredBrowserTargetTest,
                       CertificateErrorBrowserTargetServiceWorkerModuleImport) {
  // Install a service worker over bad HTTPS cert and wait for the controller to
  // change.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "navigator.serviceWorker.register('/devtools/"
      "service_worker_import_module.js', {type: 'module'});"
      "navigator.serviceWorker.oncontrollerchange = () => {debugger;};"));
  WaitForNotification("Debugger.paused");

  SendCommandSync("Debugger.resume");

  // Reload the page so that request is intercepted by SW.
  SendCommandSync("Page.reload");
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ("imported",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(CertificateErrorIgnoredBrowserTargetTest,
                       CertificateErrorBrowserTargetDedicatedWorker) {
  // Install a dedicated worker over bad HTTPS cert.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "const myWorker = new Worker('/devtools/dedicated_worker.js');"
      "myWorker.onmessage = (msg) => {document.body.textContent = msg.data; "
      "debugger;};"
      "myWorker.postMessage('test');"));

  WaitForNotification("Debugger.paused");
  SendCommandSync("Debugger.resume");

  EXPECT_EQ("reply test",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(
    CertificateErrorIgnoredBrowserTargetTest,
    CertificateErrorBrowserTargetDedicatedWorkerImportClassic) {
  // Install a dedicated worker over bad HTTPS cert.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "const myWorker = new "
      "Worker('/devtools/dedicated_worker_import_classic.js');"
      "myWorker.onmessage = (msg) => {document.body.textContent = msg.data; "
      "debugger;};"
      "myWorker.postMessage('test');"));

  WaitForNotification("Debugger.paused");
  SendCommandSync("Debugger.resume");

  EXPECT_EQ("reply imported test",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(
    CertificateErrorIgnoredBrowserTargetTest,
    CertificateErrorBrowserTargetDedicatedWorkerImportModule) {
  // Install a dedicated worker over bad HTTPS cert.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "const myWorker = new "
      "Worker('/devtools/dedicated_worker_import_module.js', {type: 'module'});"
      "myWorker.onmessage = (msg) => {document.body.textContent = msg.data; "
      "debugger;};"
      "myWorker.postMessage('test');"));

  WaitForNotification("Debugger.paused");
  SendCommandSync("Debugger.resume");

  EXPECT_EQ("reply imported test",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

// SharedWorkers are not enabled on Android. https://crbug.com/154571
#if BUILDFLAG(IS_ANDROID)
constexpr bool kIsSharedWorkerEnabled = false;
#else
constexpr bool kIsSharedWorkerEnabled = true;
#endif

IN_PROC_BROWSER_TEST_F(CertificateErrorIgnoredBrowserTargetTest,
                       CertificateErrorBrowserTargetSharedWorker) {
  if (!kIsSharedWorkerEnabled) {
    return;
  }
  // Install a shared worker over bad HTTPS cert.
  base::Value::Dict params;
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "const myWorker = new SharedWorker('/devtools/shared_worker.js');"
      "myWorker.port.start();"
      "myWorker.port.onmessage = (msg) => {document.body.textContent = "
      "msg.data; debugger;};"
      "myWorker.port.postMessage('test');"));
  WaitForNotification("Debugger.paused");
  SendCommandSync("Debugger.resume");

  EXPECT_EQ("reply test",
            EvalJs(shell()->web_contents(), "document.body.textContent"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, SubresourceWithCertificateError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.ServeFilesFromSourceDirectory("content/test/data/devtools");
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/image.html");
  base::Value::Dict command_params;

  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  Attach();
  SendCommandAsync("Security.enable");
  command_params = base::Value::Dict();
  command_params.Set("override", true);
  SendCommandSync("Security.setOverrideCertificateErrors",
                  std::move(command_params));

  TestNavigationObserver observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);

  // Expect certificateError event for main frame.
  base::Value::Dict params =
      WaitForNotification("Security.certificateError", false);
  command_params = base::Value::Dict();
  command_params.Set("eventId", *params.FindInt("eventId"));
  command_params.Set("action", "continue");
  SendCommandAsync("Security.handleCertificateError",
                   std::move(command_params));

  // Expect certificateError event for image.
  params = WaitForNotification("Security.certificateError", false);
  command_params = base::Value::Dict();
  command_params.Set("eventId", *params.FindInt("eventId"));
  command_params.Set("action", "continue");
  SendCommandAsync("Security.handleCertificateError",
                   std::move(command_params));

  observer.Wait();
  EXPECT_EQ(test_url, shell()
                          ->web_contents()
                          ->GetController()
                          .GetLastCommittedEntry()
                          ->GetURL());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, TargetDiscovery) {
  std::set<std::string> ids;
  base::Value::Dict command_params;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL first_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), first_url, 1);

  GURL second_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  Shell* second = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(second, second_url, 1);

  Attach();
  int attached_count = 0;
  command_params = base::Value::Dict();
  command_params.Set("discover", true);
  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));
  base::Value::Dict params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  attached_count += *params.FindBoolByDottedPath("targetInfo.attached") ? 1 : 0;
  std::string target_id = *params.FindStringByDottedPath("targetInfo.targetId");
  EXPECT_THAT(ids.count(target_id), Eq(0u));
  ids.insert(target_id);
  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  attached_count += *params.FindBoolByDottedPath("targetInfo.attached") ? 1 : 0;
  target_id = *params.FindStringByDottedPath("targetInfo.targetId");
  EXPECT_THAT(ids.count(target_id), Eq(0u));
  ids.insert(target_id);
  EXPECT_FALSE(HasExistingNotification());
  EXPECT_EQ(1, attached_count);

  GURL third_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  Shell* third = CreateBrowser();
  NavigateToURLBlockUntilNavigationsComplete(third, third_url, 1);

  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  std::string attached_id =
      *params.FindStringByDottedPath("targetInfo.targetId");
  EXPECT_THAT(ids.count(attached_id), Eq(0u));
  ids.insert(attached_id);
  EXPECT_THAT(params.FindBoolByDottedPath("targetInfo.attached"),
              testing::Optional(false));

  params = WaitForNotification("Target.targetInfoChanged", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.url"),
              Eq("about:blank"));

  params = WaitForNotification("Target.targetInfoChanged", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.url"),
              Eq(third_url.spec()));
  EXPECT_FALSE(HasExistingNotification());

  second->Close();
  second = nullptr;
  params = WaitForNotification("Target.targetDestroyed", true);
  target_id = *params.FindString("targetId");
  EXPECT_THAT(ids.erase(target_id), Eq(1u));
  EXPECT_FALSE(HasExistingNotification());

  command_params = base::Value::Dict();
  command_params.Set("targetId", attached_id);
  SendCommandSync("Target.attachToTarget", std::move(command_params));
  params = WaitForNotification("Target.targetInfoChanged", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.targetId"),
              Eq(attached_id));
  EXPECT_THAT(params.FindBoolByDottedPath("targetInfo.attached"),
              testing::Optional(true));
  params = WaitForNotification("Target.attachedToTarget", true);
  std::string session_id = *params.FindString("sessionId");
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.targetId"),
              Eq(attached_id));
  EXPECT_FALSE(HasExistingNotification());

  WebContents::CreateParams create_params(
      ShellContentBrowserClient::Get()->browser_context());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  EXPECT_FALSE(HasExistingNotification());

  NavigateToURLBlockUntilNavigationsComplete(web_contents.get(), first_url, 1);
  // The notification does not come when there's no delegate.
  EXPECT_FALSE(HasExistingNotification("Target.targetCreated"));

  web_contents->SetDelegate(this);
  // Attaching a delegate causes the notification to be sent.
  params = WaitForNotification("Target.targetCreated", true);
  EXPECT_THAT(*params.FindStringByDottedPath("targetInfo.type"), Eq("page"));
  EXPECT_FALSE(HasExistingNotification());

  command_params = base::Value::Dict();
  command_params.Set("discover", false);
  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));
  EXPECT_FALSE(HasExistingNotification());

  command_params = base::Value::Dict();
  command_params.Set("sessionId", session_id);
  SendCommandSync("Target.detachFromTarget", std::move(command_params));
  params = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_THAT(*params.FindString("sessionId"), Eq(session_id));
  EXPECT_THAT(*params.FindString("targetId"), Eq(attached_id));
  EXPECT_FALSE(HasExistingNotification());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, SetAndGetCookies) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  // Set two cookies, one of which matches the loaded URL and another that
  // doesn't.
  base::Value::Dict command_params;
  command_params = base::Value::Dict();
  command_params.Set("url", test_url.spec());
  command_params.Set("name", "cookie_for_this_url");
  command_params.Set("value", "mendacious");
  SendCommandAsync("Network.setCookie", std::move(command_params));

  command_params = base::Value::Dict();
  command_params.Set("url", "https://www.chromium.org");
  command_params.Set("name", "cookie_for_another_url");
  command_params.Set("value", "polyglottal");
  SendCommandAsync("Network.setCookie", std::move(command_params));

  // First get the cookies for just the loaded URL.
  SendCommandSync("Network.getCookies");

  const base::Value::List* cookies = result()->FindList("cookies");
  ASSERT_TRUE(cookies);
  EXPECT_EQ(1u, cookies->size());

  const std::string* name = nullptr;
  const std::string* value = nullptr;
  {
    ASSERT_TRUE(cookies->front().is_dict());
    const base::Value::Dict& cookie = cookies->front().GetDict();
    name = cookie.FindString("name");
    value = cookie.FindString("value");
    ASSERT_TRUE(name);
    ASSERT_TRUE(value);
    EXPECT_EQ("cookie_for_this_url", *name);
    EXPECT_EQ("mendacious", *value);

    // Then get all the cookies in the cookie jar.
    SendCommandSync("Network.getAllCookies");

    cookies = result()->FindList("cookies");
    ASSERT_TRUE(cookies);
    EXPECT_EQ(2u, cookies->size());
  }

  // Note: the cookies will be returned in unspecified order.
  size_t found = 0;
  for (const base::Value& cookie_value : *cookies) {
    ASSERT_TRUE(cookie_value.is_dict());
    const base::Value::Dict& cookie = cookie_value.GetDict();
    name = cookie.FindString("name");
    value = cookie.FindString("value");
    ASSERT_TRUE(name);
    ASSERT_TRUE(value);
    if (*name == "cookie_for_this_url") {
      EXPECT_EQ("mendacious", *value);
      found++;
    } else if (*name == "cookie_for_another_url") {
      EXPECT_EQ("polyglottal", *value);
      found++;
    } else {
      FAIL();
    }
  }
  EXPECT_EQ(2u, found);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       ReturnsCookiesOnlyForAttachableUrls) {
  SetNotAttachableHosts({"b.test"});
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  std::string cookies_to_set = "/set-cookie?foo=bar";

  GURL url = embedded_test_server()->GetURL("b.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = embedded_test_server()->GetURL("c.test", cookies_to_set);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  url = embedded_test_server()->GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(b.test(),c.test())");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  Attach();
  const base::Value::List* storage_cookies =
      SendCommandSync("Storage.getCookies")->FindList("cookies");
  ASSERT_EQ(1ul, storage_cookies->size());
  EXPECT_EQ("foo", *storage_cookies->front().GetDict().FindString("name"));
  EXPECT_EQ("c.test", *storage_cookies->front().GetDict().FindString("domain"));

  const base::Value::List* network_all_cookies =
      SendCommandSync("Network.getAllCookies")->FindList("cookies");
  ASSERT_EQ(1ul, network_all_cookies->size());
  EXPECT_EQ("foo", *network_all_cookies->front().GetDict().FindString("name"));
  EXPECT_EQ("c.test",
            *network_all_cookies->front().GetDict().FindString("domain"));

  const base::Value::List* network_cookies_no_param =
      SendCommandSync("Network.getCookies")->FindList("cookies");
  ASSERT_EQ(1ul, network_cookies_no_param->size());
  EXPECT_EQ("foo",
            *network_cookies_no_param->front().GetDict().FindString("name"));
  EXPECT_EQ("c.test",
            *network_cookies_no_param->front().GetDict().FindString("domain"));

  base::Value::List urls;
  urls.Append(embedded_test_server()
                  ->GetURL("b.com", "/cross_site_iframe_factory.html?b.test()")
                  .spec());
  urls.Append(embedded_test_server()
                  ->GetURL("c.com", "/cross_site_iframe_factory.html?c.test()")
                  .spec());
  base::Value::Dict params;
  params.Set("urls", std::move(urls));
  const base::Value::List* network_cookies_with_param =
      SendCommandSync("Network.getAllCookies", std::move(params))
          ->FindList("cookies");
  ASSERT_EQ(1ul, network_cookies_with_param->size());
  EXPECT_EQ("foo",
            *network_cookies_with_param->front().GetDict().FindString("name"));
  EXPECT_EQ("c.test", *network_cookies_with_param->front().GetDict().FindString(
                          "domain"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       AutoAttachToOOPIFAfterNavigationStarted) {
  ASSERT_TRUE(embedded_test_server()->Start());
  IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                           {"b.com"});
  GURL a_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());

  // Create iframe and start navigation.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager navigation_manager(shell()->web_contents(), b_url);
  EXPECT_TRUE(ExecJs(
      main_frame, JsReplace("const iframe = document.createElement('iframe');\n"
                            "iframe.src = $1;\n"
                            "document.body.appendChild(iframe);\n",
                            b_url)));
  // Pause subframe navigation.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  // Attach to DevTools after subframe starts navigating (but before it
  // finishes).
  Attach();

  DevToolsAgentHostImpl* main_frame_agent =
      RenderFrameDevToolsAgentHost::GetFor(main_frame);
  EXPECT_NE(main_frame_agent, nullptr);

  // Start auto-attach.
  base::Value::Dict command_params;
  command_params = base::Value::Dict();
  command_params.Set("autoAttach", true);
  command_params.Set("waitForDebuggerOnStart", false);
  command_params.Set("flatten", true);
  SendCommandSync("Target.setAutoAttach", std::move(command_params));

  // Child frame should be created at this point, but isn't an OOPIF yet, so
  // shouldn't have its own DevToolsAgentHost yet.
  FrameTreeNode* child = main_frame->child_at(0);
  EXPECT_NE(child, nullptr);
  EXPECT_EQ(RenderFrameDevToolsAgentHost::GetFor(child), main_frame_agent);

  // Resume navigation.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Target for OOPIF should get attached.
  auto notification = WaitForNotification("Target.attachedToTarget", true);
  EXPECT_NE(RenderFrameDevToolsAgentHost::GetFor(child), main_frame_agent);
  EXPECT_THAT(notification.FindBool("waitingForDebugger"),
              testing::Optional(false));
}

class DevToolsProtocolDeviceEmulationTest : public DevToolsProtocolTest {
 public:
  ~DevToolsProtocolDeviceEmulationTest() override {}

  void EmulateDeviceSize(gfx::Size size) {
    base::Value::Dict params;
    params.Set("width", size.width());
    params.Set("height", size.height());
    params.Set("deviceScaleFactor", 0);
    params.Set("mobile", false);
    SendCommandSync("Emulation.setDeviceMetricsOverride", std::move(params));
  }

  gfx::Size GetViewSize() {
    return shell()
        ->web_contents()
        ->GetPrimaryMainFrame()
        ->GetView()
        ->GetViewBounds()
        .size();
  }
};

// Setting frame size (through RWHV) is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeviceSize DISABLED_DeviceSize
#else
#define MAYBE_DeviceSize DeviceSize
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolDeviceEmulationTest, MAYBE_DeviceSize) {
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();

  const gfx::Size original_size = GetViewSize();
  const gfx::Size emulated_size_1 =
      gfx::Size(original_size.width() - 50, original_size.height() - 50);
  const gfx::Size emulated_size_2 =
      gfx::Size(original_size.width() - 100, original_size.height() - 100);

  EmulateDeviceSize(emulated_size_1);
  EXPECT_EQ(emulated_size_1, GetViewSize());

  EmulateDeviceSize(emulated_size_2);
  EXPECT_EQ(emulated_size_2, GetViewSize());

  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url2, 1);
  EXPECT_EQ(emulated_size_2, GetViewSize());

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
  EXPECT_EQ(original_size, GetViewSize());
}

// Setting frame size (through RWHV) is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RenderKillDoesNotCrashBrowser \
  DISABLED_RenderKillDoesNotCrashBrowser
#else
#define MAYBE_RenderKillDoesNotCrashBrowser RenderKillDoesNotCrashBrowser
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolDeviceEmulationTest,
                       MAYBE_RenderKillDoesNotCrashBrowser) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  EmulateDeviceSize(gfx::Size(200, 200));

  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
    NavigateToURLBlockUntilNavigationsComplete(
        shell(), GURL(blink::kChromeUICrashURL), 1);
  }

  SendCommandSync("Emulation.clearDeviceMetricsOverride");
  // Should not crash at this point.
}

class DevToolsProtocolDeviceEmulationPrerenderTest
    : public DevToolsProtocolDeviceEmulationTest {
 public:
  DevToolsProtocolDeviceEmulationPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &DevToolsProtocolDeviceEmulationPrerenderTest::GetWebContents,
            base::Unretained(this))) {}
  ~DevToolsProtocolDeviceEmulationPrerenderTest() override = default;

  void SetUpOnMainThread() override {
    DevToolsProtocolDeviceEmulationTest::SetUpOnMainThread();
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
  }

  // WebContentsDelegate overrides.
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override {
    return PreloadingEligibility::kEligible;
  }

  WebContents* GetWebContents() const { return shell()->web_contents(); }

  std::string AttachToTabTargetAndGetSessionId() {
    AttachToTabTarget(shell()->web_contents());
    shell()->web_contents()->SetDelegate(this);

    {
      base::Value::Dict params;
      params.Set("discover", true);
      SendCommandSync("Target.setDiscoverTargets", std::move(params));
    }

    std::string frame_target_id;
    for (int targetCount = 1; true; targetCount++) {
      base::Value::Dict result;
      result = WaitForNotification("Target.targetCreated", true);
      if (*result.FindStringByDottedPath("targetInfo.type") == "page") {
        frame_target_id =
            std::string(*result.FindStringByDottedPath("targetInfo.targetId"));
        break;
      }
      CHECK_LT(targetCount, 2);
    }

    {
      base::Value::Dict params;
      params.Set("targetId", frame_target_id);
      params.Set("flatten", true);
      const base::Value::Dict* result =
          SendCommandSync("Target.attachToTarget", std::move(params));
      CHECK(result);
      std::string session_id(*result->FindString("sessionId"));
      CHECK(session_id != "");
      return session_id;
    }
  }

 protected:
  test::PrerenderTestHelper prerender_helper_;
};

// Setting frame size (through RWHV) is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeviceSize DISABLED_DeviceSize
#else
#define MAYBE_DeviceSize DeviceSize
#endif
IN_PROC_BROWSER_TEST_F(DevToolsProtocolDeviceEmulationPrerenderTest,
                       MAYBE_DeviceSize) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  std::string session_id = AttachToTabTargetAndGetSessionId();

  const gfx::Size original_size = GetViewSize();
  const gfx::Size emulated_size =
      gfx::Size(original_size.width() - 50, original_size.height() - 50);

  {
    const gfx::Size size = emulated_size;
    base::Value::Dict params;
    params.Set("width", size.width());
    params.Set("height", size.height());
    params.Set("deviceScaleFactor", 0);
    params.Set("mobile", false);
    SendSessionCommand("Emulation.setDeviceMetricsOverride", std::move(params),
                       session_id, true);
  }
  EXPECT_EQ(emulated_size, GetViewSize());

  // Start a prerender and ensure frame size isn't changed.
  GURL prerender_url =
      embedded_test_server()->GetURL("/devtools/navigation.html?prerender");
  prerender_helper_.AddPrerender(prerender_url);
  EXPECT_EQ(emulated_size, GetViewSize());

  // Activate the prerendered page and ensure frame size isn't changed.
  prerender_helper_.NavigatePrimaryPage(prerender_url);
  EXPECT_EQ(emulated_size, GetViewSize());

  SendSessionCommand("Emulation.clearDeviceMetricsOverride",
                     base::Value::Dict(), session_id, true);
  EXPECT_EQ(original_size, GetViewSize());
}

class DevToolsProtocolTouchTest : public DevToolsProtocolTest {
 public:
  ~DevToolsProtocolTouchTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionDisabled);
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTouchTest, EnableTouch) {
  base::Value::Dict params;

  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url1 =
      embedded_test_server()->GetURL("A.com", "/devtools/enable_touch.html");
  GURL test_url2 =
      embedded_test_server()->GetURL("B.com", "/devtools/enable_touch.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url1, 1);
  Attach();

  params = base::Value::Dict();
  SendCommandSync("Page.enable", std::move(params));

  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "checkProtos(false)"));

  params = base::Value::Dict();
  params.Set("enabled", true);
  SendCommandSync("Emulation.setTouchEmulationEnabled", std::move(params));
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "checkProtos(false)"));

  params = base::Value::Dict();
  params.Set("url", test_url2.spec());
  SendCommandAsync("Page.navigate", std::move(params));
  WaitForNotification("Page.frameStoppedLoading");
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "checkProtos(true)"));

  params = base::Value::Dict();
  params.Set("enabled", false);
  SendCommandSync("Emulation.setTouchEmulationEnabled", std::move(params));
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "checkProtos(true)"));

  params = base::Value::Dict();
  SendCommandAsync("Page.reload", std::move(params));
  WaitForNotification("Page.frameStoppedLoading");
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "checkProtos(false)"));
}

class DevToolsProtocolBackForwardCacheTest : public DevToolsProtocolTest {
 public:
  DevToolsProtocolBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~DevToolsProtocolBackForwardCacheTest() override = default;

  // content::WebContentsDelegate:
  bool IsBackForwardCacheSupported(
      content::WebContents& web_contents) override {
    return true;
  }

  std::string Evaluate(const std::string& script,
                       const base::Location& location) {
    base::Value::Dict params;
    params.Set("expression", script);
    SendCommandSync("Runtime.evaluate", std::move(params));
    const std::string* result_value =
        result()->FindStringByDottedPath("result.value");
    DCHECK(result_value) << "Valued to evaluate " << script << " from "
                         << location.ToString();
    return *result_value;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This test checks that the DevTools continue to work when the page is stored
// in and restored from back-forward cache. In particular:
// - that the session continues to be attached and the navigations are handled
// correctly.
// - when the old page is stored in the cache, the messages are still handled by
// the new page.
// - when the page is restored from the cache, it continues to handle protocol
// messages.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolBackForwardCacheTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A and inject some state.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(ExecJs(shell(), "var state = 'page1'"));

  // 2) Attach DevTools session.
  Attach();

  // 3) Extract the state via the DevTools protocol.
  EXPECT_EQ("page1", Evaluate("state", FROM_HERE));

  // 3) Navigate to B and inject some different state.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(ExecJs(shell(), "var state = 'page2'"));

  // 4) Ensure that the DevTools protocol commands are handled by the new page
  // (even though the old page is alive and is stored in the back-forward
  // cache).
  EXPECT_EQ("page2", Evaluate("state", FROM_HERE));

  // 5) Go back.
  shell()->web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 6) Ensure that the page has been restored from the cache and responds to
  // the DevTools commands.
  EXPECT_EQ("page1", Evaluate("state", FROM_HERE));
}

// Download tests are flaky on Android: https://crbug.com/7546
#if !BUILDFLAG(IS_ANDROID)
namespace {

static DownloadManagerImpl* DownloadManagerForShell(Shell* shell) {
  // We're in a content_browsertest; we know that the DownloadManager
  // is a DownloadManagerImpl.
  return static_cast<DownloadManagerImpl*>(
      shell->web_contents()->GetBrowserContext()->GetDownloadManager());
}

static void RemoveShellDelegate(Shell* shell) {
  content::ShellDownloadManagerDelegate* shell_delegate =
      static_cast<content::ShellDownloadManagerDelegate*>(
          DownloadManagerForShell(shell)->GetDelegate());
  shell_delegate->SetDownloadManager(nullptr);
  DownloadManagerForShell(shell)->SetDelegate(nullptr);
}

class CountingDownloadFile : public download::DownloadFileImpl {
 public:
  CountingDownloadFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer)
      : download::DownloadFileImpl(std::move(save_info),
                                   default_downloads_directory,
                                   std::move(stream),
                                   download_id,
                                   observer) {}

  ~CountingDownloadFile() override {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    active_files_--;
  }

  void Initialize(
      InitializeCallback callback,
      CancelRequestCallback cancel_request_callback,
      const download::DownloadItem::ReceivedSlices& received_slices) override {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    active_files_++;
    download::DownloadFileImpl::Initialize(std::move(callback),
                                           std::move(cancel_request_callback),
                                           received_slices);
  }

  static void GetNumberActiveFiles(int* result) {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    *result = active_files_;
  }

  // Can be called on any thread, and will block (running message loop)
  // until data is returned.
  static int GetNumberActiveFilesFromFileThread() {
    int result = -1;
    base::RunLoop run_loop;
    download::GetDownloadTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CountingDownloadFile::GetNumberActiveFiles, &result),
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
    DCHECK_NE(-1, result);
    return result;
  }

 private:
  static int active_files_;
};

int CountingDownloadFile::active_files_ = 0;

class CountingDownloadFileFactory : public download::DownloadFileFactory {
 public:
  CountingDownloadFileFactory() {}
  ~CountingDownloadFileFactory() override {}

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override {
    return new CountingDownloadFile(std::move(save_info),
                                    default_downloads_directory,
                                    std::move(stream), download_id, observer);
  }
};

// Get the next created download.
class DownloadCreateObserver : DownloadManager::Observer {
 public:
  explicit DownloadCreateObserver(DownloadManager* manager)
      : manager_(manager), item_(nullptr), received_item_response_(false) {
    manager_->AddObserver(this);
  }

  ~DownloadCreateObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* download) override {
    received_item_response_ = true;

    if (!item_)
      item_ = download;

    if (completion_closure_)
      std::move(completion_closure_).Run();
  }

  void OnDownloadDropped(DownloadManager* manager) override {
    received_item_response_ = true;

    item_ = nullptr;
    if (completion_closure_)
      std::move(completion_closure_).Run();
  }

  download::DownloadItem* WaitForFinished() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!received_item_response_) {
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return item_;
  }

 private:
  raw_ptr<DownloadManager> manager_;
  raw_ptr<download::DownloadItem> item_;
  bool received_item_response_;
  base::OnceClosure completion_closure_;
};

bool IsDownloadInState(download::DownloadItem::DownloadState state,
                       download::DownloadItem* item) {
  return item->GetState() == state;
}

class DevToolsDownloadContentTest : public DevToolsProtocolTest {
 protected:
  void SetUpOnMainThread() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    // Set shell default download manager to test proxy reset behavior.
    test_delegate_ = std::make_unique<ShellDownloadManagerDelegate>();
    test_delegate_->SetDownloadBehaviorForTesting(
        downloads_directory_.GetPath());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->GetDelegate()->Shutdown();
    manager->SetDelegate(test_delegate_.get());
    test_delegate_->SetDownloadManager(manager);

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetDownloadBehavior(const std::string& behavior) {
    base::Value::Dict params;
    params.Set("behavior", behavior);
    SendCommandSync("Page.setDownloadBehavior", std::move(params));

    EXPECT_GE(received_responses_count(), 1);
  }

  void SetDownloadBehavior(const std::string& behavior,
                           const std::string& download_path) {
    base::Value::Dict params;
    params.Set("behavior", behavior);
    params.Set("downloadPath", download_path);
    SendCommandSync("Page.setDownloadBehavior", std::move(params));

    EXPECT_GE(received_responses_count(), 1);
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverTerminal(
        download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Note: Cannot be used with other alternative DownloadFileFactorys
  void SetupEnsureNoPendingDownloads() {
    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        std::unique_ptr<download::DownloadFileFactory>(
            new CountingDownloadFileFactory()));
  }

  void WaitForCompletion(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::BindRepeating(&IsDownloadInState,
                                      download::DownloadItem::COMPLETE))
        .WaitForEvent();
  }

  bool EnsureNoPendingDownloads() {
    return CountingDownloadFile::GetNumberActiveFilesFromFileThread() == 0;
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const base::FilePath& path,
                  const std::string& value,
                  const int64_t file_size) {
    std::string file_contents;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      bool read = base::ReadFileToString(path, &file_contents);
      EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
      if (!read)
        return false;  // Couldn't read the file.
    }

    // Note: we don't handle really large files (more than size_t can hold)
    // so we will fail in that case.
    size_t expected_size = static_cast<size_t>(file_size);

    // Check the size.
    EXPECT_EQ(expected_size, file_contents.size());
    if (expected_size != file_contents.size())
      return false;

    // Check the contents.
    EXPECT_EQ(value, file_contents);
    if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0)
      return false;

    return true;
  }

  // Start a download and return the item.
  download::DownloadItem* StartDownloadAndReturnItem(Shell* shell, GURL url) {
    std::unique_ptr<DownloadCreateObserver> observer(
        new DownloadCreateObserver(DownloadManagerForShell(shell)));
    shell->LoadURL(url);
    return observer->WaitForFinished();
  }

 private:
  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
  std::unique_ptr<ShellDownloadManagerDelegate> test_delegate_;
};

}  // namespace

// Check that downloading a single file works.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, SingleDownload) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string download_path =
      temp_dir.GetPath().AppendASCII("download").AsUTF8Unsafe();

  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  SetDownloadBehavior("allow", download_path);
  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib"));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download->GetState());
  WaitForCompletion(download);
  ASSERT_EQ(download::DownloadItem::COMPLETE, download->GetState());
}

// Check that downloads can be cancelled gracefully.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, DownloadCancelled) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  SetDownloadBehavior("allow", "download");
  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   content::SlowDownloadHttpResponse::kUnknownSizeUrl));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download->GetState());

  // Cancel the download and wait for download system quiesce.
  download->Cancel(true);
  DownloadTestFlushObserver flush_observer(DownloadManagerForShell(shell()));
  flush_observer.WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that denying downloads works.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, DeniedDownload) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  SetDownloadBehavior("deny");
  // Create a download, wait and confirm it was cancelled.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib"));
  DownloadTestFlushObserver flush_observer(DownloadManagerForShell(shell()));
  flush_observer.WaitForFlush();
  EXPECT_TRUE(EnsureNoPendingDownloads());
  ASSERT_EQ(download::DownloadItem::CANCELLED, download->GetState());
}

// Check that defaulting downloads works as expected.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, DefaultDownload) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  SetDownloadBehavior("default");
  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   content::SlowDownloadHttpResponse::kUnknownSizeUrl));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download->GetState());

  // Cancel the download and wait for download system quiesce.
  download->Cancel(true);
  DownloadTestFlushObserver flush_observer(DownloadManagerForShell(shell()));
  flush_observer.WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that defaulting downloads cancels when there's no proxy
// download delegate.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, DefaultDownloadHeadless) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  RemoveShellDelegate(shell());

  SetDownloadBehavior("default");
  // Create a download, wait and confirm it was cancelled.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib"));
  DownloadTestFlushObserver flush_observer(DownloadManagerForShell(shell()));
  flush_observer.WaitForFlush();
  EXPECT_TRUE(EnsureNoPendingDownloads());
  ASSERT_EQ(download::DownloadItem::CANCELLED, download->GetState());
}

// Check that defaulting downloads cancels when there's no proxy
// download delegate.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest,
                       SetDownloadBehaviorAccessChecks) {
  SetMayWriteLocalFiles(false);
  Attach();
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Value::Dict params;
  params.Set("behavior", "allow");
  params.Set("downloadPath",
             temp_dir.GetPath().AppendASCII("download").AsUTF8Unsafe());

  SendCommandSync("Page.setDownloadBehavior", params.Clone());
  ASSERT_TRUE(error());
  EXPECT_EQ(*error()->FindString("message"), "Not allowed");
  Detach();
  SetMayWriteLocalFiles(true);
  Attach();
  SendCommandSync("Page.setDownloadBehavior", std::move(params));
  EXPECT_FALSE(error());
}

// Flaky on ChromeOS https://crbug.com/860312
// Also flaky on Wndows and other platforms: http://crbug.com/1070302
// Check that downloading multiple (in this case, 2) files does not result in
// corrupted files.
IN_PROC_BROWSER_TEST_F(DevToolsDownloadContentTest, DISABLED_MultiDownload) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string download1_path =
      temp_dir.GetPath().AppendASCII("download1").AsUTF8Unsafe();
  std::string download2_path =
      temp_dir.GetPath().AppendASCII("download2").AsUTF8Unsafe();

  SetupEnsureNoPendingDownloads();
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  SetDownloadBehavior("allow", download1_path);
  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download1 = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   content::SlowDownloadHttpResponse::kUnknownSizeUrl));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download1->GetState());

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  SetDownloadBehavior("allow", download2_path);
  // Start the second download and wait until it's done.
  GURL url(embedded_test_server()->GetURL("/download/download-test.lib"));
  download::DownloadItem* download2 = StartDownloadAndReturnItem(shell(), url);
  WaitForCompletion(download2);

  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(download::DownloadItem::COMPLETE, download2->GetState());

  // Allow the first request to finish.
  std::unique_ptr<DownloadTestObserver> observer2(CreateWaiter(shell(), 1));
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   content::SlowDownloadHttpResponse::kFinishSlowResponseUrl)));
  observer2->WaitForFinished();  // Wait for the third request.
  EXPECT_EQ(
      1u, observer2->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  base::FilePath file1(download1->GetTargetFilePath());
  ASSERT_EQ(file1.DirName().MaybeAsASCII(), download1_path);
  size_t file_size1 =
      content::SlowDownloadHttpResponse::kFirstResponsePartSize +
      content::SlowDownloadHttpResponse::kSecondResponsePartSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  base::FilePath file2(download2->GetTargetFilePath());
  ASSERT_EQ(file2.DirName().MaybeAsASCII(), download2_path);
  ASSERT_TRUE(base::ContentsEqual(
      file2, GetTestFilePath("download", "download-test.lib")));
}
#endif  // !defined(ANDROID)

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, UnsafeOperations) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  base::Value::Dict params;
  params.Set("url", "http://www.example.com/hello.js");
  params.Set("data", "Tm90aGluZyB0byBzZWUgaGVyZSE=");

  SendCommandSync("Page.addCompilationCache", params.Clone());
  EXPECT_TRUE(result());
  Detach();
  SetAllowUnsafeOperations(false);
  Attach();
  SendCommandSync("Page.addCompilationCache", params.Clone());
  EXPECT_THAT(
      error()->FindInt("code"),
      testing::Optional(static_cast<int>(crdtp::DispatchCode::SERVER_ERROR)));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, TracingWithPerfettoConfig) {
  base::trace_event::TraceConfig chrome_config;
  perfetto::TraceConfig perfetto_config;
  std::string perfetto_config_encoded;

  chrome_config = base::trace_event::TraceConfig();
  perfetto_config = tracing::GetDefaultPerfettoConfig(
      chrome_config,
      /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/false,
      perfetto::protos::gen::ChromeConfig::USER_INITIATED);
  perfetto_config_encoded =
      base::Base64Encode(perfetto_config.SerializeAsString());

  base::Value::Dict params;
  params.Set("perfettoConfig", perfetto_config_encoded);
  params.Set("transferMode", "ReturnAsStream");

  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  EXPECT_TRUE(SendCommandSync("Tracing.start", std::move(params)));
  EXPECT_TRUE(SendCommandSync("Tracing.end"));

  WaitForNotification("Tracing.tracingComplete", true);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, NavigateToAboutBlankLoaderId) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();

  base::Value::Dict params;
  params.Set("url", "about:blank");
  const base::Value::Dict* result =
      SendCommandSync("Page.navigate", std::move(params));
  EXPECT_THAT(result->FindString("loaderId"),
              testing::Pointee(testing::Not("")));
}

class SystemTracingDevToolsProtocolTest : public DevToolsProtocolTest {
 protected:
  const base::Value::Dict* StartSystemTrace() {
    perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
        base::trace_event::TraceConfig(),
        /*privacy_filtering_enabled=*/false,
        /*convert_to_legacy_json=*/false,
        perfetto::protos::gen::ChromeConfig::USER_INITIATED);

    std::string perfetto_config_encoded =
        base::Base64Encode(perfetto_config.SerializeAsString());

    base::Value::Dict params;
    params.Set("perfettoConfig", perfetto_config_encoded);
    params.Set("transferMode", "ReturnAsStream");
    params.Set("tracingBackend", "system");

    NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
    Attach();

    return SendCommandSync("Tracing.start", std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(SystemTracingDevToolsProtocolTest,
                       StartSystemTracingFailsWhenSystemConsumerDisabled) {
  EXPECT_FALSE(StartSystemTrace());
}

#if BUILDFLAG(IS_POSIX)
class PosixSystemTracingDevToolsProtocolTest
    : public SystemTracingDevToolsProtocolTest {
 public:
  PosixSystemTracingDevToolsProtocolTest() {
    feature_list_.InitAndEnableFeature(features::kEnablePerfettoSystemTracing);
    tracing::PerfettoTracedProcess::Get()
        ->SetAllowSystemTracingConsumerForTesting(true);
    const char* producer_sock = getenv("PERFETTO_PRODUCER_SOCK_NAME");
    saved_producer_sock_name_ = producer_sock ? producer_sock : std::string();
    const char* consumer_sock = getenv("PERFETTO_CONSUMER_SOCK_NAME");
    saved_consumer_sock_name_ = consumer_sock ? consumer_sock : std::string();
  }

  ~PosixSystemTracingDevToolsProtocolTest() override {
    if (!saved_producer_sock_name_.empty()) {
      SetProducerSockEnvName(saved_producer_sock_name_);
    } else {
      EXPECT_EQ(0, unsetenv("PERFETTO_PRODUCER_SOCK_NAME"));
    }
    if (!saved_consumer_sock_name_.empty()) {
      SetConsumerSockEnvName(saved_consumer_sock_name_);
    } else {
      EXPECT_EQ(0, unsetenv("PERFETTO_CONSUMER_SOCK_NAME"));
    }
  }

 protected:
  void SetProducerSockEnvName(const std::string& value) {
    ASSERT_EQ(0, setenv("PERFETTO_PRODUCER_SOCK_NAME", value.c_str(),
                        /*overwrite=*/true));
  }
  void SetConsumerSockEnvName(const std::string& value) {
    ASSERT_EQ(0, setenv("PERFETTO_CONSUMER_SOCK_NAME", value.c_str(),
                        /*overwrite=*/true));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::string saved_producer_sock_name_;
  std::string saved_consumer_sock_name_;
};

class InvalidSystemTracingDevToolsProtocolTest
    : public PosixSystemTracingDevToolsProtocolTest {
 public:
  void SetUp() override {
    // Use a non-existing backend.
    SetProducerSockEnvName("non_existing");
    SetConsumerSockEnvName("non_existing");

    PosixSystemTracingDevToolsProtocolTest::SetUp();
  }
};

// TODO(https://crbug.com/328350104): Fails ASAN builds
#if defined(ADDRESS_SANITIZER)
#define MAYBE_StartTracingFailsWithInvalidSockets \
  DISABLED_StartTracingFailsWithInvalidSockets
#else
#define MAYBE_StartTracingFailsWithInvalidSockets \
  StartTracingFailsWithInvalidSockets
#endif
IN_PROC_BROWSER_TEST_F(InvalidSystemTracingDevToolsProtocolTest,
                       MAYBE_StartTracingFailsWithInvalidSockets) {
  EXPECT_FALSE(StartSystemTrace());
}

class FakeSystemTracingDevToolsProtocolTest
    : public PosixSystemTracingDevToolsProtocolTest {
 public:
  FakeSystemTracingDevToolsProtocolTest()
      : deferred_task_runner_(new base::DeferredSequencedTaskRunner()) {}

  void SetUp() override {
    SetupService();
    PosixSystemTracingDevToolsProtocolTest::SetUp();
  }

  void PreRunTestOnMainThread() override {
    deferred_task_runner_->StartWithTaskRunner(
        base::SequencedTaskRunner::GetCurrentDefault());

    PosixSystemTracingDevToolsProtocolTest::PreRunTestOnMainThread();
  }

 private:
  void SetupService() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    system_service_ = std::make_unique<tracing::MockSystemService>(
        temp_dir_, std::make_unique<base::tracing::PerfettoTaskRunner>(
                       deferred_task_runner_));

    SetProducerSockEnvName(system_service_->producer());
    SetConsumerSockEnvName(system_service_->consumer());
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::DeferredSequencedTaskRunner> deferred_task_runner_;
  std::unique_ptr<tracing::MockSystemService> system_service_;
};

// No system consumer support on Android to reduce Chrome binary size.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TracingWithFakeSystemBackend DISABLED_TracingWithFakeSystemBackend
#else
#define MAYBE_TracingWithFakeSystemBackend TracingWithFakeSystemBackend
#endif
IN_PROC_BROWSER_TEST_F(FakeSystemTracingDevToolsProtocolTest,
                       MAYBE_TracingWithFakeSystemBackend) {
  EXPECT_TRUE(StartSystemTrace());
  EXPECT_TRUE(SendCommandSync("Tracing.end"));
  WaitForNotification("Tracing.tracingComplete", true);
}

class FakeSystemTracingForbiddenDevToolsProtocolTest
    : public PosixSystemTracingDevToolsProtocolTest {
 public:
  void SetUp() override {
    tracing::PerfettoTracedProcess::Get()
        ->SetAllowSystemTracingConsumerForTesting(false);
    PosixSystemTracingDevToolsProtocolTest::SetUp();
  }
};

// No system consumer support on Android to reduce Chrome binary size.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SystemConsumerForbidden DISABLED_SystemConsumerForbidden
#else
#define MAYBE_SystemConsumerForbidden SystemConsumerForbidden
#endif
IN_PROC_BROWSER_TEST_F(FakeSystemTracingForbiddenDevToolsProtocolTest,
                       MAYBE_SystemConsumerForbidden) {
  EXPECT_FALSE(StartSystemTrace());
}
#endif  // BUILDFLAG(IS_POSIX)

class NetworkResponseProtocolTest : public DevToolsProtocolTest {
 protected:
  base::Value::Dict FetchAndWaitForResponse(const GURL& url) {
    WebContents* web_contents = shell()->web_contents();
    std::string script = JsReplace("fetch($1).then(r => r.status)", url.spec());
    EvalJsResult status = EvalJs(web_contents, script);
    CHECK_EQ(200, status);

    // Look for the requestId.
    auto matches_url = [](const GURL& url, const base::Value::Dict& params) {
      const std::string* got_url = params.FindStringByDottedPath("request.url");
      return got_url && *got_url == url.spec();
    };
    base::Value::Dict request = WaitForMatchingNotification(
        "Network.requestWillBeSent", base::BindRepeating(matches_url, url));
    const std::string* request_id = request.FindString("requestId");
    CHECK(request_id) << "Could not find request ID";

    // Look for the response.
    auto matches_id = [](const std::string& request_id,
                         const base::Value::Dict& params) {
      const std::string* id = params.FindString("requestId");
      return id && *id == request_id;
    };
    return WaitForMatchingNotification(
        "Network.responseReceived",
        base::BindRepeating(matches_id, *request_id));
  }
};

// Test that the SecurityDetails field of the resource response matches the
// server.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetails) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  server_config.cipher_suite_for_testing = 0xc02f;
  server_config.curves_for_testing = {NID_X25519};
  server_config.signature_algorithm_for_testing = SSL_SIGN_RSA_PSS_RSAE_SHA384;
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(server.Start());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
                                             server.GetURL("/title1.html"), 1);

  Attach();
  SendCommandAsync("Network.enable");

  base::Value::Dict response =
      FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* protocol =
      response.FindStringByDottedPath("response.securityDetails.protocol");
  ASSERT_TRUE(protocol);
  EXPECT_EQ("TLS 1.2", *protocol);

  const std::string* key_exchange =
      response.FindStringByDottedPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("ECDHE_RSA", *key_exchange);

  const std::string* cipher =
      response.FindStringByDottedPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  EXPECT_EQ("AES_128_GCM", *cipher);

  // AEAD ciphers should not report a MAC.
  EXPECT_FALSE(response.FindStringByDottedPath("response.securityDetails.mac"));

  const std::string* group = response.FindStringByDottedPath(
      "response.securityDetails.keyExchangeGroup");
  ASSERT_TRUE(group);
  EXPECT_EQ("X25519", *group);

  std::optional<int> sigalg = response.FindIntByDottedPath(
      "response.securityDetails.serverSignatureAlgorithm");
  EXPECT_EQ(SSL_SIGN_RSA_PSS_RSAE_SHA384, sigalg);

  std::optional<bool> ech = response.FindBoolByDottedPath(
      "response.securityDetails.encryptedClientHello");
  EXPECT_EQ(false, ech);

  const std::string* subject =
      response.FindStringByDottedPath("response.securityDetails.subjectName");
  ASSERT_TRUE(subject);
  EXPECT_EQ(server.GetCertificate()->subject().common_name, *subject);

  const std::string* issuer =
      response.FindStringByDottedPath("response.securityDetails.issuer");
  ASSERT_TRUE(issuer);
  EXPECT_EQ(server.GetCertificate()->issuer().common_name, *issuer);

  // The default certificate has a single SAN, 127.0.0.1.
  const base::Value* sans =
      response.FindByDottedPath("response.securityDetails.sanList");
  ASSERT_TRUE(sans);
  ASSERT_EQ(1u, sans->GetList().size());
  EXPECT_EQ(base::Value("127.0.0.1"), sans->GetList()[0]);

  std::optional<double> valid_from =
      response.FindDoubleByDottedPath("response.securityDetails.validFrom");
  EXPECT_EQ(server.GetCertificate()->valid_start().InSecondsFSinceUnixEpoch(),
            valid_from);

  std::optional<double> valid_to =
      response.FindDoubleByDottedPath("response.securityDetails.validTo");
  EXPECT_EQ(server.GetCertificate()->valid_expiry().InSecondsFSinceUnixEpoch(),
            valid_to);
}

// Test SecurityDetails, but with a TLS 1.3 cipher suite, which should not
// report a key exchange component.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetailsTLS13) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_3;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_3;
  server_config.curves_for_testing = {NID_X25519};
  server_config.signature_algorithm_for_testing = SSL_SIGN_RSA_PSS_RSAE_SHA384;
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(server.Start());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
                                             server.GetURL("/title1.html"), 1);

  Attach();
  SendCommandAsync("Network.enable");

  base::Value::Dict response =
      FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* protocol =
      response.FindStringByDottedPath("response.securityDetails.protocol");
  ASSERT_TRUE(protocol);
  EXPECT_EQ("TLS 1.3", *protocol);

  const std::string* key_exchange =
      response.FindStringByDottedPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("", *key_exchange);

  const std::string* cipher =
      response.FindStringByDottedPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  // Depending on whether the host machine has AES hardware, the server may
  // pick AES-GCM or ChaCha20-Poly1305.
  EXPECT_TRUE(*cipher == "AES_128_GCM" || *cipher == "CHACHA20_POLY1305");

  // AEAD ciphers should not report a MAC.
  EXPECT_FALSE(response.FindStringByDottedPath("response.securityDetails.mac"));

  const std::string* group = response.FindStringByDottedPath(
      "response.securityDetails.keyExchangeGroup");
  ASSERT_TRUE(group);
  EXPECT_EQ("X25519", *group);

  std::optional<int> sigalg = response.FindIntByDottedPath(
      "response.securityDetails.serverSignatureAlgorithm");
  EXPECT_EQ(SSL_SIGN_RSA_PSS_RSAE_SHA384, sigalg);

  std::optional<bool> ech = response.FindBoolByDottedPath(
      "response.securityDetails.encryptedClientHello");
  EXPECT_EQ(false, ech);
}

// Test SecurityDetails, but with a legacy cipher suite, which should report a
// separate MAC component and no group.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest,
                       SecurityDetailsLegacyCipher) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  // TLS_RSA_WITH_AES_128_CBC_SHA
  server_config.cipher_suite_for_testing = 0x002f;
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(server.Start());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
                                             server.GetURL("/title1.html"), 1);

  Attach();
  SendCommandAsync("Network.enable");

  base::Value::Dict response =
      FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* key_exchange =
      response.FindStringByDottedPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("RSA", *key_exchange);

  const std::string* cipher =
      response.FindStringByDottedPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  EXPECT_EQ("AES_128_CBC", *cipher);

  const std::string* mac =
      response.FindStringByDottedPath("response.securityDetails.mac");
  ASSERT_TRUE(mac);
  EXPECT_EQ("HMAC-SHA1", *mac);

  // RSA ciphers should not report a MAC.
  EXPECT_FALSE(response.FindStringByDottedPath(
      "response.securityDetails.keyExchangeGroup"));
}

// Test that complex certificate SAN lists are reported in SecurityDetails.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetailsSAN) {
  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.dns_names = {"a.example", "b.example", "*.c.example"};
  cert_config.ip_addresses = {net::IPAddress::IPv4Localhost(),
                              net::IPAddress::IPv6Localhost(),
                              net::IPAddress(1, 2, 3, 4)};
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(cert_config);
  server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(server.Start());

  NavigateToURLBlockUntilNavigationsComplete(shell(),
                                             server.GetURL("/title1.html"), 1);

  Attach();
  SendCommandAsync("Network.enable");

  base::Value::Dict response =
      FetchAndWaitForResponse(server.GetURL("/empty.html"));
  const base::Value* sans =
      response.FindByDottedPath("response.securityDetails.sanList");
  ASSERT_TRUE(sans);
  ASSERT_EQ(6u, sans->GetList().size());
  EXPECT_EQ(base::Value("a.example"), sans->GetList()[0]);
  EXPECT_EQ(base::Value("b.example"), sans->GetList()[1]);
  EXPECT_EQ(base::Value("*.c.example"), sans->GetList()[2]);
  EXPECT_EQ(base::Value("127.0.0.1"), sans->GetList()[3]);
  EXPECT_EQ(base::Value("::1"), sans->GetList()[4]);
  EXPECT_EQ(base::Value("1.2.3.4"), sans->GetList()[5]);
}

class NetworkResponseProtocolECHTest : public NetworkResponseProtocolTest {
 public:
  // a.test is covered by `CERT_TEST_NAMES`.
  static constexpr char kHostname[] = "a.test";
  static constexpr char kPublicName[] = "public-name.test";
  static constexpr char kDohServerHostname[] = "doh.test";

  NetworkResponseProtocolECHTest()
      : ech_server_{net::EmbeddedTestServer::TYPE_HTTPS} {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{net::features::kUseDnsHttpsSvcb,
          {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"}}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    // Configure `ech_server_` to use ECH.
    net::SSLServerConfig server_config;
    std::vector<uint8_t> ech_config_list;
    server_config.ech_keys = net::MakeTestEchKeys(
        kPublicName, /*max_name_len=*/64, &ech_config_list);
    ASSERT_TRUE(server_config.ech_keys);
    ech_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ech_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                             server_config);

    ASSERT_TRUE(ech_server_.Start());

    // Start a DoH server, which ensures we use a resolver with HTTPS RR
    // support. Configure it to serve records for `ech_server_`.
    doh_server_.SetHostname(kDohServerHostname);
    url::SchemeHostPort ech_host(GetURL("/"));
    doh_server_.AddAddressRecord(ech_host.host(),
                                 net::IPAddress::IPv4Localhost());
    doh_server_.AddRecord(net::BuildTestHttpsServiceRecord(
        net::dns_util::GetNameForHttpsQuery(ech_host),
        /*priority=*/1, /*service_name=*/ech_host.host(),
        {net::BuildTestHttpsServiceEchConfigParam(ech_config_list)}));
    ASSERT_TRUE(doh_server_.Start());

    // Add a single bootstrapping rule so we can resolve the DoH server.
    host_resolver()->AddRule(kDohServerHostname, "127.0.0.1");

    // Configure the network service to use the test DoH server.
    std::optional<net::DnsOverHttpsConfig> doh_config =
        net::DnsOverHttpsConfig::FromString(doh_server_.GetTemplate());
    ASSERT_TRUE(doh_config.has_value());
    SetTestDohConfig(net::SecureDnsMode::kSecure,
                     std::move(doh_config.value()));
    SetReplaceSystemDnsConfig();
  }

  GURL GetURL(std::string_view path) {
    return ech_server_.GetURL(kHostname, path);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::TestDohServer doh_server_;
  net::EmbeddedTestServer ech_server_;
};

// Test SecurityDetails reports when Encrypted ClientHello was negotiated.
// Flaky: https://crbug.com/1521189
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolECHTest,
                       DISABLED_SecurityDetailsECH) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GetURL("/title1.html"),
                                             1);

  Attach();
  SendCommandAsync("Network.enable");

  base::Value::Dict response = FetchAndWaitForResponse(GetURL("/empty.html"));
  std::optional<bool> ech = response.FindBoolByDottedPath(
      "response.securityDetails.encryptedClientHello");
  EXPECT_EQ(true, ech);
}

IN_PROC_BROWSER_TEST_F(
    PrerenderDevToolsProtocolTest,
    PrerenderStatusUpdatedReportsFailureWithDisallowedMojoInterface) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");

  // Navigate to an initial page.
  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Make a prerendered page.
  FrameTreeNodeId host_id = AddPrerender(kPrerenderingUrl);
  auto* prerender_render_frame_host = GetPrerenderedMainFrameHost(host_id);
  Attach();
  SendCommandSync("Preload.enable");

  // Executing `navigator.getGamepads()` to start binding the GamepadMonitor
  // interface, and this is expected to cause prerender cancellation because
  // the API is disallowed.
  ExecuteScriptAsyncWithoutUserGesture(prerender_render_frame_host,
                                       "navigator.getGamepads()");

  base::Value::Dict result;
  while (true) {
    result = WaitForNotification("Preload.prerenderStatusUpdated", true);
    if (*result.FindString("status") == "Failure") {
      break;
    }
  }

  EXPECT_THAT(*result.FindString("disallowedMojoInterface"),
              Eq("device.mojom.GamepadMonitor"));
}

IN_PROC_BROWSER_TEST_F(
    PrerenderDevToolsProtocolTest,
    PrerenderStatusUpdatedReportsFailureWithPrerenderMismatchedHeaders) {
  const std::string user_agent_override = "foo";
  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to an initial page.
  const GURL initial_url = GetUrl("/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), initial_url));

  // Enable user agent override for future navigations.
  UserAgentInjector injector(shell()->web_contents(), user_agent_override);

  const GURL prerendering_url = GetUrl("/empty.html?prerender");

  // Start prerendering.
  const FrameTreeNodeId host_id = AddPrerender(prerendering_url);

  Attach();
  SendCommandSync("Preload.enable");

  RenderFrameHostImpl* prerender_rfh =
      static_cast<RenderFrameHostImpl*>(GetPrerenderedMainFrameHost(host_id));
  EXPECT_EQ(user_agent_override, EvalJs(prerender_rfh, "navigator.userAgent"));

  // Stop overriding user agent from now on.
  injector.set_is_overriding_user_agent(false);

  // Activate the prerendered page.
  test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  NavigatePrimaryPage(prerendering_url);
  host_observer.WaitForDestroyed();

  base::Value::Dict result;
  while (true) {
    result = WaitForNotification("Preload.prerenderStatusUpdated", true);
    if (*result.FindString("status") == "Failure") {
      break;
    }
  }
  EXPECT_TRUE(result.Find("mismatchedHeaders"));
}

IN_PROC_BROWSER_TEST_F(PrerenderDevToolsProtocolTest,
                       RenderFrameDevToolsAgentHostCacheEvictionCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kInitialUrl = GetUrl("/empty.html");
  const GURL kPrerenderingUrl = GetUrl("/empty.html?prerender");
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());

  ASSERT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Attaching a session via a "tab" target is required to opt-in into
  // FTN swapping mode during prerender activation.
  AttachToTabTarget(web_contents_impl);
  base::Value::Dict command_params;
  command_params.Set("autoAttach", true);
  command_params.Set("waitForDebuggerOnStart", false);
  command_params.Set("flatten", true);
  SendCommandSync("Target.setAutoAttach", std::move(command_params));

  // Stash current RFDTAH for WebContents that is about to be retained
  // by BFCache after prerender navigation and flushed later.
  auto old_host = DevToolsAgentHost::GetOrCreateFor(web_contents_impl);
  RenderFrameDeletedObserver delete_observer(
      web_contents_impl->GetPrimaryMainFrame());

  // Activating a prerender should cause FTN swapping on the RFH and put
  // the old one into the BFCache with frame_tree_node_ == nullptr.
  AddPrerender(kPrerenderingUrl);
  NavigatePrimaryPage(kPrerenderingUrl);

  EXPECT_FALSE(delete_observer.deleted());
  web_contents_impl->GetController().GetBackForwardCache().Flush();
  delete_observer.WaitUntilDeleted();

  // Assure methods on disconnected host are safe to call.
  EXPECT_THAT(old_host->GetTitle(), testing::Eq(""));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, ResponseAfterReload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");

  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);

  Attach();

  SendCommandSync("Fetch.enable");
  SendCommandAsync("Page.reload");

  base::Value::Dict command_params;
  command_params.Set("discover", true);
  SendCommandSync("Target.setDiscoverTargets", std::move(command_params));

  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    shell()->LoadURL(GURL(blink::kChromeUICrashURL));
    WaitForNotification("Target.targetCrashed", true);
  }

  SetProtocolCommandId(42);
  SendCommandAsync("Network.enable");

  SetProtocolCommandId(42);
  SendCommandSync("Page.reload");

  SendCommandAsync("Fetch.disable");
  SendCommandSync("Network.enable");
}

class SharedStorageDevToolsProtocolTest : public DevToolsProtocolTest {
 public:
  SharedStorageDevToolsProtocolTest() {
    feature_list_
        .InitWithFeaturesAndParameters(/*enabled_features=*/
                                       {{blink::features::kSharedStorageAPI,
                                         {{"SharedStorageBitBudget",
                                           base::NumberToString(
                                               kBudgetAllowed)}}},
                                        {features::
                                             kPrivacySandboxAdsAPIsOverride,
                                         {}}},
                                       /*disabled_features=*/{});
  }

  void MakeBudgetWithdrawal(const GURL& url, double bits) {
    auto* manager = shell()
                        ->web_contents()
                        ->GetBrowserContext()
                        ->GetDefaultStoragePartition()
                        ->GetSharedStorageManager();
    ASSERT_TRUE(manager);
    base::test::TestFuture<storage::SharedStorageManager::OperationResult>
        future;
    manager->MakeBudgetWithdrawal(net::SchemefulSite(url), bits,
                                  future.GetCallback());
    EXPECT_EQ(storage::SharedStorageManager::OperationResult::kSuccess,
              future.Get());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedStorageDevToolsProtocolTest,
                       ResetSharedStorageBudget) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  Attach();

  base::Value::Dict command_params;
  command_params.Set("enable", true);
  SendCommandSync("Storage.setSharedStorageTracking",
                  std::move(command_params));
  ASSERT_FALSE(error());

  // Set an entry in order to initialize shared storage database for
  // `origin_str`.
  command_params = base::Value::Dict();
  std::string origin_str = url.GetWithEmptyPath().spec();
  command_params.Set("ownerOrigin", origin_str);
  command_params.Set("key", "key1");
  command_params.Set("value", "value1");
  SendCommandSync("Storage.setSharedStorageEntry", std::move(command_params));
  ASSERT_FALSE(error());

  // "remainingBudget" should currently be at its max, `kBudgetAllowed`.
  command_params = base::Value::Dict();
  command_params.Set("ownerOrigin", origin_str);
  SendCommandSync("Storage.getSharedStorageMetadata",
                  std::move(command_params));
  ASSERT_TRUE(result());
  EXPECT_THAT(result()->FindDoubleByDottedPath("metadata.remainingBudget"),
              testing::Optional(kBudgetAllowed));

  // Make some withdrawals.
  MakeBudgetWithdrawal(url, 1.0);
  MakeBudgetWithdrawal(url, 2.5);

  // "remainingBudget" should have decreased the appropriate amount.
  command_params = base::Value::Dict();
  command_params.Set("ownerOrigin", origin_str);
  SendCommandSync("Storage.getSharedStorageMetadata",
                  std::move(command_params));
  ASSERT_TRUE(result());
  EXPECT_THAT(result()->FindDoubleByDottedPath("metadata.remainingBudget"),
              testing::Optional(kBudgetAllowed - 1.0 - 2.5));

  // Reset the budget.
  command_params = base::Value::Dict();
  command_params.Set("ownerOrigin", origin_str);
  SendCommandSync("Storage.resetSharedStorageBudget",
                  std::move(command_params));
  ASSERT_FALSE(error());

  // "remainingBudget" should be back at its max, `kBudgetAllowed`.
  command_params = base::Value::Dict();
  command_params.Set("ownerOrigin", origin_str);
  SendCommandSync("Storage.getSharedStorageMetadata",
                  std::move(command_params));
  ASSERT_TRUE(result());
  EXPECT_THAT(result()->FindDoubleByDottedPath("metadata.remainingBudget"),
              testing::Optional(kBudgetAllowed));
}

}  // namespace content
