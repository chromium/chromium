// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_main_frame_observer.h"

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"  // nogncheck
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"  // nogncheck
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace content {

namespace {
using testing::Eq;
using testing::Optional;

class FakeJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  explicit FakeJsErrorReportProcessor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  // After calling this, expect all SendErrorReport's to use this
  // BrowserContext.
  void SetExpectedBrowserContext(BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       BrowserContext* browser_context) override {
    CHECK_EQ(browser_context, browser_context_);
    last_error_report_ = std::move(error_report);
    ++error_report_count_;
    task_runner_->PostTask(FROM_HERE, std::move(completion_callback));
  }

  const JavaScriptErrorReport& last_error_report() const {
    return last_error_report_;
  }

  int error_report_count() const { return error_report_count_; }

  // Make public for testing.
  using JsErrorReportProcessor::SetDefault;

 protected:
  ~FakeJsErrorReportProcessor() override = default;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  JavaScriptErrorReport last_error_report_;
  int error_report_count_ = 0;
  raw_ptr<BrowserContext> browser_context_ = nullptr;
};

class MockWebUIController : public WebUIController {
 public:
  explicit MockWebUIController(WebUI* web_ui) : WebUIController(web_ui) {}

  bool IsJavascriptErrorReportingEnabled() override {
    return enable_javascript_error_reporting_;
  }
  void enable_javascript_error_reporting(
      bool enable_javascript_error_reporting) {
    enable_javascript_error_reporting_ = enable_javascript_error_reporting;
  }

 private:
  bool enable_javascript_error_reporting_ = true;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(MockWebUIController)

}  // namespace

class WebUIMainFrameObserverTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    site_instance_ = SiteInstance::Create(browser_context());
    SetContents(TestWebContents::Create(browser_context(), site_instance_));
    // Since we just created the web_contents() pointer with
    // TestWebContents::Create, the static_casts are safe.
    web_ui_ = std::make_unique<WebUIImpl>(web_contents());
    web_ui_->SetRenderFrameHost(
        static_cast<TestWebContents*>(web_contents())->GetPrimaryMainFrame());
    web_ui_->SetController(
        std::make_unique<MockWebUIController>(web_ui_.get()));
    process()->Init();

    previous_processor_ = JsErrorReportProcessor::Get();
    processor_ = base::MakeRefCounted<FakeJsErrorReportProcessor>(
        task_environment()->GetMainThreadTaskRunner());
    FakeJsErrorReportProcessor::SetDefault(processor_);
    processor_->SetExpectedBrowserContext(browser_context());
  }

  void TearDown() override {
    FakeJsErrorReportProcessor::SetDefault(previous_processor_);

    // We have to destroy the site_instance_ before
    // RenderViewHostTestHarness::TearDown() destroys the
    // BrowserTaskEnvironment. We've gotten a lot of things that depend directly
    // or indirectly on BrowserTaskEnvironment, so just destroy everything we
    // can.
    previous_processor_.reset();
    processor_.reset();
    web_ui_.reset();
    site_instance_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  // Simulate navigating to the WebUI page. Basically so that
  // WebUIMainFrameObserver::ReadyToCommitNavigation() gets called, since that
  // initializes the error handling.
  void NavigateToPage() {
    NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                      GURL(kPageURL8));
  }

  // Calls the observer's OnDidAddMessageToConsole with the given arguments.
  // This is just here so that we don't need to FRIEND_TEST_ALL_PREFIXES for
  // each and every test.
  void CallOnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& stack_trace) {
    web_ui_->GetWebUIMainFrameObserverForTest()->OnDidAddMessageToConsole(
        source_frame, log_level, message, line_no, source_id, stack_trace);
  }

 protected:
  scoped_refptr<SiteInstance> site_instance_;
  std::unique_ptr<WebUIImpl> web_ui_;
  scoped_refptr<FakeJsErrorReportProcessor> processor_;
  scoped_refptr<JsErrorReportProcessor> previous_processor_;

  static constexpr char kMessage8[] = "An Error Is Me";
  static constexpr char16_t kMessage16[] = u"An Error Is Me";
  static constexpr char kSourceURL8[] = "chrome://here.is.error/bad.js";
  static constexpr char16_t kSourceURL16[] = u"chrome://here.is.error/bad.js";
  static constexpr char kPageURL8[] = "chrome://here.is.error/index.html";
  static constexpr char kStackTrace8[] =
      "at badFunction (chrome://page/my.js:20:30)\n"
      "at poorCaller (chrome://page/my.js:50:10)\n";
  static constexpr char16_t kStackTrace16[] =
      u"at badFunction (chrome://page/my.js:20:30)\n"
      u"at poorCaller (chrome://page/my.js:50:10)\n";
};

constexpr char WebUIMainFrameObserverTest::kMessage8[];
constexpr char16_t WebUIMainFrameObserverTest::kMessage16[];
constexpr char WebUIMainFrameObserverTest::kSourceURL8[];
constexpr char16_t WebUIMainFrameObserverTest::kSourceURL16[];
constexpr char WebUIMainFrameObserverTest::kPageURL8[];
constexpr char WebUIMainFrameObserverTest::kStackTrace8[];
constexpr char16_t WebUIMainFrameObserverTest::kStackTrace16[];

