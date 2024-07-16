// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_browser.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobars_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "third_party/crashpad/crashpad/client/crash_report_database.h"  // nogncheck
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/mac/mac_util.h"
#endif

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

using testing::UnorderedElementsAre;

namespace headless {

namespace {

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
      : proxy_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    proxy_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("headless/test/data")));
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    EXPECT_TRUE(proxy_server_.ShutdownAndWaitUntilComplete());
    HeadlessBrowserTest::TearDown();
  }

  net::EmbeddedTestServer* proxy_server() { return &proxy_server_; }

 private:
  net::EmbeddedTestServer proxy_server_;
};

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40697469): Fix this test on Fuchsia and re-enable.
#define MAYBE_SetProxyConfig DISABLED_SetProxyConfig
#else
#define MAYBE_SetProxyConfig SetProxyConfig
#endif
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestWithProxy, MAYBE_SetProxyConfig) {
  std::unique_ptr<net::ProxyConfig> proxy_config(new net::ProxyConfig);
  proxy_config->proxy_rules().ParseFromString(
      proxy_server()->host_port_pair().ToString());
  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProxyConfig(std::move(proxy_config))
          .Build();

  // Load a page which doesn't actually exist, but for which our proxy
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

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, WebGLSupported) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  bool expected_support = true;
#if BUILDFLAG(IS_APPLE)
  LOG(INFO) << "CPU type: " << static_cast<int>(base::mac::GetCPUType());
  if (base::mac::GetCPUType() == base::mac::CPUType::kArm) {
    expected_support = false;
  }
#elif BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  expected_support = false;
#endif  // BUILDFLAG(IS_APPLE)

  EXPECT_THAT(
      EvaluateScript(web_contents,
                     "(document.createElement('canvas').getContext('webgl')"
                     "    instanceof WebGLRenderingContext)"),
      DictHasValue("result.result.value", expected_support));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ClipboardCopyPasteText) {
  // Tests copy-pasting text with the clipboard in headless mode.
  ui::Clipboard* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);

  static const struct ClipboardBufferInfo {
    ui::ClipboardBuffer buffer;
    std::u16string paste_text;
  } clipboard_buffers[] = {
      {ui::ClipboardBuffer::kCopyPaste, u"kCopyPaste"},
      {ui::ClipboardBuffer::kSelection, u"kSelection"},
      {ui::ClipboardBuffer::kDrag, u"kDrag"},
  };

  // Check basic write/read ops into each buffer type.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (!ui::Clipboard::IsSupportedClipboardBuffer(buffer))
      continue;
    {
      ui::ScopedClipboardWriter writer(buffer);
      writer.WriteText(paste_text);
    }
    std::u16string copy_text;
    clipboard->ReadText(buffer, /* data_dst = */ nullptr, &copy_text);
    EXPECT_EQ(paste_text, copy_text);
  }

  // Verify that different clipboard buffer data is independent.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (!ui::Clipboard::IsSupportedClipboardBuffer(buffer)) {
      continue;
    }
    std::u16string copy_text;
    clipboard->ReadText(buffer, /* data_dst = */ nullptr, &copy_text);
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

  const int expected_width = kDefaultOptions.window_size.width();
  const int expected_height = kDefaultOptions.window_size.height();

  EXPECT_THAT(EvaluateScript(web_contents, "screen.width"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "screen.height"),
              DictHasValue("result.result.value", expected_height));

  EXPECT_THAT(EvaluateScript(web_contents, "window.outerWidth"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "window.outerHeight"),
              DictHasValue("result.result.value", expected_height));

  EXPECT_THAT(EvaluateScript(web_contents, "window.innerWidth"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "window.innerHeight"),
              DictHasValue("result.result.value", expected_height));
}

class HeadlessBrowserWindowSizeTest : public HeadlessBrowserTest {
 public:
  static constexpr gfx::Size kWindowSize = {1920, 1080};

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWindowSize,
        base::StringPrintf("%u,%u", kWindowSize.width(), kWindowSize.height()));
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserWindowSizeTest, WindowSize) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  const int expected_width = kWindowSize.width();
  const int expected_height = kWindowSize.height();

  EXPECT_THAT(EvaluateScript(web_contents, "screen.width"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "screen.height"),
              DictHasValue("result.result.value", expected_height));

  EXPECT_THAT(EvaluateScript(web_contents, "window.outerWidth"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "window.outerHeight"),
              DictHasValue("result.result.value", expected_height));

  EXPECT_THAT(EvaluateScript(web_contents, "window.innerWidth"),
              DictHasValue("result.result.value", expected_width));
  EXPECT_THAT(EvaluateScript(web_contents, "window.innerHeight"),
              DictHasValue("result.result.value", expected_height));
}

