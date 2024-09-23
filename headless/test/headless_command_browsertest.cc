// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/headless/command_handler/headless_command_handler.h"
#include "components/headless/command_handler/headless_command_switches.h"
#include "components/headless/test/bitmap_utils.h"
#include "components/headless/test/capture_std_stream.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <Windows.h>
#endif

#if BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(ENABLE_PDF)
#include "components/headless/test/pdf_utils.h"
#endif

namespace headless {

namespace {

bool DecodePNG(const std::string& png_data, SkBitmap* bitmap) {
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(png_data.data()), png_data.size(),
      bitmap);
}

}  // namespace

class HeadlessCommandBrowserTest : public HeadlessBrowserTest,
                                   public content::WebContentsObserver {
 public:
  HeadlessCommandBrowserTest() = default;

  void RunTest() {
    ASSERT_TRUE(embedded_test_server()->Start());

    HeadlessBrowserContext::Builder context_builder =
        browser()->CreateBrowserContextBuilder();
    HeadlessBrowserContext* browser_context = context_builder.Build();
    browser()->SetDefaultBrowserContext(browser_context);

    GURL handler_url = HeadlessCommandHandler::GetHandlerUrl();
    HeadlessWebContents::Builder builder(
        browser_context->CreateWebContentsBuilder());
    HeadlessWebContents* web_contents =
        builder.SetInitialURL(handler_url).Build();

    content::WebContents* content_web_contents =
        HeadlessWebContentsImpl::From(web_contents)->web_contents();

    content::WebContentsObserver::Observe(content_web_contents);

    HeadlessCommandHandler::ProcessCommands(
        content_web_contents, GetTargetUrl(),
        base::BindOnce(&HeadlessCommandBrowserTest::FinishTest,
                       base::Unretained(this)),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    RunAsynchronousTest();

    if (aborted_) {
      return;
    }

    content::WebContentsObserver::Observe(nullptr);

    web_contents->Close();
    browser_context->Close();
    base::RunLoop().RunUntilIdle();
  }

  void set_aborted() { aborted_ = true; }

  std::optional<HeadlessCommandHandler::Result> result() const {
    return result_;
  }

 private:
  virtual GURL GetTargetUrl() = 0;

  void FinishTest(HeadlessCommandHandler::Result result) {
    result_ = result;
    FinishAsynchronousTest();
  }

  bool aborted_ = false;
  std::optional<HeadlessCommandHandler::Result> result_;
};

class HeadlessFileCommandBrowserTest : public HeadlessCommandBrowserTest {
 public:
  HeadlessFileCommandBrowserTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::IsDirectoryEmpty(temp_dir()));

    HeadlessCommandBrowserTest::SetUp();
  }

  void TearDown() override {
    HeadlessCommandBrowserTest::TearDown();

    ASSERT_TRUE(temp_dir_.Delete());
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
};

// DumpDom command tests ----------------------------------------------

class HeadlessDumpDomCommandBrowserTest : public HeadlessCommandBrowserTest {
 public:
  HeadlessDumpDomCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDumpDom);
  }

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/hello.html");
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessDumpDomCommandBrowserTest, DumpDom) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CaptureStdOut capture_stdout;
  capture_stdout.StartCapture();
  RunTest();
  capture_stdout.StopCapture();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  std::string captured_stdout = capture_stdout.TakeCapturedData();

  static const char kDomDump[] =
      "<!DOCTYPE html>\n"
      "<html><head></head><body><h1>Hello headless world!</h1>\n"
      "</body></html>\n";
  EXPECT_THAT(captured_stdout, testing::HasSubstr(kDomDump));
}

class HeadlessDumpDomVirtualTimeBudgetCommandBrowserTest
    : public HeadlessDumpDomCommandBrowserTest {
 public:
  HeadlessDumpDomVirtualTimeBudgetCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessDumpDomCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kVirtualTimeBudget, "5500");
  }

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/stepper_page.html");
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessDumpDomVirtualTimeBudgetCommandBrowserTest,
                       DumpDomVirtualTimeBudget) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CaptureStdOut capture_stdout;
  capture_stdout.StartCapture();
  RunTest();
  capture_stdout.StopCapture();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  std::vector<std::string> captured_lines =
      base::SplitString(capture_stdout.TakeCapturedData(), "\n",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  EXPECT_THAT(captured_lines, testing::Contains(R"(<div id="box">5</div>)"));
}

class HeadlessDumpDomTimeoutCommandBrowserTestBase
    : public HeadlessDumpDomCommandBrowserTest {
 public:
  HeadlessDumpDomTimeoutCommandBrowserTestBase() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessDumpDomCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kTimeout,
                                    base::ToString(timeout().InMilliseconds()));
  }

  base::TimeDelta timeout() { return TestTimeouts::action_timeout(); }
};

