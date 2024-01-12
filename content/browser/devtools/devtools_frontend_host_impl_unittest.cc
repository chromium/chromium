// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host_impl.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"  // nogncheck
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"  // nogncheck
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/navigation_simulator.h"
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
}  // namespace

class DevToolsFrontendHostImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    site_instance_ = SiteInstance::Create(browser_context());
    SetContents(TestWebContents::Create(browser_context(), site_instance_));
    // Since we just created the web_contents() pointer with
    // TestWebContents::Create, the static_casts are safe.
    devtools_frontend_host_impl_ = DevToolsFrontendHostImpl::CreateForTesting(
        static_cast<TestWebContents*>(web_contents())->GetPrimaryMainFrame(),
        base::RepeatingCallback<void(base::Value::Dict)>());

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
    devtools_frontend_host_impl_.reset();
    site_instance_.reset();

    RenderViewHostTestHarness::TearDown();
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
    devtools_frontend_host_impl_->OnDidAddMessageToConsole(
        source_frame, log_level, message, line_no, source_id, stack_trace);
  }

 protected:
  scoped_refptr<SiteInstance> site_instance_;
  std::unique_ptr<DevToolsFrontendHostImpl> devtools_frontend_host_impl_;
  scoped_refptr<FakeJsErrorReportProcessor> processor_;
  scoped_refptr<JsErrorReportProcessor> previous_processor_;
  base::test::ScopedFeatureList scoped_feature_list_;

  static constexpr char kMessage8[] = "Uncaught exception";
  static constexpr char16_t kMessage16[] = u"Uncaught exception";
  static constexpr char kSourceURL8[] = "devtools://here.is.error/bad.js";
  static constexpr char16_t kSourceURL16[] = u"devtools://here.is.error/bad.js";
  static constexpr char kStackTrace8[] =
      "at badFunction (devtools://page/my.js:20:30)\n"
      "at poorCaller (devtools://page/my.js:50:10)\n";
  static constexpr char16_t kStackTrace16[] =
      u"at badFunction (devtools://page/my.js:20:30)\n"
      u"at poorCaller (devtools://page/my.js:50:10)\n";
};

class DevToolsFrontendHostImplJsErrorReportingEnabledTest
    : public DevToolsFrontendHostImplTest {
 public:
  void SetUp() override {
    DevToolsFrontendHostImplTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableDevToolsJsErrorReporting);
  }
};

constexpr char DevToolsFrontendHostImplTest::kMessage8[];
constexpr char16_t DevToolsFrontendHostImplTest::kMessage16[];
constexpr char DevToolsFrontendHostImplTest::kSourceURL8[];
constexpr char16_t DevToolsFrontendHostImplTest::kSourceURL16[];
constexpr char DevToolsFrontendHostImplTest::kStackTrace8[];
constexpr char16_t DevToolsFrontendHostImplTest::kStackTrace16[];

TEST_F(DevToolsFrontendHostImplTest, ErrorNotReportedWhenFeatureIsNotEnabled) {
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(DevToolsFrontendHostImplJsErrorReportingEnabledTest, Sent) {
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 1);
  EXPECT_EQ(processor_->last_error_report().message, kMessage8);
  EXPECT_EQ(processor_->last_error_report().url, kSourceURL8);
  EXPECT_EQ(processor_->last_error_report().source_system,
            JavaScriptErrorReport::SourceSystem::kDevToolsObserver);
  EXPECT_THAT(processor_->last_error_report().stack_trace,
              Optional(Eq(kStackTrace8)));
  // DevTools should use default product & version.
  EXPECT_EQ(processor_->last_error_report().product, "");
  EXPECT_EQ(processor_->last_error_report().version, "");
  EXPECT_EQ(*processor_->last_error_report().line_number, 5);
  EXPECT_FALSE(processor_->last_error_report().column_number);
  EXPECT_FALSE(processor_->last_error_report().app_locale);
  EXPECT_TRUE(processor_->last_error_report().send_to_production_servers);
}

TEST_F(DevToolsFrontendHostImplJsErrorReportingEnabledTest,
       NotSentForNonErrors) {
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kWarning,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kInfo,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kVerbose,
                               kMessage16, 5, kSourceURL16, kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(DevToolsFrontendHostImplJsErrorReportingEnabledTest,
       NotSentIfInvalidURL) {
  CallOnDidAddMessageToConsole(main_rfh(),
                               blink::mojom::ConsoleMessageLevel::kError,
                               kMessage16, 5, u"invalid URL", kStackTrace16);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(processor_->error_report_count(), 0);
}

TEST_F(DevToolsFrontendHostImplJsErrorReportingEnabledTest,
       NotSentIfNonDevToolsURL) {
  const char16_t* const kNonChromeSourceURLs[] = {
      u"chrome-untrusted://media-app",
      u"chrome-error://chromewebdata/",
      u"chrome-extension://abc123/",
      u"chrome://settings",
      u"about:blank",
  };

  for (const auto* url : kNonChromeSourceURLs) {
    CallOnDidAddMessageToConsole(main_rfh(),
                                 blink::mojom::ConsoleMessageLevel::kError,
                                 kMessage16, 5, url, kStackTrace16);
    task_environment()->RunUntilIdle();
    EXPECT_EQ(processor_->error_report_count(), 0) << url;
  }
}

}  // namespace content

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
