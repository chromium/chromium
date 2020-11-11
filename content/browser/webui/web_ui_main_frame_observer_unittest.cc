// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_main_frame_observer.h"

#include <memory>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)

namespace content {

namespace {

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
  BrowserContext* browser_context_ = nullptr;
};
}  // namespace

class WebUIMainFrameObserverTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSendWebUIJavaScriptErrorReports,
        {{features::kSendWebUIJavaScriptErrorReportsSendToProductionVariation,
          "true"}});
    site_instance_ = SiteInstance::Create(browser_context());
    SetContents(TestWebContents::Create(browser_context(), site_instance_));
    // Since we just created the web_contents() pointer with
    // TestWebContents::Create, the static_cast is safe.
    web_ui_ = std::make_unique<WebUIImpl>(
        static_cast<TestWebContents*>(web_contents()), main_rfh());
    observer_ =
        std::make_unique<WebUIMainFrameObserver>(web_ui_.get(), web_contents());

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
    // BrowserTaskEnvironment. We gotten a lot of things that depend directly
    // or indirectly on BrowserTaskEnvironment, so just destroy everything we
    // can.
    previous_processor_.reset();
    processor_.reset();
    observer_.reset();
    web_ui_.reset();
    site_instance_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  // Calls observer_->OnDidAddMessageToConsole with the given arguments. This
  // is just here so that we don't need to FRIEND_TEST_ALL_PREFIXES for each
  // and every test.
  void CallOnDidAddMessageToConsole(RenderFrameHost* source_frame,
                                    blink::mojom::ConsoleMessageLevel log_level,
                                    const base::string16& message,
                                    int32_t line_no,
                                    const base::string16& source_id) {
    observer_->OnDidAddMessageToConsole(source_frame, log_level, message,
                                        line_no, source_id);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<SiteInstance> site_instance_;
  std::unique_ptr<WebUIImpl> web_ui_;
  std::unique_ptr<WebUIMainFrameObserver> observer_;
  scoped_refptr<FakeJsErrorReportProcessor> processor_;
  scoped_refptr<JsErrorReportProcessor> previous_processor_;

  static constexpr char kMessage8[] = "An Error Is Me";
  const base::string16 kMessage16 = base::UTF8ToUTF16(kMessage8);
  static constexpr char kSourceURL8[] = "chrome://here.is.error/";
  const base::string16 kSourceId16 = base::UTF8ToUTF16(kSourceURL8);
};

constexpr char WebUIMainFrameObserverTest::kMessage8[];
constexpr char WebUIMainFrameObserverTest::kSourceURL8[];

TEST_F(WebUIMainFrameObserverTest, ErrorReported) {
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceId16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 1);
  EXPECT_EQ(processor_->last_error_report().message, kMessage8);
  EXPECT_EQ(processor_->last_error_report().url, kSourceURL8);
  // WebUI should use default product & version.
  EXPECT_EQ(processor_->last_error_report().product, "");
  EXPECT_EQ(processor_->last_error_report().version, "");
  EXPECT_EQ(*processor_->last_error_report().line_number, 5);
  EXPECT_FALSE(processor_->last_error_report().column_number);
  EXPECT_FALSE(processor_->last_error_report().stack_trace);
  EXPECT_FALSE(processor_->last_error_report().app_locale);
  EXPECT_TRUE(processor_->last_error_report().send_to_production_servers);
}

TEST_F(WebUIMainFrameObserverTest, NonErrorsIgnored) {
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kWarning,
                               kMessage16, 5, kSourceId16);
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kInfo,
                               kMessage16, 5, kSourceId16);
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kVerbose,
                               kMessage16, 5, kSourceId16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, NoProcessorDoesntCrash) {
  FakeJsErrorReportProcessor::SetDefault(nullptr);
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceId16);
  task_environment()->RunUntilIdle();
}

TEST_F(WebUIMainFrameObserverTest, NotSentIfFlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      features::kSendWebUIJavaScriptErrorReports);
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceId16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, NotSentIfInvalidURL) {
  CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, base::UTF8ToUTF16("invalid URL"));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(WebUIMainFrameObserverTest, URLPathIsPreservedOtherPartsRemoved) {
  struct URLTest {
    const char* const input;
    const char* const expected;
  };
  const URLTest kTests[] = {
      // No path still has no path.
      {"chrome://version", "chrome://version/"},
      {"chrome://version/", "chrome://version/"},
      // Path is kept.
      {"chrome://discards/graph", "chrome://discards/graph"},
      {"chrome://discards/graph/", "chrome://discards/graph/"},
      // Longer paths are kept.
      {"chrome://discards/graph/a/b/c/d", "chrome://discards/graph/a/b/c/d"},
      // Queries are removed, with or without a path.
      {"chrome://bookmarks/?q=chromium", "chrome://bookmarks/"},
      {"chrome://bookmarks/add?q=chromium", "chrome://bookmarks/add"},
      {"chrome://bookmarks/add/?q=chromium", "chrome://bookmarks/add/"},
      // Fragments are removed, with or without a path.
      {"chrome://flags/#tab-groups", "chrome://flags/"},
      {"chrome://flags/available/#tab-groups", "chrome://flags/available/"},
      // Queries & fragments are removed.
      {"chrome://bookmarks/add?q=chromium#code", "chrome://bookmarks/add"},
      // User name and password are removed.
      {"https://chronos:test0000@www.chromium.org/Home",
       "https://www.chromium.org/Home"},
      // Port is not removed.
      {"http://127.0.0.1:8088/static/legend_help.html",
       "http://127.0.0.1:8088/static/legend_help.html"},
      {"http://127.0.0.1:8088/#active", "http://127.0.0.1:8088/"},
      // about: pages also have their queries & fragments removed. (They really
      // shouldn't have their "about:" removed either, but that's hard to
      // prevent.)
      {"about:blank/?q=foo", "blank/"},
      {"about:blank/#foo", "blank/"},
  };

  for (const URLTest& test : kTests) {
    int previous_count = processor_->error_report_count();
    CallOnDidAddMessageToConsole(web_ui_->frame_host_for_test(),
                                 blink::mojom::ConsoleMessageLevel::kError,
                                 kMessage16, 5, base::UTF8ToUTF16(test.input));
    task_environment()->RunUntilIdle();
    EXPECT_EQ(processor_->error_report_count(), previous_count + 1)
        << "for " << test.input;
    EXPECT_EQ(processor_->last_error_report().url, test.expected)
        << "for " << test.input;
  }
}

}  // namespace content

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
