// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_web_contents.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/test/pixel_test_utils.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "headless/test/headless_devtooled_browsertest.h"
#include "headless/test/test_network_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Not;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {

class HeadlessWebContentsTest : public HeadlessBrowserTest {};

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, Navigation) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, WindowOpen) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/window_open.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());

  HeadlessWebContentsImpl* child = nullptr;
  HeadlessWebContentsImpl* parent = nullptr;
  for (HeadlessWebContents* c : browser_context->GetAllWebContents()) {
    HeadlessWebContentsImpl* impl = HeadlessWebContentsImpl::From(c);
    if (impl->window_id() == 1)
      parent = impl;
    else if (impl->window_id() == 2)
      child = impl;
  }

  EXPECT_NE(nullptr, parent);
  EXPECT_NE(nullptr, child);
  EXPECT_NE(parent, child);

  // Mac doesn't have WindowTreeHosts.
  if (parent && child && parent->window_tree_host())
    EXPECT_NE(parent->window_tree_host(), child->window_tree_host());

  gfx::Rect expected_bounds(0, 0, 200, 100);
#if !BUILDFLAG(IS_MAC)
  EXPECT_EQ(expected_bounds, child->web_contents()->GetViewBounds());
  EXPECT_EQ(expected_bounds, child->web_contents()->GetContainerBounds());
#else   // !BUILDFLAG(IS_MAC)
  // Mac does not support GetViewBounds() and view positions are random.
  EXPECT_EQ(expected_bounds.size(),
            child->web_contents()->GetContainerBounds().size());
#endif  // !BUILDFLAG(IS_MAC)
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest,
                       FocusOfHeadlessWebContents_IsIndependent) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  WaitForLoadAndGainFocus(web_contents);

  EXPECT_THAT(EvaluateScript(web_contents, "document.hasFocus()"),
              DictHasValue("result.result.value", true));

  HeadlessWebContents* web_contents2 =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  WaitForLoadAndGainFocus(web_contents2);

  // Focus of different WebContents is independent.
  EXPECT_THAT(EvaluateScript(web_contents, "document.hasFocus()"),
              DictHasValue("result.result.value", true));
  EXPECT_THAT(EvaluateScript(web_contents2, "document.hasFocus()"),
              DictHasValue("result.result.value", true));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, HandleSSLError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server.Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(https_server.GetURL("/hello.html"))
          .Build();

  EXPECT_FALSE(WaitForLoad(web_contents));
}

namespace {
bool DecodePNG(const std::string& png_data, SkBitmap* bitmap) {
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(png_data.data()), png_data.size(),
      bitmap);
}
}  // namespace