// TODO(skyostil): This test currently relies on being able to run a shell
// script.
#if BUILDFLAG(IS_POSIX)
class HeadlessBrowserRendererCommandPrefixTest : public HeadlessBrowserTest {
 public:
  const base::FilePath& launcher_stamp() const { return launcher_stamp_; }

  void SetUp() override {
    base::CreateTemporaryFile(&launcher_stamp_);

    base::ScopedFILE launcher_file =
        base::CreateAndOpenTemporaryStream(&launcher_script_);
    fprintf(launcher_file.get(), "#!/bin/sh\n");
    fprintf(launcher_file.get(), "echo $@ > %s\n",
            launcher_stamp_.value().c_str());
    fprintf(launcher_file.get(), "exec $@\n");
    launcher_file.reset();
#if !BUILDFLAG(IS_FUCHSIA)
    base::SetPosixFilePermissions(launcher_script_,
                                  base::FILE_PERMISSION_READ_BY_USER |
                                      base::FILE_PERMISSION_EXECUTE_BY_USER);
#endif  // !BUILDFLAG(IS_FUCHSIA)

    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    if (!launcher_script_.empty())
      base::DeleteFile(launcher_script_);
    if (!launcher_stamp_.empty())
      base::DeleteFile(launcher_stamp_);
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
  base::ScopedAllowBlockingForTesting allow_blocking;
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
#endif  // BUILDFLAG(IS_POSIX)

class CrashReporterTest : public HeadlessBrowserTest,
                          public HeadlessWebContents::Observer {
 public:
  CrashReporterTest() {}
  ~CrashReporterTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CreateNewTempDirectory(FILE_PATH_LITERAL("CrashReporterTest"),
                                 &crash_dumps_dir_);
    command_line->AppendSwitch(switches::kEnableCrashReporter);
    command_line->AppendSwitchPath(switches::kCrashDumpsDir, crash_dumps_dir_);
    HeadlessBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDown() override {
    base::DeleteFile(crash_dumps_dir_);
  }

  // HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override {
    devtools_client_.AttachToWebContents(
        HeadlessWebContentsImpl::From(web_contents_)->web_contents());

    devtools_client_.AddEventHandler(
        "Inspector.targetCrashed",
        base::BindRepeating(&CrashReporterTest::OnTargetCrashed,
                            base::Unretained(this)));
  }

  void OnTargetCrashed(const base::Value::Dict&) { FinishAsynchronousTest(); }

 protected:
  raw_ptr<HeadlessBrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_ = nullptr;
  raw_ptr<HeadlessWebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  SimpleDevToolsProtocolClient devtools_client_;
  base::FilePath crash_dumps_dir_;
};

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(CrashReporterTest, GenerateMinidump) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  // Navigates a tab to chrome://crash and checks that a minidump is generated.
  // Note that we only test renderer crashes here -- browser crashes need to be
  // tested with a separate harness.
  //
  // The case where crash reporting is disabled is covered by
  // HeadlessCrashObserverTest.
  browser_context_ = browser()->CreateBrowserContextBuilder().Build();

  web_contents_ = browser_context_->CreateWebContentsBuilder()
                      .SetInitialURL(GURL(blink::kChromeUICrashURL))
                      .Build();

  web_contents_->AddObserver(this);
  RunAsynchronousTest();

  // Check that one minidump got created.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    auto database = crashpad::CrashReportDatabase::Initialize(crash_dumps_dir_);
    std::vector<crashpad::CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              crashpad::CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
  }

  web_contents_->RemoveObserver(this);
  web_contents_->Close();
  web_contents_ = nullptr;

  browser_context_->Close();
  browser_context_ = nullptr;
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_WIN)

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
                blink::PermissionType::NOTIFICATIONS, url, url));
}

class BrowserTargetTracingTest : public HeadlessBrowserTest {
 public:
  BrowserTargetTracingTest() = default;

 protected:
  void RunTest() {
    browser_devtools_client_.AttachToBrowser();

    browser_devtools_client_.AddEventHandler(
        "Tracing.dataCollected",
        base::BindRepeating(&BrowserTargetTracingTest::OnDataCollected,
                            base::Unretained(this)));

    browser_devtools_client_.AddEventHandler(
        "Tracing.tracingComplete",
        base::BindRepeating(&BrowserTargetTracingTest::OnTracingComplete,
                            base::Unretained(this)));

    browser_devtools_client_.SendCommand(
        "Tracing.start",
        base::BindOnce(&BrowserTargetTracingTest::OnTracingStarted,
                       base::Unretained(this)));

    RunAsynchronousTest();

    browser_devtools_client_.DetachClient();
  }

