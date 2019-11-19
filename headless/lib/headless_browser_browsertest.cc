// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_macros.h"
#include "headless/public/devtools/domains/inspector.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/tracing.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "net/cert/cert_status_flags.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_MACOSX)
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#endif

using testing::UnorderedElementsAre;

namespace headless {

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyBrowserContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest,
                       CreateAndDoNotDestroyBrowserContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));

  // We check that HeadlessBrowser correctly handles non-closed BrowserContexts.
  // We can rely on Chromium DCHECKs to capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyWebContents) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  // TODO(skyostil): Verify viewport dimensions once we can.

  web_contents->Close();

  EXPECT_TRUE(browser_context->GetAllWebContents().empty());

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest,
                       WebContentsAreDestroyedWithContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());

  // If WebContents are not destroyed, Chromium DCHECKs will capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDoNotDestroyWebContents) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  // If WebContents are not destroyed, Chromium DCHECKs will capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, DestroyAndCreateTwoWebContents) {
  HeadlessBrowserContext* browser_context1 =
      browser()->CreateBrowserContextBuilder().Build();
  EXPECT_TRUE(browser_context1);
  HeadlessWebContents* web_contents1 =
      browser_context1->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents1);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context1));
  EXPECT_THAT(browser_context1->GetAllWebContents(),
              UnorderedElementsAre(web_contents1));

  HeadlessBrowserContext* browser_context2 =
      browser()->CreateBrowserContextBuilder().Build();
  EXPECT_TRUE(browser_context2);
  HeadlessWebContents* web_contents2 =
      browser_context2->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents2);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context1, browser_context2));
  EXPECT_THAT(browser_context1->GetAllWebContents(),
              UnorderedElementsAre(web_contents1));
  EXPECT_THAT(browser_context2->GetAllWebContents(),
              UnorderedElementsAre(web_contents2));

  browser_context1->Close();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context2));
  EXPECT_THAT(browser_context2->GetAllWebContents(),
              UnorderedElementsAre(web_contents2));

  browser_context2->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateWithBadURL) {
  GURL bad_url("not_valid");

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(bad_url)
          .Build();

  EXPECT_FALSE(web_contents);
  EXPECT_TRUE(browser_context->GetAllWebContents().empty());
}

class HeadlessBrowserTestWithProxy : public HeadlessBrowserTest {
 public:
  HeadlessBrowserTestWithProxy()
      : proxy_server_(net::SpawnedTestServer::TYPE_HTTP,
                      base::FilePath(FILE_PATH_LITERAL("headless/test/data"))) {
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    proxy_server_.Stop();
    HeadlessBrowserTest::TearDown();
  }

  net::SpawnedTestServer* proxy_server() { return &proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestWithProxy, SetProxyConfig) {
  std::unique_ptr<net::ProxyConfig> proxy_config(new net::ProxyConfig);
  proxy_config->proxy_rules().ParseFromString(
      proxy_server()->host_port_pair().ToString());
  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProxyConfig(std::move(proxy_config))
          .Build();

  // Load a page which doesn't actually exist, but for which the our proxy
  // returns valid content anyway.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));
  web_contents->Close();
  EXPECT_TRUE(browser_context->GetAllWebContents().empty());
}

// TODO(crbug.com/867447): Flaky on Windows 10 debug.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_WebGLSupported DISABLED_WebGLSupported
#else
#define MAYBE_WebGLSupported WebGLSupported
#endif
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, MAYBE_WebGLSupported) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  EXPECT_TRUE(
      EvaluateScript(web_contents,
                     "(document.createElement('canvas').getContext('webgl')"
                     "    instanceof WebGLRenderingContext)")
          ->GetResult()
          ->GetValue()
          ->GetBool());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ClipboardCopyPasteText) {
  // Tests copy-pasting text with the clipboard in headless mode.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ASSERT_TRUE(clipboard);
  base::string16 paste_text = base::ASCIIToUTF16("Clippy!");
  for (ui::ClipboardBuffer buffer :
       {ui::ClipboardBuffer::kCopyPaste, ui::ClipboardBuffer::kSelection,
        ui::ClipboardBuffer::kDrag}) {
    if (!ui::Clipboard::IsSupportedClipboardBuffer(buffer))
      continue;
    {
      ui::ScopedClipboardWriter writer(buffer);
      writer.WriteText(paste_text);
    }
    base::string16 copy_text;
    clipboard->ReadText(buffer, &copy_text);
    EXPECT_EQ(paste_text, copy_text);
  }
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, DefaultSizes) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  HeadlessBrowser::Options::Builder builder;
  const HeadlessBrowser::Options kDefaultOptions = builder.Build();