// Parameter specifies whether --disable-gpu should be used.
class HeadlessWebContentsScreenshotTest
    : public HeadlessDevTooledBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    EnablePixelOutput();
    if (GetParam()) {
      UseSoftwareCompositing();
      SetUpWithoutGPU();
    } else {
      HeadlessDevTooledBrowserTest::SetUp();
    }
  }

  void RunDevTooledTest() override {
    devtools_client_.SendCommand(
        "Runtime.evaluate",
        Param("expression", "document.body.style.background = '#0000ff'"),
        base::BindOnce(&HeadlessWebContentsScreenshotTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(base::Value::Dict) {
    devtools_client_.SendCommand(
        "Page.captureScreenshot",
        base::BindOnce(&HeadlessWebContentsScreenshotTest::OnScreenshotCaptured,
                       base::Unretained(this)));
  }

  void OnScreenshotCaptured(base::Value::Dict result) {
    std::string png_data_base64 = DictString(result, "result.data");
    ASSERT_FALSE(png_data_base64.empty());

    std::string png_data;
    ASSERT_TRUE(base::Base64Decode(png_data_base64, &png_data));
    EXPECT_GT(png_data.size(), 0U);

    SkBitmap result_bitmap;
    EXPECT_TRUE(DecodePNG(png_data, &result_bitmap));

    EXPECT_EQ(800, result_bitmap.width());
    EXPECT_EQ(600, result_bitmap.height());
    SkColor actual_color = result_bitmap.getColor(400, 300);
    SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
    EXPECT_EQ(expected_color, actual_color);

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessWebContentsScreenshotTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_SUITE_P(HeadlessWebContentsScreenshotTests,
                         HeadlessWebContentsScreenshotTest,
                         ::testing::Bool());

// Regression test for crbug.com/832138.
class HeadlessWebContentsScreenshotWindowPositionTest
    : public HeadlessWebContentsScreenshotTest {
 public:
  void RunDevTooledTest() override {
    base::Value::Dict params;
    params.Set("windowId",
               HeadlessWebContentsImpl::From(web_contents_)->window_id());
    params.SetByDottedPath("bounds.left", 600);
    params.SetByDottedPath("bounds.top", 100);
    params.SetByDottedPath("bounds.width", 800);
    params.SetByDottedPath("bounds.height", 600);

    browser_devtools_client_.SendCommand(
        "Browser.setWindowBounds", std::move(params),
        base::BindOnce(
            &HeadlessWebContentsScreenshotWindowPositionTest::OnWindowBoundsSet,
            base::Unretained(this)));
  }

  void OnWindowBoundsSet(base::Value::Dict result) {
    EXPECT_NE(result.FindDict("result"), nullptr);
    HeadlessWebContentsScreenshotTest::RunDevTooledTest();
  }
};

HEADLESS_DEVTOOLED_TEST_P(HeadlessWebContentsScreenshotWindowPositionTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessWebContentsScreenshotWindowPositionTest,
                         ::testing::Bool());

// Regression test for https://crbug.com/733569.
class HeadlessWebContentsRequestStorageQuotaTest
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Runtime.consoleAPICalled",
        base::BindRepeating(
            &HeadlessWebContentsRequestStorageQuotaTest::OnConsoleAPICalled,
            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Runtime.enable");

    // Should not crash and call console.log() if quota request succeeds.
    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()
                         ->GetURL("/request_storage_quota.html")
                         .spec()));
  }

  void OnConsoleAPICalled(const base::Value::Dict& params) {
    const base::Value::List* args = params.FindListByDottedPath("params.args");
    ASSERT_NE(args, nullptr);
    ASSERT_GT(args->size(), 0ul);

    const base::Value* value = args->front().GetDict().Find("value");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->GetString(), "success");

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessWebContentsRequestStorageQuotaTest);

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, BrowserTabChangeContent) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string script = "window.location = '" +
                       embedded_test_server()->GetURL("/hello.html").spec() +
                       "';";
  EXPECT_THAT(EvaluateScript(web_contents, script),
              Not(DictHasKey("exceptionDetails")));

  // This will time out if the previous script did not work.
  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessWebContentsTest, BrowserOpenInTab) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/link.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  EXPECT_EQ(1u, browser_context->GetAllWebContents().size());
  // Simulates a middle-button click on a link to ensure that the
  // link is opened in a new tab by the browser and not by the renderer.
  std::string script =
      "var event = new MouseEvent('click', {'button': 1});"
      "document.getElementsByTagName('a')[0].dispatchEvent(event);";
  EXPECT_THAT(EvaluateScript(web_contents, script),
              Not(DictHasKey("exceptionDetails")));
  // Check that we have a new tab.
  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());
}

// BeginFrameControl is not supported on MacOS.
#if !BUILDFLAG(IS_MAC)

// TODO(kvitekp): Check to see if this could be trimmed down by using
// Pre/PostRunAsynchronousTest().
class HeadlessWebContentsBeginFrameControlTest : public HeadlessBrowserTest {
 public:
  HeadlessWebContentsBeginFrameControlTest() {}

  void SetUp() override {
    EnablePixelOutput();
    HeadlessBrowserTest::SetUp();
  }