 private:
  void OnTracingStarted(base::Value::Dict) {
    browser_devtools_client_.SendCommand("Tracing.end");
  }

  void OnDataCollected(const base::Value::Dict& params) {
    const base::Value::List* value_list =
        params.FindListByDottedPath("params.value");
    ASSERT_NE(value_list, nullptr);
    for (const auto& value : *value_list) {
      tracing_data_.Append(value.Clone());
    }
  }

  void OnTracingComplete(const base::Value::Dict&) {
    EXPECT_LT(0u, tracing_data_.size());

    FinishAsynchronousTest();
  }

  SimpleDevToolsProtocolClient browser_devtools_client_;

  base::Value::List tracing_data_;
};

// Flaky, http://crbug.com/1269261.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BrowserTargetTracing DISABLED_BrowserTargetTracing
#else
#define MAYBE_BrowserTargetTracing BrowserTargetTracing
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(BrowserTargetTracingTest, MAYBE_BrowserTargetTracing) {
  RunTest();
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
  EXPECT_THAT(EvaluateScript(web_contents, "window.print()"),
              Not(DictHasKey("exceptionDetails")));
}

class HeadlessBrowserAllowInsecureLocalhostTest : public HeadlessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kAllowInsecureLocalhost);
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

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40697469): Fix this test on Fuchsia and re-enable.
#define MAYBE_ServerWantsClientCertificate DISABLED_ServerWantsClientCertificate
#else
#define MAYBE_ServerWantsClientCertificate ServerWantsClientCertificate
#endif
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest,
                       MAYBE_ServerWantsClientCertificate) {
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::CERT_AUTO, server_config);
  server.ServeFilesFromSourceDirectory("headless/test/data");
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
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.intermediate = net::EmbeddedTestServer::IntermediateType::kByAIA;
  server.SetSSLConfig(cert_config);
  server.AddDefaultHandlers(base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
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

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, BadgingAPI) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  GURL url = embedded_test_server()->GetURL("/badging_api.html");
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().SetInitialURL(url).Build();

  EXPECT_TRUE(WaitForLoad(web_contents));
}

class HeadlessBrowserTestWithExplicitlyAllowedPorts
    : public HeadlessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    if (is_port_allowed()) {
      command_line->AppendSwitchASCII(switches::kExplicitlyAllowedPorts,
                                      "10080");
    }
  }

  bool is_port_allowed() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(HeadlessBrowserTestWithExplicitlyAllowedPorts,
                         HeadlessBrowserTestWithExplicitlyAllowedPorts,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithExplicitlyAllowedPorts,
                       AllowedPort) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://127.0.0.1:10080"))
          .Build();

  // If the port is allowed, the request is expected to fail for
  // reasons other than ERR_UNSAFE_PORT.
  net::Error error = net::OK;
  EXPECT_FALSE(WaitForLoad(web_contents, &error));
  if (is_port_allowed())
    EXPECT_NE(error, net::ERR_UNSAFE_PORT);
  else
    EXPECT_EQ(error, net::ERR_UNSAFE_PORT);
}

// This assures that both string and data blink resources are
// present. These are essential for correct rendering.
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, LocalizedResources) {
  EXPECT_THAT(
      ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceString(
          IDS_FORM_SUBMIT_LABEL),
      testing::Eq("Submit"));
  EXPECT_THAT(ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_UASTYLE_HTML_CSS),
              testing::Ne(""));
}

class SelectFileDialogHeadlessBrowserTest
    : public HeadlessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<const char*, ui::SelectFileDialog::Type>> {
 public:
  static constexpr char kTestMountPoint[] = "testfs";

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Register an external mount point to test support for virtual paths.
    // This maps the virtual path a native local path to make these tests work
    // on all platforms.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kTestMountPoint, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), temp_dir_.GetPath());

    HeadlessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable write access.
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalWebPlatformFeatures);
  }

  void TearDown() override {
    HeadlessBrowserTest::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kTestMountPoint);
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void WaitForSelectFileDialogCallback() {
    if (select_file_dialog_type_ != ui::SelectFileDialog::SELECT_NONE)
      return;

    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void OnSelectFileDialogCallback(ui::SelectFileDialog::Type type) {
    select_file_dialog_type_ = type;

    if (run_loop_)
      run_loop_->Quit();
  }

  const char* file_dialog_script() { return std::get<0>(GetParam()); }
  ui::SelectFileDialog::Type expected_type() { return std::get<1>(GetParam()); }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ScopedTempDir temp_dir_;
  ui::SelectFileDialog::Type select_file_dialog_type_ =
      ui::SelectFileDialog::SELECT_NONE;
};

