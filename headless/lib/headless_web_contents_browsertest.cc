// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/browser.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/headless_experimental.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/security.h"
#include "headless/public/devtools/domains/target.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "base/strings/string_number_conversions.h"
#include "pdf/pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#endif

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Not;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

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
#if !defined(OS_MACOSX)
  EXPECT_EQ(expected_bounds, child->web_contents()->GetViewBounds());
  EXPECT_EQ(expected_bounds, child->web_contents()->GetContainerBounds());
#else   // !defined(OS_MACOSX)
  // Mac does not support GetViewBounds() and view positions are random.
  EXPECT_EQ(expected_bounds.size(),
            child->web_contents()->GetContainerBounds().size());
#endif  // !defined(OS_MACOSX)
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

  std::unique_ptr<runtime::EvaluateResult> has_focus =
      EvaluateScript(web_contents, "document.hasFocus()");
  EXPECT_TRUE(has_focus->GetResult()->GetValue()->GetBool());

  HeadlessWebContents* web_contents2 =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  WaitForLoadAndGainFocus(web_contents2);

  // Focus of different WebContents is independent.
  has_focus = EvaluateScript(web_contents, "document.hasFocus()");
  EXPECT_TRUE(has_focus->GetResult()->GetValue()->GetBool());

  has_focus = EvaluateScript(web_contents2, "document.hasFocus()");
  EXPECT_TRUE(has_focus->GetResult()->GetValue()->GetBool());
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
bool DecodePNG(std::string base64_data, SkBitmap* bitmap) {
  std::string png_data;
  if (!base::Base64Decode(base64_data, &png_data))
    return false;
  return gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(png_data.data()), png_data.size(),
      bitmap);
}
}  // namespace