 protected:
  virtual std::string GetTestHtmlFile() = 0;
  virtual void StartFrames() {}
  virtual void OnFrameFinished(base::Value::Dict result) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    // See bit.ly/headless-rendering for why we use these flags.
    command_line->AppendSwitch(::switches::kRunAllCompositorStagesBeforeDraw);
    command_line->AppendSwitch(::switches::kDisableNewContentRenderingTimeout);
    command_line->AppendSwitch(cc::switches::kDisableCheckerImaging);
    command_line->AppendSwitch(cc::switches::kDisableThreadedAnimation);
  }

  void RunTest() {
    browser()->SetDefaultBrowserContext(
        browser()->CreateBrowserContextBuilder().Build());
    SimpleDevToolsProtocolClient browser_devtools_client;
    browser_devtools_client.AttachToBrowser();

    EXPECT_TRUE(embedded_test_server()->Start());

    base::Value::Dict params;
    params.Set("url", "about:blank");
    params.Set("width", 200);
    params.Set("height", 200);
    params.Set("enableBeginFrameControl", true);
    browser_devtools_client.SendCommand(
        "Target.createTarget", std::move(params),
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::OnTargetCreated,
            base::Unretained(this)));

    RunAsynchronousTest();

    browser_devtools_client.DetachClient();
  }

  void OnTargetCreated(base::Value::Dict result) {
    const std::string targetId = DictString(result, "result.targetId");
    ASSERT_FALSE(targetId.empty());

    web_contents_ = HeadlessWebContentsImpl::From(
        browser()->GetWebContentsForDevToolsAgentHostId(targetId));

    devtools_client_.AttachToWebContents(web_contents_->web_contents());
    devtools_client_.AddEventHandler("Page.loadEventFired",
                                     on_load_event_fired_handler_);

    devtools_client_.SendCommand(
        "Page.enable",
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::OnPageDomainEnabled,
            base::Unretained(this)));
  }

  void OnPageDomainEnabled(base::Value::Dict) {
    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL(GetTestHtmlFile()).spec()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    TRACE_EVENT0("headless",
                 "HeadlessWebContentsBeginFrameControlTest::OnLoadEventFired");

    devtools_client_.SendCommand("Page.disable");
    devtools_client_.RemoveEventHandler("Page.loadEventFired",
                                        on_load_event_fired_handler_);

    StartFrames();
  }

  void BeginFrame(bool screenshot) {
    num_begin_frames_++;

    base::Value::Dict params;
    if (screenshot)
      params.Set("screenshot", base::Value::Dict());

    devtools_client_.SendCommand(
        "HeadlessExperimental.beginFrame", std::move(params),
        base::BindOnce(&HeadlessWebContentsBeginFrameControlTest::FrameFinished,
                       base::Unretained(this)));
  }

  void FrameFinished(base::Value::Dict result) {
    TRACE_EVENT2(
        "headless", "HeadlessWebContentsBeginFrameControlTest::FrameFinished",
        "has_damage", DictBool(result, "result.hasDamage"),
        "has_screenshot_data", DictString(result, "result.screenshotData"));

    OnFrameFinished(std::move(result));
  }

  void PostFinishAsynchronousTest() {
    browser()->BrowserMainThread()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::FinishAsynchronousTest,
            base::Unretained(this)));
  }

  raw_ptr<HeadlessWebContentsImpl, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;  // Not owned.

  int num_begin_frames_ = 0;

  SimpleDevToolsProtocolClient devtools_client_;

  SimpleDevToolsProtocolClient::EventCallback on_load_event_fired_handler_ =
      base::BindRepeating(
          &HeadlessWebContentsBeginFrameControlTest::OnLoadEventFired,
          base::Unretained(this));
};