#if !defined(OS_MACOSX)
  // On Mac headless does not override the screen dimensions, so they are
  // left with the actual screen values.
  EXPECT_EQ(kDefaultOptions.window_size.width(),
            EvaluateScript(web_contents, "screen.width")
                ->GetResult()
                ->GetValue()
                ->GetInt());
  EXPECT_EQ(kDefaultOptions.window_size.height(),
            EvaluateScript(web_contents, "screen.height")
                ->GetResult()
                ->GetValue()
                ->GetInt());
#endif  // !defined(OS_MACOSX)
  EXPECT_EQ(kDefaultOptions.window_size.width(),
            EvaluateScript(web_contents, "window.innerWidth")
                ->GetResult()
                ->GetValue()
                ->GetInt());
  EXPECT_EQ(kDefaultOptions.window_size.height(),
            EvaluateScript(web_contents, "window.innerHeight")
                ->GetResult()
                ->GetValue()
                ->GetInt());
}

namespace {

class CookieSetter {
 public:
  CookieSetter(HeadlessBrowserTest* browser_test,
               HeadlessWebContents* web_contents,
               std::unique_ptr<network::SetCookieParams> set_cookie_params)
      : browser_test_(browser_test),
        web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetNetwork()->GetExperimental()->SetCookie(
        std::move(set_cookie_params),
        base::BindOnce(&CookieSetter::OnSetCookieResult,
                       base::Unretained(this)));
  }

  ~CookieSetter() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  void OnSetCookieResult(std::unique_ptr<network::SetCookieResult> result) {
    result_ = std::move(result);
    browser_test_->FinishAsynchronousTest();
  }

  std::unique_ptr<network::SetCookieResult> TakeResult() {
    return std::move(result_);
  }

 private:
  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  std::unique_ptr<network::SetCookieResult> result_;

  DISALLOW_COPY_AND_ASSIGN(CookieSetter);
};

}  // namespace

// TODO(skyostil): This test currently relies on being able to run a shell
// script.
#if defined(OS_POSIX)
class HeadlessBrowserRendererCommandPrefixTest : public HeadlessBrowserTest {
 public:
  const base::FilePath& launcher_stamp() const { return launcher_stamp_; }

  void SetUp() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::CreateTemporaryFile(&launcher_stamp_);

    FILE* launcher_file = base::CreateAndOpenTemporaryFile(&launcher_script_);
    fprintf(launcher_file, "#!/bin/sh\n");
    fprintf(launcher_file, "echo $@ > %s\n", launcher_stamp_.value().c_str());
    fprintf(launcher_file, "exec $@\n");
    fclose(launcher_file);
#if !defined(OS_FUCHSIA)
    base::SetPosixFilePermissions(launcher_script_,
                                  base::FILE_PERMISSION_READ_BY_USER |
                                      base::FILE_PERMISSION_EXECUTE_BY_USER);
#endif  // !defined(OS_FUCHSIA)

    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    if (!launcher_script_.empty())
      base::DeleteFile(launcher_script_, false);
    if (!launcher_stamp_.empty())
      base::DeleteFile(launcher_stamp_, false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("--no-sandbox");
    command_line->AppendSwitchASCII("--renderer-cmd-prefix",
                                    launcher_script_.value());
  }

 private:
  base::FilePath launcher_stamp_;
  base::FilePath launcher_script_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserRendererCommandPrefixTest, Prefix) {
  base::ThreadRestrictions::SetIOAllowed(true);
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Make sure the launcher was invoked when starting the renderer.
  std::string stamp;
  EXPECT_TRUE(base::ReadFileToString(launcher_stamp(), &stamp));
  EXPECT_GE(stamp.find("--type=renderer"), 0u);
}
#endif  // defined(OS_POSIX)