INSTANTIATE_TEST_SUITE_P(
    SelectFileDialogHeadlessBrowserTest,
    SelectFileDialogHeadlessBrowserTest,
    testing::Values(std::make_tuple("window.showOpenFilePicker()",
                                    ui::SelectFileDialog::SELECT_OPEN_FILE),
                    std::make_tuple("window.showSaveFilePicker()",
                                    ui::SelectFileDialog::SELECT_SAVEAS_FILE),
                    std::make_tuple("window.showDirectoryPicker()",
                                    ui::SelectFileDialog::SELECT_FOLDER)));

IN_PROC_BROWSER_TEST_P(SelectFileDialogHeadlessBrowserTest, SelectFileDialog) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  ASSERT_TRUE(WaitForLoad(web_contents));

  // Select file dialog will not be shown if the owning frame does not
  // have user activation, see VerifyIsAllowedToShowFilePicker in
  // third_party/blink/renderer/.../global_file_system_access.cc
  content::WebContents* content =
      HeadlessWebContentsImpl::From(web_contents)->web_contents();
  content::RenderFrameHost* main_frame = content->GetPrimaryMainFrame();
  main_frame->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);

  HeadlessSelectFileDialogFactory::SetSelectFileDialogOnceCallbackForTests(
      base::BindOnce(
          &SelectFileDialogHeadlessBrowserTest::OnSelectFileDialogCallback,
          base::Unretained(this)));

  EvaluateScript(web_contents, file_dialog_script());
  WaitForSelectFileDialogCallback();

  EXPECT_EQ(select_file_dialog_type_, expected_type());
}

// TODO(crbug.com/40285755): Flaky on all builders.
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, DISABLED_NetworkServiceCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* headless_web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  ASSERT_TRUE(WaitForLoad(headless_web_contents));

  SimulateNetworkServiceCrash();

  content::WebContents* wc =
      HeadlessWebContentsImpl::From(headless_web_contents)->web_contents();
  const GURL new_url = embedded_test_server()->GetURL("/blue_page.html");
  // Wait for navigaitons including those non-committed and re-try as needed,
  // as a navigation may be aborted during network service restart.
  do {
    wc->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(new_url));

    content::TestNavigationObserver nav_observer(
        wc, /* expected_number_of_navigations */ 1,
        content::MessageLoopRunner::QuitMode::IMMEDIATE,
        /* ignore_uncommitted_navigations */ false);
    nav_observer.Wait();
  } while (wc->GetController().GetLastCommittedEntry()->GetURL() != new_url);
}

// Infobar tests -------------------------------------------------------------

class TestInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit TestInfoBarDelegate(int buttons) : buttons_(buttons) {}

  TestInfoBarDelegate(const TestInfoBarDelegate&) = delete;
  TestInfoBarDelegate& operator=(const TestInfoBarDelegate&) = delete;

  ~TestInfoBarDelegate() override = default;

  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     int buttons) {
    infobar_manager->AddInfoBar(std::make_unique<infobars::InfoBar>(
        std::make_unique<TestInfoBarDelegate>(buttons)));
  }

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return TEST_INFOBAR;
  }
  std::u16string GetMessageText() const override {
    return buttons_ ? u"BUTTON" : u"";
  }
  int GetButtons() const override { return buttons_; }

 private:
  int buttons_;
};

class HeadlessInfobarBrowserTest : public HeadlessBrowserTest,
                                   public testing::WithParamInterface<bool> {
 public:
  HeadlessInfobarBrowserTest() = default;
  ~HeadlessInfobarBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTest::SetUpCommandLine(command_line);
    if (disable_infobars()) {
      command_line->AppendSwitch(::switches::kDisableInfoBars);
    }
  }

  bool disable_infobars() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessInfobarBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(HeadlessInfobarBrowserTest, InfoBarsCanBeDisabled) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* headless_web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  ASSERT_TRUE(WaitForLoad(headless_web_contents));

  content::WebContents* web_contents =
      HeadlessWebContentsImpl::From(headless_web_contents)->web_contents();
  ASSERT_TRUE(web_contents);

  auto infobar_manager =
      std::make_unique<infobars::ContentInfoBarManager>(web_contents);
  ASSERT_THAT(infobar_manager->infobars(), testing::IsEmpty());

  TestInfoBarDelegate::Create(infobar_manager.get(),
                              ConfirmInfoBarDelegate::BUTTON_NONE);
  TestInfoBarDelegate::Create(infobar_manager.get(),
                              ConfirmInfoBarDelegate::BUTTON_OK);

  // The infobar with a button should appear even if infobars are disabled.
  EXPECT_THAT(infobar_manager->infobars(),
              testing::SizeIs(disable_infobars() ? 1 : 2));
}

}  // namespace

}  // namespace headless