class HeadlessWebContentsBeginFrameControlBasicTest
    : public HeadlessWebContentsBeginFrameControlTest {
 public:
  HeadlessWebContentsBeginFrameControlBasicTest() = default;

 protected:
  std::string GetTestHtmlFile() override {
    // Blue background.
    return "/blue_page.html";
  }

  void StartFrames() override { BeginFrame(true); }

  void OnFrameFinished(base::Value::Dict result) override {
    if (num_begin_frames_ == 1) {
      // First BeginFrame should have caused damage and have a screenshot.
      EXPECT_TRUE(DictBool(result, "result.hasDamage"));

      std::string png_data_base64 = DictString(result, "result.screenshotData");
      ASSERT_FALSE(png_data_base64.empty());

      std::string png_data;
      ASSERT_TRUE(base::Base64Decode(png_data_base64, &png_data));
      EXPECT_GT(png_data.size(), 0U);

      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(png_data, &result_bitmap));
      EXPECT_EQ(200, result_bitmap.width());
      EXPECT_EQ(200, result_bitmap.height());
      SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
      SkColor actual_color = result_bitmap.getColor(100, 100);
      EXPECT_EQ(expected_color, actual_color);
    } else {
      DCHECK_EQ(2, num_begin_frames_);
      // Can't guarantee that the second BeginFrame didn't have damage, but it
      // should not have a screenshot.
      EXPECT_FALSE(result.FindStringByDottedPath("result.screenshotData"));
    }

    if (num_begin_frames_ < 2) {
      // Don't capture a screenshot in the second BeginFrame.
      BeginFrame(false);
    } else {
      // Post completion to avoid deleting the WebContents on the same callstack
      // as frame finished callback.
      PostFinishAsynchronousTest();
    }
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessWebContentsBeginFrameControlBasicTest);

class HeadlessWebContentsBeginFrameControlViewportTest
    : public HeadlessWebContentsBeginFrameControlTest {
 public:
  HeadlessWebContentsBeginFrameControlViewportTest() = default;

 protected:
  std::string GetTestHtmlFile() override {
    // Draws a 100x100px blue box at 200x200px.
    return "/blue_box.html";
  }

  void StartFrames() override {
    // Send a first BeginFrame to initialize the surface.
    BeginFrame(false);
  }

  void SetUpViewport() {
    base::Value::Dict params;
    params.Set("width", 0);
    params.Set("height", 0);
    params.Set("deviceScaleFactor", 0);
    params.Set("mobile", false);
    params.SetByDottedPath("viewport.x", 200);
    params.SetByDottedPath("viewport.y", 200);
    params.SetByDottedPath("viewport.width", 100);
    params.SetByDottedPath("viewport.height", 100);
    params.SetByDottedPath("viewport.scale", 3);

    devtools_client_.SendCommand(
        "Emulation.setDeviceMetricsOverride", std::move(params),
        base::BindOnce(&HeadlessWebContentsBeginFrameControlViewportTest::
                           OnSetDeviceMetricsOverrideDone,
                       base::Unretained(this)));
  }

  void OnSetDeviceMetricsOverrideDone(base::Value::Dict result) {
    EXPECT_THAT(result, DictHasKey("result"));
    // Take a screenshot in the second BeginFrame.
    BeginFrame(true);
  }

  void OnFrameFinished(base::Value::Dict result) override {
    if (num_begin_frames_ == 1) {
      SetUpViewport();
      return;
    }

    DCHECK_EQ(2, num_begin_frames_);
    // Second BeginFrame should have a screenshot of the configured viewport and
    // of the correct size.
    EXPECT_TRUE(*result.FindBoolByDottedPath("result.hasDamage"));

    std::string png_data_base64 = DictString(result, "result.screenshotData");
    ASSERT_FALSE(png_data_base64.empty());

    std::string png_data;
    ASSERT_TRUE(base::Base64Decode(png_data_base64, &png_data));
    ASSERT_GT(png_data.size(), 0ul);

    SkBitmap result_bitmap;
    EXPECT_TRUE(DecodePNG(png_data, &result_bitmap));

    // Expect a 300x300 bitmap that is all blue.
    SkBitmap expected_bitmap;
    SkImageInfo info;
    expected_bitmap.allocPixels(
        SkImageInfo::MakeN32(300, 300, kOpaque_SkAlphaType), /*rowBytes=*/0);
    expected_bitmap.eraseColor(SkColorSetRGB(0x00, 0x00, 0xff));

    EXPECT_TRUE(cc::MatchesBitmap(result_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));

    // Post completion to avoid deleting the WebContents on the same callstack
    // as frame finished callback.
    PostFinishAsynchronousTest();
  }
};

// TODO(crbug.com/40274291): Turning this off since it's flaking regularly.
DISABLED_HEADLESS_DEVTOOLED_TEST_F(
    HeadlessWebContentsBeginFrameControlViewportTest);

#endif  // !BUILDFLAG(IS_MAC)