TEST_F(WebUIMainFrameObserverTest, ErrorReported) {
  NavigateToPage();
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 1);
  EXPECT_EQ(processor_->last_error_report().message, kMessage8);
  EXPECT_EQ(processor_->last_error_report().url, kSourceURL8);
  EXPECT_EQ(processor_->last_error_report().page_url, kPageURL8);
  EXPECT_EQ(processor_->last_error_report().source_system,
            JavaScriptErrorReport::SourceSystem::kWebUIObserver);
  EXPECT_THAT(processor_->last_error_report().stack_trace,
              Optional(Eq(kStackTrace8)));
  // WebUI should use default product & version.
  EXPECT_EQ(processor_->last_error_report().product, "");
  EXPECT_EQ(processor_->last_error_report().version, "");
  EXPECT_EQ(*processor_->last_error_report().line_number, 5);
  EXPECT_FALSE(processor_->last_error_report().column_number);
  EXPECT_FALSE(processor_->last_error_report().app_locale);
  EXPECT_TRUE(processor_->last_error_report().send_to_production_servers);
}

TEST_F(WebUIMainFrameObserverTest, NoStackTrace) {
  NavigateToPage();
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, std::nullopt);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 1);
  EXPECT_EQ(processor_->last_error_report().stack_trace, std::nullopt);
}

TEST_F(WebUIMainFrameObserverTest, NonErrorsIgnored) {
  NavigateToPage();
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kWarning,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kInfo,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kVerbose,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, NoProcessorDoesntCrash) {
  NavigateToPage();
  FakeJsErrorReportProcessor::SetDefault(nullptr);
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
}

TEST_F(WebUIMainFrameObserverTest, NotSentIfInvalidURL) {
  NavigateToPage();
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, u"invalid URL", kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, NotSentIfDisabledForPage) {
  static_cast<MockWebUIController*>(web_ui_->GetController())
      ->enable_javascript_error_reporting(false);
  NavigateToPage();
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, URLPathIsPreservedOtherPartsRemoved) {
  NavigateToPage();
  struct URLTest {
    const char16_t* const input;
    const char* const expected;
  };
  const URLTest kTests[] = {
      // No path still has no path.
      {u"chrome://version", "chrome://version/"},
      {u"chrome://version/", "chrome://version/"},
      // Path is kept.
      {u"chrome://discards/graph", "chrome://discards/graph"},
      {u"chrome://discards/graph/", "chrome://discards/graph/"},
      // Longer paths are kept.
      {u"chrome://discards/graph/a/b/c/d", "chrome://discards/graph/a/b/c/d"},
      // Queries are removed, with or without a path.
      {u"chrome://bookmarks/?q=chromium", "chrome://bookmarks/"},
      {u"chrome://bookmarks/add?q=chromium", "chrome://bookmarks/add"},
      {u"chrome://bookmarks/add/?q=chromium", "chrome://bookmarks/add/"},
      // Fragments are removed, with or without a path.
      {u"chrome://flags/#tab-groups", "chrome://flags/"},
      {u"chrome://flags/available/#tab-groups", "chrome://flags/available/"},
      // Queries & fragments are removed.
      {u"chrome://bookmarks/add?q=chromium#code", "chrome://bookmarks/add"},
      // User name and password are removed. (It's weird to have a user name or
      // password on a chrome URL, but otherwise we get blocked by the
      // no-non-chrome-URLs check)
      {u"chrome://chronos:test0000@version/Home", "chrome://version/Home"},
  };

  for (const URLTest& test : kTests) {
    int previous_count = processor_->error_report_count();
    CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                                 blink::mojom::ConsoleMessageLevel::kError,
                                 kMessage16, 5, test.input, kStackTrace16);
    task_environment()->RunUntilIdle();
    EXPECT_EQ(processor_->error_report_count(), previous_count + 1)
        << "for " << test.input;
    EXPECT_EQ(processor_->last_error_report().url, test.expected)
        << "for " << test.input;
  }
}

TEST_F(WebUIMainFrameObserverTest, PageURLAlsoRedacted) {
  constexpr char kPageWithQueryAndFragment[] =
      "chrome://bookmarks/add?q=chromium#code";
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kPageWithQueryAndFragment));
  CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 1);
  EXPECT_EQ(processor_->last_error_report().page_url, "chrome://bookmarks/add");
}

TEST_F(WebUIMainFrameObserverTest, ErrorsNotReportedInOtherFrames) {
  NavigateToPage();
  auto another_contents =
      TestWebContents::Create(browser_context(), site_instance_);
  CHECK(another_contents->GetPrimaryMainFrame());
  CallOnDidAddMessageToConsole(another_contents->GetPrimaryMainFrame(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, ErrorsNotReportedForNonChromeURLs) {
  NavigateToPage();
  const char16_t* const kNonChromeSourceURLs[] = {
      u"chrome-untrusted://media-app",
      u"chrome-error://chromewebdata/",
      u"chrome-extension://abc123/",
      u"about:blank",
  };

  for (const auto* url : kNonChromeSourceURLs) {
    CallOnDidAddMessageToConsole(web_ui_->GetRenderFrameHost(),
                                 blink::mojom::ConsoleMessageLevel::kError,
                                 kMessage16, 5, url, kStackTrace16);
    task_environment()->RunUntilIdle();
    EXPECT_EQ(processor_->error_report_count(), 0) << url;
  }
}

}  // namespace content

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