// Parameter specifies whether --disable-gpu should be used.
class HeadlessWebContentsScreenshotTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    EnablePixelOutput();
    if (GetParam()) {
      UseSoftwareCompositing();
      SetUpWithoutGPU();
    } else {
      HeadlessAsyncDevTooledBrowserTest::SetUp();
    }
  }

  void RunDevTooledTest() override {
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression("document.body.style.background = '#0000ff'")
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::BindOnce(&HeadlessWebContentsScreenshotTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->CaptureScreenshot(
        page::CaptureScreenshotParams::Builder().Build(),
        base::BindOnce(&HeadlessWebContentsScreenshotTest::OnScreenshotCaptured,
                       base::Unretained(this)));
  }

  void OnScreenshotCaptured(
      std::unique_ptr<page::CaptureScreenshotResult> result) {
    std::string base64 = result->GetData();
    EXPECT_GT(base64.length(), 0U);
    SkBitmap result_bitmap;
    EXPECT_TRUE(DecodePNG(base64, &result_bitmap));

    EXPECT_EQ(800, result_bitmap.width());
    EXPECT_EQ(600, result_bitmap.height());
    SkColor actual_color = result_bitmap.getColor(400, 300);
    SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
    EXPECT_EQ(expected_color, actual_color);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_P(HeadlessWebContentsScreenshotTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(HeadlessWebContentsScreenshotTests,
                        HeadlessWebContentsScreenshotTest,
                        ::testing::Bool());

// Regression test for crbug.com/832138.
class HeadlessWebContentsScreenshotWindowPositionTest
    : public HeadlessWebContentsScreenshotTest {
 public:
  void RunDevTooledTest() override {
    browser_devtools_client_->GetBrowser()->GetExperimental()->SetWindowBounds(
        browser::SetWindowBoundsParams::Builder()
            .SetWindowId(
                HeadlessWebContentsImpl::From(web_contents_)->window_id())
            .SetBounds(browser::Bounds::Builder()
                           .SetLeft(600)
                           .SetTop(100)
                           .SetWidth(800)
                           .SetHeight(600)
                           .Build())
            .Build(),
        base::BindOnce(
            &HeadlessWebContentsScreenshotWindowPositionTest::OnWindowBoundsSet,
            base::Unretained(this)));
  }

  void OnWindowBoundsSet(
      std::unique_ptr<browser::SetWindowBoundsResult> result) {
    EXPECT_TRUE(result);
    HeadlessWebContentsScreenshotTest::RunDevTooledTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_P(
    HeadlessWebContentsScreenshotWindowPositionTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(HeadlessWebContentsScreenshotWindowPositionTests,
                        HeadlessWebContentsScreenshotWindowPositionTest,
                        ::testing::Bool());

#if BUILDFLAG(ENABLE_PRINTING)
class HeadlessWebContentsPDFTest : public HeadlessAsyncDevTooledBrowserTest {
 public:
  const double kPaperWidth = 10;
  const double kPaperHeight = 15;
  const double kDocHeight = 50;
  // Number of color channels in a BGRA bitmap.
  const int kColorChannels = 4;
  const int kDpi = 300;

  void RunDevTooledTest() override {
    std::string height_expression = "document.body.style.height = '" +
                                    base::NumberToString(kDocHeight) + "in'";
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder()
            .SetExpression("document.body.style.background = '#123456';" +
                           height_expression)
            .Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::BindOnce(&HeadlessWebContentsPDFTest::OnPageSetupCompleted,
                       base::Unretained(this)));
  }

  void OnPageSetupCompleted(std::unique_ptr<runtime::EvaluateResult> result) {
    devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
        page::PrintToPDFParams::Builder()
            .SetPrintBackground(true)
            .SetPaperHeight(kPaperHeight)
            .SetPaperWidth(kPaperWidth)
            .SetMarginTop(0)
            .SetMarginBottom(0)
            .SetMarginLeft(0)
            .SetMarginRight(0)
            .Build(),
        base::BindOnce(&HeadlessWebContentsPDFTest::OnPDFCreated,
                       base::Unretained(this)));
  }

  void OnPDFCreated(std::unique_ptr<page::PrintToPDFResult> result) {
    std::string base64 = result->GetData();
    EXPECT_GT(base64.length(), 0U);
    std::string pdf_data;
    EXPECT_TRUE(base::Base64Decode(base64, &pdf_data));

    auto pdf_span = base::as_bytes(base::make_span(pdf_data));
    int num_pages;
    EXPECT_TRUE(chrome_pdf::GetPDFDocInfo(pdf_span, &num_pages, nullptr));
    EXPECT_EQ(std::ceil(kDocHeight / kPaperHeight), num_pages);

    for (int i = 0; i < num_pages; i++) {
      double width_in_points;
      double height_in_points;
      EXPECT_TRUE(chrome_pdf::GetPDFPageSizeByIndex(
          pdf_span, i, &width_in_points, &height_in_points));
      EXPECT_EQ(static_cast<int>(width_in_points),
                static_cast<int>(kPaperWidth * printing::kPointsPerInch));
      EXPECT_EQ(static_cast<int>(height_in_points),
                static_cast<int>(kPaperHeight * printing::kPointsPerInch));

      gfx::Rect rect(kPaperWidth * kDpi, kPaperHeight * kDpi);
      printing::PdfRenderSettings settings(
          rect, gfx::Point(0, 0), gfx::Size(kDpi, kDpi), /*autorotate=*/true,
          /*use_color=*/true, printing::PdfRenderSettings::Mode::NORMAL);
      std::vector<uint8_t> page_bitmap_data(kColorChannels *
                                            settings.area.size().GetArea());
      EXPECT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
          pdf_span, i, page_bitmap_data.data(), settings.area.size().width(),
          settings.area.size().height(), settings.dpi.width(),
          settings.dpi.height(), settings.autorotate, settings.use_color));
      EXPECT_EQ(0x56, page_bitmap_data[0]);  // B
      EXPECT_EQ(0x34, page_bitmap_data[1]);  // G
      EXPECT_EQ(0x12, page_bitmap_data[2]);  // R
    }
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsPDFTest);
#endif

class HeadlessWebContentsSecurityTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public security::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    devtools_client_->GetSecurity()->GetExperimental()->AddObserver(this);
    devtools_client_->GetSecurity()->GetExperimental()->Enable(
        security::EnableParams::Builder().Build());
  }

  void OnSecurityStateChanged(
      const security::SecurityStateChangedParams& params) override {
    EXPECT_EQ(security::SecurityState::NEUTRAL, params.GetSecurityState());

    devtools_client_->GetSecurity()->GetExperimental()->Disable(
        security::DisableParams::Builder().Build());
    devtools_client_->GetSecurity()->GetExperimental()->RemoveObserver(this);
    FinishAsynchronousTest();
  }
};

// Regression test for https://crbug.com/733569.
class HeadlessWebContentsRequestStorageQuotaTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public runtime::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable(run_loop.QuitClosure());
    run_loop.Run();

    // Should not crash and call console.log() if quota request succeeds.
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/request_storage_quota.html").spec());
  }

  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsRequestStorageQuotaTest);

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
  EXPECT_FALSE(EvaluateScript(web_contents, script)->HasExceptionDetails());

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
  EXPECT_FALSE(EvaluateScript(web_contents, script)->HasExceptionDetails());

  // Check that we have a new tab.
  EXPECT_EQ(2u, browser_context->GetAllWebContents().size());
}