class CookiesEnabled : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(&CookiesEnabled::OnLoadEventFired,
                            base::Unretained(this)));
    devtools_client_.SendCommand("Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/cookie.html").spec()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    devtools_client_.SendCommand(
        "Runtime.evaluate", Param("expression", "window.test_result"),
        base::BindOnce(&CookiesEnabled::OnEvaluateResult,
                       base::Unretained(this)));
  }

  void OnEvaluateResult(base::Value::Dict result) {
    EXPECT_EQ(DictString(result, "result.result.value"), "0");

    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(CookiesEnabled);

namespace {
const char* kPageWhichOpensAWindow = R"(
<html>
<body>
<script>
const win = window.open('/page2.html');
if (!win)
  console.error('ready');
win.addEventListener('load', () => console.log('ready'));
</script>
</body>
</html>
)";

const char* kPage2 = R"(
<html>
<body>
Page 2.
</body>
</html>
)";
}  // namespace

class WebContentsOpenTest : public HeadlessDevTooledBrowserTest {
 public:
  void PreRunAsynchronousTest() override {
    interceptor_ = std::make_unique<TestNetworkInterceptor>();
  }

  void PostRunAsynchronousTest() override { interceptor_.reset(); }

  void RunDevTooledTest() override {
    DCHECK(interceptor_);

    interceptor_->InsertResponse("http://foo.com/index.html",
                                 {kPageWhichOpensAWindow, "text/html"});
    interceptor_->InsertResponse("http://foo.com/page2.html",
                                 {kPage2, "text/html"});

    devtools_client_.AddEventHandler(
        "Runtime.consoleAPICalled",
        base::BindRepeating(&WebContentsOpenTest::OnConsoleAPICalled,
                            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Runtime.enable");

    devtools_client_.SendCommand("Page.navigate",
                                 Param("url", "http://foo.com/index.html"));
  }

  virtual void OnConsoleAPICalled(const base::Value::Dict& params) {}

 protected:
  std::unique_ptr<TestNetworkInterceptor> interceptor_;
};

class DontBlockWebContentsOpenTest : public WebContentsOpenTest {
 public:
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetBlockNewWebContents(false);
  }

  void OnConsoleAPICalled(const base::Value::Dict& params) override {
    EXPECT_THAT(
        interceptor_->urls_requested(),
        ElementsAre("http://foo.com/index.html", "http://foo.com/page2.html"));
    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(DontBlockWebContentsOpenTest);

class BlockWebContentsOpenTest : public WebContentsOpenTest {
 public:
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetBlockNewWebContents(true);
  }

  void OnConsoleAPICalled(const base::Value::Dict& params) override {
    EXPECT_THAT(interceptor_->urls_requested(),
                ElementsAre("http://foo.com/index.html"));
    FinishAsynchronousTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(BlockWebContentsOpenTest);

// Regression test for crbug.com/1385982.
class BlockDevToolsEmbedding : public HeadlessDevTooledBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessDevTooledBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort,
                                    base::NumberToString(port_));
  }

  void RunDevTooledTest() override {
    std::stringstream url;
    url << "data:text/html,<iframe src='http://localhost:" << port_
        << "/json/version'></iframe>";

    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(&BlockDevToolsEmbedding::OnLoadEventFired,
                            base::Unretained(this)));
    devtools_client_.SendCommand("Page.enable");
    devtools_client_.SendCommand("Page.navigate", Param("url", url.str()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    devtools_client_.SendCommand(
        "Page.getFrameTree",
        base::BindOnce(&BlockDevToolsEmbedding::OnFrameTreeResult,
                       base::Unretained(this)));
  }

  void OnFrameTreeResult(base::Value::Dict result) {
    // Make sure the iframe did not load successfully.
    auto& child_frames =
        *result.FindListByDottedPath("result.frameTree.childFrames");
    EXPECT_EQ(DictString(child_frames[0].GetDict(), "frame.url"),
              "chrome-error://chromewebdata/");
    FinishAsynchronousTest();
  }

 private:
  int port_ = 10000 + (rand() % 60000);
};

HEADLESS_DEVTOOLED_TEST_F(BlockDevToolsEmbedding);

}  // namespace headless