class HeadlessDumpDomTimeoutCommandBrowserTest
    : public HeadlessDumpDomTimeoutCommandBrowserTestBase {
 public:
  HeadlessDumpDomTimeoutCommandBrowserTest() = default;

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/page.html");
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/page.html") {
      auto response = std::make_unique<net::test_server::DelayedHttpResponse>(
          timeout() * 2);
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(R"(<div>Hi, I'm headless!</div>)");

      return response;
    }

    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessDumpDomTimeoutCommandBrowserTest,
                       DumpDomTimeout) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &HeadlessDumpDomTimeoutCommandBrowserTest::RequestHandler,
      base::Unretained(this)));

  CaptureStdOut capture_stdout;
  capture_stdout.StartCapture();
  RunTest();
  capture_stdout.StopCapture();

  // Main page timeout should be reported.
  EXPECT_THAT(result(),
              testing::Eq(HeadlessCommandHandler::Result::kPageLoadTimeout));

  std::vector<std::string> captured_lines =
      base::SplitString(capture_stdout.TakeCapturedData(), "\n",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Expect about:blank page DOM.
  EXPECT_THAT(captured_lines,
              testing::Contains(R"(<html><head></head><body></body></html>)"));
}

class HeadlessDumpDomSubResourceTimeoutCommandBrowserTest
    : public HeadlessDumpDomTimeoutCommandBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  HeadlessDumpDomSubResourceTimeoutCommandBrowserTest() = default;

  bool delay_response() { return GetParam(); }

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/page.html");
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/page.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->set_content(R"(
        <html><body>
        <div id="thediv">INITIAL</div>
        <script src="./script.js"></script>
        </body></html>
      )");

      return response;
    }

    if (request.relative_url == "/script.js") {
      std::unique_ptr<net::test_server::BasicHttpResponse> response;
      if (delay_response()) {
        response = std::make_unique<net::test_server::DelayedHttpResponse>(
            timeout() * 2);
      } else {
        response = std::make_unique<net::test_server::BasicHttpResponse>();
      }

      response->set_code(net::HTTP_OK);
      response->set_content_type("text/javascript");
      response->set_content(R"(
        document.getElementById("thediv").innerText="REPLACED";
      )");

      return response;
    }

    return nullptr;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         HeadlessDumpDomSubResourceTimeoutCommandBrowserTest,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(HeadlessDumpDomSubResourceTimeoutCommandBrowserTest,
                       DumpDomSubResourceTimeout) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &HeadlessDumpDomSubResourceTimeoutCommandBrowserTest::RequestHandler,
      base::Unretained(this)));

  CaptureStdOut capture_stdout;
  capture_stdout.StartCapture();
  RunTest();
  capture_stdout.StopCapture();

  std::string captured_stdout_data = capture_stdout.TakeCapturedData();
  std::erase_if(captured_stdout_data, isspace);

  if (delay_response()) {
    EXPECT_THAT(result(),
                testing::Eq(HeadlessCommandHandler::Result::kPageLoadTimeout));
    EXPECT_THAT(captured_stdout_data,
                testing::HasSubstr(
                    "<html><head></head><body><divid=\"thediv\">INITIAL</"
                    "div><scriptsrc=\"./script.js\"></script></body></html>"));
  } else {
    EXPECT_THAT(result(),
                testing::Eq(HeadlessCommandHandler::Result::kSuccess));
    EXPECT_THAT(captured_stdout_data,
                testing::HasSubstr(
                    "<html><head></head><body><divid=\"thediv\">REPLACED</"
                    "div><scriptsrc=\"./script.js\"></script></body></html>"));
  }
}

// Screenshot command tests -------------------------------------------

class HeadlessScreenshotCommandBrowserTest
    : public HeadlessFileCommandBrowserTest {
 public:
  HeadlessScreenshotCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessFileCommandBrowserTest::SetUpCommandLine(command_line);

    screenshot_filename_ =
        temp_dir().Append(FILE_PATH_LITERAL("screenshot.png"));
    command_line->AppendSwitchPath(switches::kScreenshot, screenshot_filename_);

    command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "1");
  }

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/centered_blue_box.html");
  }

  base::FilePath screenshot_filename_;
};

IN_PROC_BROWSER_TEST_F(HeadlessScreenshotCommandBrowserTest, Screenshot) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  ASSERT_TRUE(base::PathExists(screenshot_filename_)) << screenshot_filename_;

  std::string png_data;
  ASSERT_TRUE(base::ReadFileToString(screenshot_filename_, &png_data))
      << screenshot_filename_;

  SkBitmap bitmap;
  ASSERT_TRUE(DecodePNG(png_data, &bitmap));

  ASSERT_EQ(800, bitmap.width());
  ASSERT_EQ(600, bitmap.height());

  // Expect a centered blue rectangle on white background.
  EXPECT_TRUE(CheckColoredRect(bitmap, SkColorSetRGB(0x00, 0x00, 0xff),
                               SkColorSetRGB(0xff, 0xff, 0xff)));
}