class CrashReporterTest : public HeadlessBrowserTest,
                          public HeadlessWebContents::Observer,
                          inspector::ExperimentalObserver {
 public:
  CrashReporterTest() {}
  ~CrashReporterTest() override = default;

  void SetUp() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::CreateNewTempDirectory(FILE_PATH_LITERAL("CrashReporterTest"),
                                 &crash_dumps_dir_);
    EXPECT_FALSE(options()->enable_crash_reporter);
    options()->enable_crash_reporter = true;
    options()->crash_dumps_dir = crash_dumps_dir_;
    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    base::ThreadRestrictions::SetIOAllowed(true);
    base::DeleteFile(crash_dumps_dir_, /* recursive */ false);
  }

  // HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override {
    EXPECT_TRUE(web_contents_->GetDevToolsTarget());
    devtools_client_ = HeadlessDevToolsClient::Create();
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetInspector()->GetExperimental()->AddObserver(this);
  }

  // inspector::ExperimentalObserver implementation:
  void OnTargetCrashed(const inspector::TargetCrashedParams&) override {
    FinishAsynchronousTest();
  }

 protected:
  HeadlessBrowserContext* browser_context_ = nullptr;
  HeadlessWebContents* web_contents_ = nullptr;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  base::FilePath crash_dumps_dir_;
};

// TODO(skyostil): Minidump generation currently is only supported on Linux and
// Mac.
#if (defined(HEADLESS_USE_BREAKPAD) || defined(OS_MACOSX)) && \
    !defined(ADDRESS_SANITIZER)
#define MAYBE_GenerateMinidump GenerateMinidump
#else
#define MAYBE_GenerateMinidump DISABLED_GenerateMinidump
#endif  // defined(HEADLESS_USE_BREAKPAD) || defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(CrashReporterTest, MAYBE_GenerateMinidump) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  // Navigates a tab to chrome://crash and checks that a minidump is generated.
  // Note that we only test renderer crashes here -- browser crashes need to be
  // tested with a separate harness.
  //
  // The case where crash reporting is disabled is covered by
  // HeadlessCrashObserverTest.
  browser_context_ = browser()->CreateBrowserContextBuilder().Build();

  web_contents_ = browser_context_->CreateWebContentsBuilder()
                      .SetInitialURL(GURL(content::kChromeUICrashURL))
                      .Build();

  web_contents_->AddObserver(this);
  RunAsynchronousTest();

  // The target has crashed and should no longer be there.
  EXPECT_FALSE(web_contents_->GetDevToolsTarget());

  // Check that one minidump got created.
  {
    base::ThreadRestrictions::SetIOAllowed(true);

#if defined(OS_MACOSX)
    auto database = crashpad::CrashReportDatabase::Initialize(crash_dumps_dir_);
    std::vector<crashpad::CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              crashpad::CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
#else
    base::FileEnumerator it(crash_dumps_dir_, /* recursive */ false,
                            base::FileEnumerator::FILES);
    base::FilePath minidump = it.Next();
    EXPECT_FALSE(minidump.empty());
    EXPECT_EQ(FILE_PATH_LITERAL(".dmp"), minidump.Extension());
    EXPECT_TRUE(it.Next().empty());
#endif
  }

  web_contents_->RemoveObserver(this);
  web_contents_->Close();
  web_contents_ = nullptr;

  browser_context_->Close();
  browser_context_ = nullptr;
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, PermissionManagerAlwaysASK) {
  GURL url("https://example.com");

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* headless_web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(headless_web_contents);

  HeadlessWebContentsImpl* web_contents =
      HeadlessWebContentsImpl::From(headless_web_contents);

  content::PermissionControllerDelegate* permission_controller_delegate =
      web_contents->browser_context()->GetPermissionControllerDelegate();
  EXPECT_NE(nullptr, permission_controller_delegate);

  // Check that the permission manager returns ASK for a given permission type.
  EXPECT_EQ(blink::mojom::PermissionStatus::ASK,
            permission_controller_delegate->GetPermissionStatus(
                content::PermissionType::NOTIFICATIONS, url, url));
}

namespace {

class TraceHelper : public tracing::ExperimentalObserver {
 public:
  TraceHelper(HeadlessBrowserTest* browser_test, HeadlessDevToolsTarget* target)
      : browser_test_(browser_test),
        target_(target),
        client_(HeadlessDevToolsClient::Create()),
        tracing_data_(std::make_unique<base::ListValue>()) {
    EXPECT_FALSE(target_->IsAttached());
    target_->AttachClient(client_.get());
    EXPECT_TRUE(target_->IsAttached());

    client_->GetTracing()->GetExperimental()->AddObserver(this);

    client_->GetTracing()->GetExperimental()->Start(
        tracing::StartParams::Builder().Build(),
        base::BindOnce(&TraceHelper::OnTracingStarted, base::Unretained(this)));
  }

  ~TraceHelper() override {
    target_->DetachClient(client_.get());
    EXPECT_FALSE(target_->IsAttached());
  }

