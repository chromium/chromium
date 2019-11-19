// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

namespace headless {
namespace {

class SynchronousLoadObserver {
 public:
  SynchronousLoadObserver(HeadlessBrowserTest* browser_test,
                          HeadlessWebContents* web_contents)
      : web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    load_observer_.reset(new LoadObserver(
        devtools_client_.get(),
        base::Bind(&HeadlessBrowserTest::FinishAsynchronousTest,
                   base::Unretained(browser_test))));
  }

  ~SynchronousLoadObserver() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  bool navigation_succeeded() const {
    return load_observer_->navigation_succeeded();
  }

 private:
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  std::unique_ptr<LoadObserver> load_observer_;
};

class EvaluateHelper {
 public:
  EvaluateHelper(HeadlessBrowserTest* browser_test,
                 HeadlessWebContents* web_contents,
                 const std::string& script_to_eval)
      : browser_test_(browser_test),
        web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetRuntime()->Evaluate(
        script_to_eval, base::BindOnce(&EvaluateHelper::OnEvaluateResult,
                                       base::Unretained(this)));
  }

  ~EvaluateHelper() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    result_ = std::move(result);
    browser_test_->FinishAsynchronousTest();
  }

  std::unique_ptr<runtime::EvaluateResult> TakeResult() {
    return std::move(result_);
  }

 private:
  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  std::unique_ptr<runtime::EvaluateResult> result_;

  DISALLOW_COPY_AND_ASSIGN(EvaluateHelper);
};

}  // namespace

LoadObserver::LoadObserver(HeadlessDevToolsClient* devtools_client,
                           base::OnceClosure callback)
    : callback_(std::move(callback)),
      devtools_client_(devtools_client),
      navigation_succeeded_(true) {
  devtools_client_->GetNetwork()->AddObserver(this);
  devtools_client_->GetNetwork()->Enable();
  devtools_client_->GetPage()->AddObserver(this);
  devtools_client_->GetPage()->Enable();
}

LoadObserver::~LoadObserver() {
  devtools_client_->GetNetwork()->RemoveObserver(this);
  devtools_client_->GetPage()->RemoveObserver(this);
}

void LoadObserver::OnLoadEventFired(const page::LoadEventFiredParams& params) {
  std::move(callback_).Run();
}

void LoadObserver::OnResponseReceived(
    const network::ResponseReceivedParams& params) {
  if (params.GetResponse()->GetStatus() != 200 ||
      params.GetResponse()->GetUrl() == content::kUnreachableWebDataURL) {
    navigation_succeeded_ = false;
  }
}

HeadlessBrowserTest::HeadlessBrowserTest() {
#if defined(OS_MACOSX)
  // On Mac the source root is not set properly. We override it by assuming
  // that is two directories up from the execution test file.
  base::FilePath dir_exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &dir_exe_path));
  dir_exe_path = dir_exe_path.Append("../../");
  CHECK(base::PathService::Override(base::DIR_SOURCE_ROOT, dir_exe_path));
#endif  // defined(OS_MACOSX)
  base::FilePath headless_test_data(FILE_PATH_LITERAL("headless/test/data"));
  CreateTestServer(headless_test_data);
}

void HeadlessBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable GPU usage (i.e., SwiftShader, hardware GL on macOS) in all tests
  // since that's the default configuration of --headless.
  command_line->AppendSwitch(switches::kUseGpuInTests);
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

void HeadlessBrowserTest::SetUpWithoutGPU() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

HeadlessBrowserTest::~HeadlessBrowserTest() = default;

void HeadlessBrowserTest::PreRunTestOnMainThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Pump startup related events.
  base::RunLoop().RunUntilIdle();
}

void HeadlessBrowserTest::PostRunTestOnMainThread() {
  browser()->Shutdown();
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
}

HeadlessBrowser* HeadlessBrowserTest::browser() const {
  return HeadlessContentMainDelegate::GetInstance()->browser();
}

HeadlessBrowser::Options* HeadlessBrowserTest::options() const {
  return HeadlessContentMainDelegate::GetInstance()->browser()->options();
}

bool HeadlessBrowserTest::WaitForLoad(HeadlessWebContents* web_contents) {
  HeadlessWebContentsImpl* web_contents_impl =
      HeadlessWebContentsImpl::From(web_contents);
  content::TestNavigationObserver observer(web_contents_impl->web_contents(),
                                           1);
  observer.Wait();
  return observer.last_navigation_succeeded();
}