// BeginFrameControl is not supported on MacOS.
#if !defined(OS_MACOSX)

class HeadlessWebContentsBeginFrameControlTest
    : public HeadlessBrowserTest,
      public headless_experimental::ExperimentalObserver,
      public page::Observer {
 public:
  HeadlessWebContentsBeginFrameControlTest() {}

  void SetUp() override {
    EnablePixelOutput();
    HeadlessBrowserTest::SetUp();
  }

 protected:
  virtual std::string GetTestHtmlFile() = 0;
  virtual void OnNeedsBeginFrame() {}
  virtual void OnFrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {}

  void RunTest() {
    browser_devtools_client_ = HeadlessDevToolsClient::Create();
    devtools_client_ = HeadlessDevToolsClient::Create();
    browser_context_ = browser()->CreateBrowserContextBuilder().Build();
    browser()->SetDefaultBrowserContext(browser_context_);
    browser()->GetDevToolsTarget()->AttachClient(
        browser_devtools_client_.get());

    EXPECT_TRUE(embedded_test_server()->Start());

    browser_devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl("about://blank")
            .SetWidth(200)
            .SetHeight(200)
            .SetEnableBeginFrameControl(true)
            .Build(),
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::OnCreateTargetResult,
            base::Unretained(this)));

    RunAsynchronousTest();

    browser()->GetDevToolsTarget()->DetachClient(
        browser_devtools_client_.get());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    // See bit.ly/headless-rendering for why we use these flags.
    command_line->AppendSwitch(switches::kRunAllCompositorStagesBeforeDraw);
    command_line->AppendSwitch(switches::kDisableNewContentRenderingTimeout);
    command_line->AppendSwitch(cc::switches::kDisableCheckerImaging);
    command_line->AppendSwitch(cc::switches::kDisableThreadedAnimation);
    command_line->AppendSwitch(switches::kDisableThreadedScrolling);

    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableSurfaceSynchronization);
  }

  void OnCreateTargetResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    web_contents_ = HeadlessWebContentsImpl::From(
        browser()->GetWebContentsForDevToolsAgentHostId(result->GetTargetId()));

    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetHeadlessExperimental()->GetExperimental()->AddObserver(
        this);
    devtools_client_->GetHeadlessExperimental()->GetExperimental()->Enable(
        headless_experimental::EnableParams::Builder().Build());

    devtools_client_->GetPage()->GetExperimental()->StopLoading(
        page::StopLoadingParams::Builder().Build(),
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::LoadingStopped,
            base::Unretained(this)));
  }

  void LoadingStopped(std::unique_ptr<page::StopLoadingResult>) {
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable(base::BindOnce(
        &HeadlessWebContentsBeginFrameControlTest::PageDomainEnabled,
        base::Unretained(this)));
  }

  void PageDomainEnabled() {
    devtools_client_->GetPage()->Navigate(
        page::NavigateParams::Builder()
            .SetUrl(embedded_test_server()->GetURL(GetTestHtmlFile()).spec())
            .Build());
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    TRACE_EVENT0("headless",
                 "HeadlessWebContentsBeginFrameControlTest::OnLoadEventFired");
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->RemoveObserver(this);
    page_ready_ = true;
    if (needs_begin_frames_) {
      DCHECK(!frame_in_flight_);
      OnNeedsBeginFrame();
    }
  }

  // headless_experimental::ExperimentalObserver implementation:
  void OnNeedsBeginFramesChanged(
      const headless_experimental::NeedsBeginFramesChangedParams& params)
      override {
    TRACE_EVENT1(
        "headless",
        "HeadlessWebContentsBeginFrameControlTest::OnNeedsBeginFramesChanged",
        "needs_begin_frames", params.GetNeedsBeginFrames());
    needs_begin_frames_ = params.GetNeedsBeginFrames();
    // With full-pipeline mode and surface sync, the needs_begin_frame signal
    // should become and then always stay true.
    EXPECT_TRUE(needs_begin_frames_);
    EXPECT_FALSE(frame_in_flight_);
    if (page_ready_)
      OnNeedsBeginFrame();
  }

  void BeginFrame(bool screenshot) {
    // With full-pipeline mode and surface sync, the needs_begin_frame signal
    // should always be true.
    EXPECT_TRUE(needs_begin_frames_);

    frame_in_flight_ = true;
    num_begin_frames_++;

    auto builder = headless_experimental::BeginFrameParams::Builder();
    if (screenshot) {
      builder.SetScreenshot(
          headless_experimental::ScreenshotParams::Builder().Build());
    }

    devtools_client_->GetHeadlessExperimental()->GetExperimental()->BeginFrame(
        builder.Build(),
        base::BindOnce(&HeadlessWebContentsBeginFrameControlTest::FrameFinished,
                       base::Unretained(this)));
  }

  void FrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {
    TRACE_EVENT2("headless",
                 "HeadlessWebContentsBeginFrameControlTest::FrameFinished",
                 "has_damage", result->GetHasDamage(), "has_screenshot_data",
                 result->HasScreenshotData());

    // Post OnFrameFinished call so that any pending OnNeedsBeginFramesChanged
    // call will be executed first.
    browser()->BrowserMainThread()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::NotifyOnFrameFinished,
            base::Unretained(this), std::move(result)));
  }

  void NotifyOnFrameFinished(
      std::unique_ptr<headless_experimental::BeginFrameResult> result) {
    frame_in_flight_ = false;
    OnFrameFinished(std::move(result));
  }

  void PostFinishAsynchronousTest() {
    devtools_client_->GetHeadlessExperimental()
        ->GetExperimental()
        ->RemoveObserver(this);

    browser()->BrowserMainThread()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HeadlessWebContentsBeginFrameControlTest::FinishAsynchronousTest,
            base::Unretained(this)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  HeadlessBrowserContext* browser_context_ = nullptr;  // Not owned.
  HeadlessWebContentsImpl* web_contents_ = nullptr;    // Not owned.

  bool page_ready_ = false;
  bool needs_begin_frames_ = false;
  bool frame_in_flight_ = false;
  int num_begin_frames_ = 0;
  std::unique_ptr<HeadlessDevToolsClient> browser_devtools_client_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
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

  void OnNeedsBeginFrame() override {
    BeginFrame(true);
  }

  void OnFrameFinished(std::unique_ptr<headless_experimental::BeginFrameResult>
                           result) override {
    if (num_begin_frames_ == 1) {
      // First BeginFrame should have caused damage and have a screenshot.
      EXPECT_TRUE(result->GetHasDamage());
      ASSERT_TRUE(result->HasScreenshotData());
      std::string base64 = result->GetScreenshotData();
      EXPECT_LT(0u, base64.length());
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(base64, &result_bitmap));
      EXPECT_EQ(200, result_bitmap.width());
      EXPECT_EQ(200, result_bitmap.height());
      SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
      SkColor actual_color = result_bitmap.getColor(100, 100);
      EXPECT_EQ(expected_color, actual_color);
    } else {
      DCHECK_EQ(2, num_begin_frames_);
      // Can't guarantee that the second BeginFrame didn't have damage, but it
      // should not have a screenshot.
      EXPECT_FALSE(result->HasScreenshotData());
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

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessWebContentsBeginFrameControlBasicTest);

class HeadlessWebContentsBeginFrameControlViewportTest
    : public HeadlessWebContentsBeginFrameControlTest {
 public:
  HeadlessWebContentsBeginFrameControlViewportTest() = default;

 protected:
  std::string GetTestHtmlFile() override {
    // Draws a 100x100px blue box at 200x200px.
    return "/blue_box.html";
  }

  void OnNeedsBeginFrame() override {
    // Send a first BeginFrame to initialize the surface.
    BeginFrame(false);
  }

  void SetUpViewport() {
    devtools_client_->GetEmulation()
        ->GetExperimental()
        ->SetDeviceMetricsOverride(
            emulation::SetDeviceMetricsOverrideParams::Builder()
                .SetWidth(0)
                .SetHeight(0)
                .SetDeviceScaleFactor(0)
                .SetMobile(false)
                .SetViewport(page::Viewport::Builder()
                                 .SetX(200)
                                 .SetY(200)
                                 .SetWidth(100)
                                 .SetHeight(100)
                                 .SetScale(3)
                                 .Build())
                .Build(),
            base::BindOnce(&HeadlessWebContentsBeginFrameControlViewportTest::
                               SetDeviceMetricsOverrideDone,
                           base::Unretained(this)));
  }

  void SetDeviceMetricsOverrideDone(
      std::unique_ptr<emulation::SetDeviceMetricsOverrideResult> result) {
    EXPECT_TRUE(result);
    // Take a screenshot in the second BeginFrame.
    BeginFrame(true);
  }

  void OnFrameFinished(std::unique_ptr<headless_experimental::BeginFrameResult>
                           result) override {
    if (num_begin_frames_ == 1) {
      SetUpViewport();
      return;
    }

    DCHECK_EQ(2, num_begin_frames_);
    // Second BeginFrame should have a screenshot of the configured viewport and
    // of the correct size.
    EXPECT_TRUE(result->GetHasDamage());
    EXPECT_TRUE(result->HasScreenshotData());
    if (result->HasScreenshotData()) {
      std::string base64 = result->GetScreenshotData();
      EXPECT_LT(0u, base64.length());
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(base64, &result_bitmap));

      EXPECT_EQ(300, result_bitmap.width());
      EXPECT_EQ(300, result_bitmap.height());
      SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);

      SkColor actual_color = result_bitmap.getColor(100, 100);
      EXPECT_EQ(expected_color, actual_color);
      actual_color = result_bitmap.getColor(0, 0);
      EXPECT_EQ(expected_color, actual_color);
      actual_color = result_bitmap.getColor(0, 299);
      EXPECT_EQ(expected_color, actual_color);
      actual_color = result_bitmap.getColor(299, 0);
      EXPECT_EQ(expected_color, actual_color);
      actual_color = result_bitmap.getColor(299, 299);
      EXPECT_EQ(expected_color, actual_color);
    }

    // Post completion to avoid deleting the WebContents on the same callstack
    // as frame finished callback.
    PostFinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(
    HeadlessWebContentsBeginFrameControlViewportTest);

#endif  // !defined(OS_MACOSX)

class CookiesEnabled : public HeadlessAsyncDevTooledBrowserTest,
                       page::Observer {
 public:
  void RunDevTooledTest() override {
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();

    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/cookie.html").spec());
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetRuntime()->Evaluate(
        "window.test_result",
        base::BindOnce(&CookiesEnabled::OnResult, base::Unretained(this)));
  }

  void OnResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string value;
    EXPECT_TRUE(result->GetResult()->HasValue());
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&value));
    EXPECT_EQ("0", value);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(CookiesEnabled);

namespace {
const char* kPageWhichOpensAWindow = R"(
<html>
<body>
<script>
window.open('/page2.html');
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

class WebContentsOpenTest : public page::Observer,
                            public HeadlessAsyncDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    devtools_client_->GetPage()->AddObserver(this);
    interceptor_->InsertResponse("http://foo.com/index.html",
                                 {kPageWhichOpensAWindow, "text/html"});
    interceptor_->InsertResponse("http://foo.com/page2.html",
                                 {kPage2, "text/html"});

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    run_loop.Run();

    devtools_client_->GetPage()->Navigate("http://foo.com/index.html");
  }
};

class DontBlockWebContentsOpenTest : public WebContentsOpenTest {
 public:
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetBlockNewWebContents(false);
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    EXPECT_THAT(
        interceptor_->urls_requested(),
        ElementsAre("http://foo.com/index.html", "http://foo.com/page2.html"));
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DontBlockWebContentsOpenTest);

class BlockWebContentsOpenTest : public WebContentsOpenTest {
 public:
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetBlockNewWebContents(true);
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    EXPECT_THAT(interceptor_->urls_requested(),
                ElementsAre("http://foo.com/index.html"));
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(BlockWebContentsOpenTest);

}  // namespace headless