  std::unique_ptr<base::ListValue> TakeTracingData() {
    return std::move(tracing_data_);
  }

 private:
  void OnTracingStarted(std::unique_ptr<tracing::StartResult>) {
    // We don't need the callback from End, but the OnTracingComplete event.
    client_->GetTracing()->GetExperimental()->End(
        tracing::EndParams::Builder().Build());
  }

  // tracing::ExperimentalObserver implementation:
  void OnDataCollected(const tracing::DataCollectedParams& params) override {
    for (const auto& value : *params.GetValue()) {
      tracing_data_->Append(value->CreateDeepCopy());
    }
  }

  void OnTracingComplete(
      const tracing::TracingCompleteParams& params) override {
    browser_test_->FinishAsynchronousTest();
  }

  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessDevToolsTarget* target_;     // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> client_;

  std::unique_ptr<base::ListValue> tracing_data_;

  DISALLOW_COPY_AND_ASSIGN(TraceHelper);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, TraceUsingBrowserDevToolsTarget) {
  HeadlessDevToolsTarget* target = browser()->GetDevToolsTarget();
  EXPECT_NE(nullptr, target);

  TraceHelper helper(this, target);
  RunAsynchronousTest();

  std::unique_ptr<base::ListValue> tracing_data = helper.TakeTracingData();
  EXPECT_TRUE(tracing_data);
  EXPECT_LT(0u, tracing_data->GetSize());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, WindowPrint) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  EXPECT_FALSE(
      EvaluateScript(web_contents, "window.print()")->HasExceptionDetails());
}

class HeadlessBrowserAllowInsecureLocalhostTest : public HeadlessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserAllowInsecureLocalhostTest,
                       AllowInsecureLocalhostFlag) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server.ServeFilesFromSourceDirectory("headless/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/hello.html");

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContentsImpl* web_contents =
      HeadlessWebContentsImpl::From(browser_context->CreateWebContentsBuilder()
                                        .SetInitialURL(test_url)
                                        .Build());

  // If the certificate fails to validate, this should fail.
  EXPECT_TRUE(WaitForLoad(web_contents));
}

class HeadlessBrowserTestAppendCommandLineFlags : public HeadlessBrowserTest {
 public:
  HeadlessBrowserTestAppendCommandLineFlags() {
    options()->append_command_line_flags_callback = base::Bind(
        &HeadlessBrowserTestAppendCommandLineFlags::AppendCommandLineFlags,
        base::Unretained(this));
  }

  void AppendCommandLineFlags(base::CommandLine* command_line,
                              HeadlessBrowserContext* child_browser_context,
                              const std::string& child_process_type,
                              int child_process_id) {
    if (child_process_type != "renderer")
      return;

    callback_was_run_ = true;
    EXPECT_LE(0, child_process_id);
    EXPECT_NE(nullptr, command_line);
    EXPECT_NE(nullptr, child_browser_context);
  }

 protected:
  bool callback_was_run_ = false;
};

#if defined(OS_WIN)
// Flaky on Win ASAN. See https://crbug.com/884095.
#define MAYBE_AppendChildProcessCommandLineFlags \
  DISABLED_AppendChildProcessCommandLineFlags
#else
#define MAYBE_AppendChildProcessCommandLineFlags \
  AppendChildProcessCommandLineFlags
#endif
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestAppendCommandLineFlags,
                       MAYBE_AppendChildProcessCommandLineFlags) {
  // Create a new renderer process, and verify that callback was executed.
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("about:blank"))
          .Build();

  EXPECT_TRUE(callback_was_run_);

  // Used only for lifetime.
  (void)web_contents;
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ServerWantsClientCertificate) {
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;

  net::SpawnedTestServer server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("headless/test/data")));
  EXPECT_TRUE(server.Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(server.GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, AIAFetching) {
  net::SpawnedTestServer::SSLOptions ssl_options(
      net::SpawnedTestServer::SSLOptions::CERT_AUTO_AIA_INTERMEDIATE);
  net::SpawnedTestServer server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(server.Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();
  browser()->SetDefaultBrowserContext(browser_context);

  GURL url = server.GetURL("/defaultresponse");
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().SetInitialURL(url).Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  content::NavigationEntry* last_entry =
      HeadlessWebContentsImpl::From(web_contents)
          ->web_contents()
          ->GetController()
          .GetLastCommittedEntry();
  EXPECT_FALSE(net::IsCertStatusError(last_entry->GetSSL().cert_status));
  EXPECT_EQ(url, last_entry->GetURL());
}

}  // namespace headless