void HeadlessBrowserTest::WaitForLoadAndGainFocus(
    HeadlessWebContents* web_contents) {
  content::WebContents* content =
      HeadlessWebContentsImpl::From(web_contents)->web_contents();

  // To finish loading and to gain focus are two independent events. Which one
  // is issued first is undefined. The following code is waiting on both, in any
  // order.
  content::TestNavigationObserver load_observer(content, 1);
  content::FrameFocusedObserver focus_observer(content->GetMainFrame());
  load_observer.Wait();
  focus_observer.Wait();
}

std::unique_ptr<runtime::EvaluateResult> HeadlessBrowserTest::EvaluateScript(
    HeadlessWebContents* web_contents,
    const std::string& script) {
  EvaluateHelper helper(this, web_contents, script);
  RunAsynchronousTest();
  return helper.TakeResult();
}

void HeadlessBrowserTest::RunAsynchronousTest() {
  EXPECT_FALSE(run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  PreRunAsynchronousTest();
  run_loop_->Run();
  PostRunAsynchronousTest();
  run_loop_ = nullptr;
}

void HeadlessBrowserTest::FinishAsynchronousTest() {
  run_loop_->Quit();
}

HeadlessAsyncDevTooledBrowserTest::HeadlessAsyncDevTooledBrowserTest()
    : browser_context_(nullptr),
      web_contents_(nullptr),
      render_process_exited_(false) {}

HeadlessAsyncDevTooledBrowserTest::~HeadlessAsyncDevTooledBrowserTest() =
    default;

void HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady() {
  EXPECT_TRUE(web_contents_->GetDevToolsTarget());
  web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
#if defined(OS_MACOSX)
  devtools_client_->GetEmulation()->SetDeviceMetricsOverride(
      emulation::SetDeviceMetricsOverrideParams::Builder()
          .SetWidth(0)
          .SetHeight(0)
          .SetDeviceScaleFactor(1)
          .SetMobile(false)
          .Build(),
      base::BindOnce(
          [](HeadlessAsyncDevTooledBrowserTest* self) {
            self->RunDevTooledTest();
          },
          base::Unretained(this)));
#else
  RunDevTooledTest();
#endif
}

void HeadlessAsyncDevTooledBrowserTest::RenderProcessExited(
    base::TerminationStatus status,
    int exit_code) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;

  FinishAsynchronousTest();
  render_process_exited_ = true;
  FAIL() << "Abnormal renderer termination";
}

void HeadlessAsyncDevTooledBrowserTest::RunTest() {
  devtools_client_ = HeadlessDevToolsClient::Create();
  browser_devtools_client_ = HeadlessDevToolsClient::Create();
  interceptor_ = std::make_unique<TestNetworkInterceptor>();
  HeadlessBrowserContext::Builder builder =
      browser()->CreateBrowserContextBuilder();
  CustomizeHeadlessBrowserContext(builder);
  browser_context_ = builder.Build();

  browser()->SetDefaultBrowserContext(browser_context_);
  browser()->GetDevToolsTarget()->AttachClient(browser_devtools_client_.get());

  HeadlessWebContents::Builder web_contents_builder =
      browser_context_->CreateWebContentsBuilder();
  web_contents_builder.SetEnableBeginFrameControl(GetEnableBeginFrameControl());
  CustomizeHeadlessWebContents(web_contents_builder);
  web_contents_ = web_contents_builder.Build();

  web_contents_->AddObserver(this);

  RunAsynchronousTest();
  interceptor_.reset();
  if (!render_process_exited_)
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  web_contents_->RemoveObserver(this);
  web_contents_->Close();
  web_contents_ = nullptr;
  browser()->GetDevToolsTarget()->DetachClient(browser_devtools_client_.get());
  browser_context_->Close();
  browser_context_ = nullptr;
}

bool HeadlessAsyncDevTooledBrowserTest::GetEnableBeginFrameControl() {
  return false;
}

void HeadlessAsyncDevTooledBrowserTest::CustomizeHeadlessBrowserContext(
    HeadlessBrowserContext::Builder& builder) {}

void HeadlessAsyncDevTooledBrowserTest::CustomizeHeadlessWebContents(
    HeadlessWebContents::Builder& builder) {}

}  // namespace headless