class HeadlessScreenshotWithBackgroundCommandBrowserTest
    : public HeadlessScreenshotCommandBrowserTest {
 public:
  HeadlessScreenshotWithBackgroundCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessScreenshotCommandBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kDefaultBackgroundColor,
                                    "ff0000");
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessScreenshotWithBackgroundCommandBrowserTest,
                       ScreenshotBackground) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  ASSERT_TRUE(base::PathExists(screenshot_filename_)) << screenshot_filename_;

  std::string png_data;
  ASSERT_TRUE(base::ReadFileToString(screenshot_filename_, &png_data))
      << screenshot_filename_;

  SkBitmap bitmap;
  ASSERT_TRUE(DecodePNG(png_data, &bitmap));

  ASSERT_EQ(800, bitmap.width());
  ASSERT_EQ(600, bitmap.height());

  // Expect a centered blue rectangle on red background.
  EXPECT_TRUE(CheckColoredRect(bitmap, SkColorSetRGB(0x00, 0x00, 0xff),
                               SkColorSetRGB(0xff, 0x00, 0x00)));
}

class HeadlessScreenshotCommandBrowserTestWithWindowSize
    : public HeadlessScreenshotCommandBrowserTest {
 public:
  static constexpr gfx::Size kWindowSize = {4096, 2160};

  HeadlessScreenshotCommandBrowserTestWithWindowSize() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessScreenshotCommandBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWindowSize,
        base::StringPrintf("%u,%u", kWindowSize.width(), kWindowSize.height()));
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessScreenshotCommandBrowserTestWithWindowSize,
                       ScreenshotWindowSize) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  ASSERT_TRUE(base::PathExists(screenshot_filename_)) << screenshot_filename_;

  std::string png_data;
  ASSERT_TRUE(base::ReadFileToString(screenshot_filename_, &png_data))
      << screenshot_filename_;

  SkBitmap bitmap;
  ASSERT_TRUE(DecodePNG(png_data, &bitmap));

  EXPECT_EQ(bitmap.width(), kWindowSize.width());
  EXPECT_EQ(bitmap.height(), kWindowSize.height());
}

// PrintToPDF command tests -------------------------------------------

#if BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(ENABLE_PDF)

class HeadlessPrintToPdfCommandBrowserTest
    : public HeadlessFileCommandBrowserTest {
 public:
  static constexpr float kPageMarginsInInches =
      0.393701;  // See Page.PrintToPDF specs.

  HeadlessPrintToPdfCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessFileCommandBrowserTest::SetUpCommandLine(command_line);
    print_to_pdf_filename_ =
        temp_dir().Append(FILE_PATH_LITERAL("print_to.pdf"));
    command_line->AppendSwitchPath(switches::kPrintToPDF,
                                   print_to_pdf_filename_);
    command_line->AppendSwitch(switches::kNoPDFHeaderFooter);
  }

  GURL GetTargetUrl() override {
    return embedded_test_server()->GetURL("/centered_blue_box.html");
  }

  base::FilePath print_to_pdf_filename_;
};

IN_PROC_BROWSER_TEST_F(HeadlessPrintToPdfCommandBrowserTest, PrintToPdf) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  ASSERT_TRUE(base::PathExists(print_to_pdf_filename_))
      << print_to_pdf_filename_;

  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(print_to_pdf_filename_);
  ASSERT_TRUE(pdf_data.has_value()) << print_to_pdf_filename_;

  PDFPageBitmap page_bitmap;
  ASSERT_TRUE(page_bitmap.Render(pdf_data.value(), 0));

  // Expect blue rectangle on white background.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(0xff0000ff, 0xffffffff));
}

class HeadlessPrintToPdfWithBackgroundCommandBrowserTest
    : public HeadlessPrintToPdfCommandBrowserTest {
 public:
  HeadlessPrintToPdfWithBackgroundCommandBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessPrintToPdfCommandBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kDefaultBackgroundColor,
                                    "ff0000");
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessPrintToPdfWithBackgroundCommandBrowserTest,
                       PrintToPdfBackground) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  ASSERT_THAT(result(), testing::Eq(HeadlessCommandHandler::Result::kSuccess));

  ASSERT_TRUE(base::PathExists(print_to_pdf_filename_))
      << print_to_pdf_filename_;

  std::optional<std::vector<uint8_t>> pdf_data =
      base::ReadFileToBytes(print_to_pdf_filename_);
  ASSERT_TRUE(pdf_data.has_value()) << print_to_pdf_filename_;

  PDFPageBitmap page_bitmap;
  ASSERT_TRUE(page_bitmap.Render(pdf_data.value(), 0));

  // Expect blue rectangle on red background sans margin.
  EXPECT_TRUE(page_bitmap.CheckColoredRect(0xff0000ff, 0xffff0000, 120));
}

#endif  // BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(ENABLE_PDF)

// Graceful signal handling is currently available only on Linux, Mac and
// Windows.
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

class HeadlessCommandSignalBrowserTest
    : public HeadlessDumpDomCommandBrowserTest {
 public:
  HeadlessCommandSignalBrowserTest() = default;

  // content::WebContentsObserver implementation:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    set_aborted();

#if BUILDFLAG(IS_POSIX)
    set_expected_exit_code(128 + SIGINT);
    raise(SIGINT);
#elif BUILDFLAG(IS_WIN)
    ::GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessCommandSignalBrowserTest, SendCtrlCSignal) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  RunTest();

  // Command execution is expected to be Ctrl+C'ed gracefully.
}

#endif  // #if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

}  // namespace headless
