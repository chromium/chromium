// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#endif

namespace content {

namespace {

// The pattern to catch messages printed by the browser when navigation to a
// URL is blocked. Navigation to filesystem: URLs uses a slightly different
// message than other blocked schemes, so use a wildcard to match both.
const char kNavigationBlockedMessage[] = "Not allowed to navigate *to %s URL:*";

// The message printed by the data or filesystem URL when it successfully
// navigates.
const char kNavigationSuccessfulMessage[] = "NAVIGATION_SUCCESSFUL";

// A "Hello World" pdf.
const char kPDF[] =
    "%PDF-1.7\n"
    "1 0 obj << /Type /Page /Parent 3 0 R /Resources 5 0 R /Contents 2 0 R >>\n"
    "endobj\n"
    "2 0 obj << /Length 51 >>\n"
    " stream BT\n"
    " /F1 12 Tf\n"
    " 1 0 0 1 100 20 Tm\n"
    " (Hello World)Tj\n"
    " ET\n"
    " endstream\n"
    "endobj\n"
    "3 0 obj << /Type /Pages /Kids [ 1 0 R ] /Count 1 /MediaBox [ 0 0 300 50] "
    ">>\n"
    "endobj\n"
    "4 0 obj << /Type /Font /Subtype /Type1 /Name /F1 /BaseFont/Arial >>\n"
    "endobj\n"
    "5 0 obj << /ProcSet[/PDF/Text] /Font <</F1 4 0 R >> >>\n"
    "endobj\n"
    "6 0 obj << /Type /Catalog /Pages 3 0 R >>\n"
    "endobj\n"
    "trailer << /Root 6 0 R >>\n";

enum ExpectedNavigationStatus { NAVIGATION_BLOCKED, NAVIGATION_ALLOWED };

// A wrapper around WebContentsConsoleObserver that watches for a success or
// failure message. This will add a failure if an unexpected message is seen.
class BlockedURLWarningConsoleObserver {
 public:
  enum Status {
    NO_MESSAGE,
    SAW_SUCCESS_MESSAGE,
    SAW_FAILURE_MESSAGE,
  };
  BlockedURLWarningConsoleObserver(WebContents* web_contents,
                                   const std::string& success_filter,
                                   const std::string& fail_filter)
      : console_observer_(web_contents),
        success_filter_(success_filter),
        fail_filter_(fail_filter),
        status_(NO_MESSAGE) {}

  ~BlockedURLWarningConsoleObserver() = default;

  void Wait() {
    ASSERT_TRUE(console_observer_.Wait());
    ASSERT_EQ(1u, console_observer_.messages().size());
    std::string message = console_observer_.GetMessageAt(0u);
    if (base::MatchPattern(message, fail_filter_))
      status_ = SAW_FAILURE_MESSAGE;
    else if (base::MatchPattern(message, success_filter_))
      status_ = SAW_SUCCESS_MESSAGE;
    else
      ADD_FAILURE() << "Unexpected message: " << message;
  }

  Status status() const { return status_; }

 private:
  WebContentsConsoleObserver console_observer_;
  const std::string success_filter_;
  const std::string fail_filter_;
  Status status_;
};

#if BUILDFLAG(ENABLE_PLUGINS)
// Registers a fake PDF plugin handler so that navigations with a PDF
// mime type end up with a navigation and don't simply download the file.
void RegisterFakePlugin() {
  const char16_t kPluginName[] = u"PDF";
  const char kPdfMimeType[] = "application/pdf";
  const char kPdfFileType[] = "pdf";
  WebPluginInfo plugin_info;
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
  plugin_info.name = kPluginName;
  plugin_info.mime_types.emplace_back(kPdfMimeType, kPdfFileType,
                                      std::string());
  auto* plugin_service = PluginService::GetInstance();
  plugin_service->RegisterInternalPlugin(plugin_info, false);
  plugin_service->RefreshPlugins();
}

void UnregisterFakePlugin() {
  auto* plugin_service = PluginService::GetInstance();
  std::vector<WebPluginInfo> plugins;
  plugin_service->GetInternalPlugins(&plugins);
  EXPECT_EQ(1u, plugins.size());

  plugin_service->UnregisterInternalPlugin(plugins[0].path);
  plugin_service->RefreshPlugins();

  plugins.clear();
  plugin_service->GetInternalPlugins(&plugins);
  EXPECT_TRUE(plugins.empty());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace

class BlockedSchemeNavigationBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  BlockedSchemeNavigationBrowserTest() = default;

  BlockedSchemeNavigationBrowserTest(
      const BlockedSchemeNavigationBrowserTest&) = delete;
  BlockedSchemeNavigationBrowserTest& operator=(
      const BlockedSchemeNavigationBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
#if BUILDFLAG(ENABLE_PLUGINS)
    RegisterFakePlugin();
#endif

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("data_url_navigations.html");
    ASSERT_TRUE(base::PathExists(path));

    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(path, &contents));
    data_url_ = GURL(std::string("data:text/html,") + contents);

    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  void TearDownOnMainThread() override { UnregisterFakePlugin(); }
#endif

  void Navigate(const GURL& url) {
    content::DOMMessageQueue message_queue(shell()->web_contents());
    EXPECT_TRUE(NavigateToURL(shell(), url));
    std::string message;
    while (message_queue.WaitForMessage(&message)) {
      if (message == "\"READY\"")
        break;
    }
  }

  // Creates a filesystem: URL on the current origin.
  GURL CreateFileSystemUrl(const std::string& filename,
                           const std::string& content,
                           const std::string& mime_type) {
    const char kCreateFilesystemUrlScript[] =
        "var contents = `%s`;"
        "new Promise(resolve => {"
        "    webkitRequestFileSystem(window.TEMPORARY, 1024, fs => {"
        "    fs.root.getFile('%s', {create: true}, entry => {"
        "      entry.createWriter(w => {"
        "        w.write(new Blob([contents], {type: '%s'}));"
        "        w.onwrite = function(evt) {"
        "          resolve(entry.toURL());"
        "        }"
        "      });"
        "    });"
        "  });"
        "});";
    std::string filesystem_url_string =
        EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
               base::StringPrintf(kCreateFilesystemUrlScript, content.c_str(),
                                  filename.c_str(), mime_type.c_str()))
            .ExtractString();
    GURL filesystem_url(filesystem_url_string);
    EXPECT_TRUE(filesystem_url.is_valid());
    EXPECT_TRUE(filesystem_url.SchemeIsFileSystem());
    return filesystem_url;
  }

  bool IsDataURLTest() const {
    return std::string(url::kDataScheme) == GetParam();
  }

  GURL CreateEmptyURLWithBlockedScheme() {
    return CreateURLWithBlockedScheme("empty.html", "<html></html>",
                                      "text/html");
  }

  GURL CreateURLWithBlockedScheme(const std::string& filename,
                                  const std::string& content,
                                  const std::string& mimetype) {
    if (IsDataURLTest()) {
      return GURL(
          base::StringPrintf("data:%s,%s", mimetype.c_str(), content.c_str()));
    }
    // We need an origin to create a filesystem URL on, so navigate to one.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
    return CreateFileSystemUrl(filename, content, mimetype);
  }

  GURL GetTestURL() {
    return embedded_test_server()->GetURL(
        base::StringPrintf("/%s_url_navigations.html", GetParam()));
  }

  // Adds an iframe to |rfh| pointing to |url|.
  void AddIFrame(RenderFrameHost* rfh, const GURL& url) {
    content::DOMMessageQueue message_queue(
        WebContents::FromRenderFrameHost(rfh));
    const std::string javascript = base::StringPrintf(
        "f = document.createElement('iframe'); f.src = '%s';"
        "document.body.appendChild(f);",
        url.spec().c_str());
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(rfh, javascript));
    observer.Wait();
    std::string message;
    while (message_queue.WaitForMessage(&message)) {
      if (message == "\"READY\"")
        break;
    }
  }

  // Runs |javascript| on the first child frame and checks for a navigation.
  void TestNavigationFromFrame(
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckNavigation(shell(), child, scheme, javascript,
                                    expected_navigation_status);
  }

  // Runs |javascript| on the first child frame and expects a download to occur.
  void TestDownloadFromFrame(const std::string& javascript) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckDownload(child, javascript);
  }

  // Runs |javascript| on the first child frame and checks for a navigation to
  // the PDF file pointed by the test case.
  void TestPDFNavigationFromFrame(
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckPDFNavigation(child, scheme, javascript,
                                       expected_navigation_status);
  }

  // Same as TestNavigationFromFrame, but instead of navigating, the child frame
  // tries to open a new window with a blocked URL (data or filesystem)
  void TestWindowOpenFromFrame(
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckWindowOpen(child, scheme, javascript,
                                    expected_navigation_status);
  }

  // Executes |javascript| on |rfh| and waits for a console message based on
  // |expected_navigation_status|.
  // - Blocked navigations should print kDataUrlBlockedPattern.
  // - Allowed navigations should print kNavigationSuccessfulMessage.
  void ExecuteScriptAndCheckNavigation(
      Shell* shell,
      RenderFrameHost* rfh,
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    if (expected_navigation_status == NAVIGATION_ALLOWED)
      ExecuteScriptAndCheckNavigationAllowed(shell, rfh, javascript, scheme);
    else
      ExecuteScriptAndCheckNavigationBlocked(shell, rfh, javascript, scheme);
  }

 protected:
  // Similar to ExecuteScriptAndCheckNavigation(), but doesn't wait for a
  // console message if the navigation is expected to be allowed (this is
  // because PDF files can't print to the console).
  void ExecuteScriptAndCheckPDFNavigation(
      RenderFrameHost* rfh,
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());

    const std::string expected_message =
        (expected_navigation_status == NAVIGATION_ALLOWED)
            ? std::string()
            : base::StringPrintf(kNavigationBlockedMessage, scheme.c_str());

    std::optional<WebContentsConsoleObserver> console_observer;
    if (!expected_message.empty()) {
      console_observer.emplace(shell()->web_contents());
      console_observer->SetPattern(expected_message);
    }

    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(rfh, javascript));

    if (console_observer)
      ASSERT_TRUE(console_observer->Wait());

    switch (expected_navigation_status) {
      case NAVIGATION_ALLOWED:
        navigation_observer.Wait();
        // The new page should have the expected scheme.
        EXPECT_TRUE(
            shell()->web_contents()->GetLastCommittedURL().SchemeIs(scheme));
        EXPECT_TRUE(navigation_observer.last_navigation_url().SchemeIs(scheme));
        EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
        break;

      case NAVIGATION_BLOCKED:
        // Original page shouldn't navigate away.
        EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
        EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Executes |javascript| on |rfh| and waits for a new window to be opened.
  // Does not check for console messages (it's currently not possible to
  // concurrently wait for a new shell to be created and a console message to be
  // printed on that new shell).
  void ExecuteScriptAndCheckWindowOpen(
      RenderFrameHost* rfh,
      const std::string& scheme,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(rfh, javascript));

    Shell* new_shell = new_shell_observer.GetShell();
    WaitForLoadStop(new_shell->web_contents());

    switch (expected_navigation_status) {
      case NAVIGATION_ALLOWED:
        EXPECT_TRUE(
            new_shell->web_contents()->GetLastCommittedURL().SchemeIs(scheme));
        break;

      case NAVIGATION_BLOCKED:
        EXPECT_TRUE(
            new_shell->web_contents()->GetLastCommittedURL().is_empty());
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  // Executes |javascript| on |rfh| and waits for a download to be started by
  // a window.open call.
  void ExecuteScriptAndCheckWindowOpenDownload(RenderFrameHost* rfh,
                                               const std::string& javascript) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    ShellAddedObserver new_shell_observer;
    DownloadManager* download_manager =
        shell()->web_contents()->GetBrowserContext()->GetDownloadManager();

    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    EXPECT_TRUE(ExecJs(rfh, javascript));
    Shell* new_shell = new_shell_observer.GetShell();

    WaitForLoadStop(new_shell->web_contents());
    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    EXPECT_TRUE(
        new_shell->web_contents()->GetLastCommittedURL().spec().empty());
    // No navigation should commit.
    EXPECT_TRUE(new_shell->web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->IsInitialEntry());
    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Executes |javascript| on |rfh| and waits for a download to be started.
  void ExecuteScriptAndCheckDownload(RenderFrameHost* rfh,
                                     const std::string& javascript) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    DownloadManager* download_manager =
        shell()->web_contents()->GetBrowserContext()->GetDownloadManager();
    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    EXPECT_TRUE(ExecJs(rfh, javascript));
    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Initiates a browser initiated navigation to |url| and waits for a download
  // to be started.
  void NavigateAndCheckDownload(const GURL& url) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    DownloadManager* download_manager =
        shell()->web_contents()->GetBrowserContext()->GetDownloadManager();
    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    // Since this navigation will result in a download, there should be no
    // commit.
    EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(), url));

    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // data URL form of the file at content/test/data/data_url_navigations.html
  GURL data_url() const { return data_url_; }

  std::string GetNavigationBlockedMessage() const {
    return base::StringPrintf(kNavigationBlockedMessage, GetParam());
  }

 private:
  // Executes |javascript| on |rfh| and waits for a console message that
  // indicates the navigation has completed. |scheme| is the scheme being
  // tested.
  static void ExecuteScriptAndCheckNavigationAllowed(
      Shell* shell,
      RenderFrameHost* rfh,
      const std::string& javascript,
      const std::string& scheme) {
    // Should see success message, should never see blocked message.
    const std::string blocked_message =
        base::StringPrintf(kNavigationBlockedMessage, scheme.c_str());
    BlockedURLWarningConsoleObserver console_observer(
        shell->web_contents(), kNavigationSuccessfulMessage, blocked_message);

    TestNavigationObserver navigation_observer(shell->web_contents());
    EXPECT_TRUE(ExecJs(rfh, javascript));
    console_observer.Wait();
    EXPECT_EQ(BlockedURLWarningConsoleObserver::SAW_SUCCESS_MESSAGE,
              console_observer.status());
    navigation_observer.Wait();

    // The new page should have the expected scheme.
    EXPECT_EQ(navigation_observer.last_navigation_url(),
              shell->web_contents()->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Similar to ExecuteScriptAndCheckNavigationAllowed. Executes |javascript| on
  // |rfh| and waits for a console message that indicates the navigation has
  // been blocked. |scheme| is the scheme being tested.
  static void ExecuteScriptAndCheckNavigationBlocked(
      Shell* shell,
      RenderFrameHost* rfh,
      const std::string& javascript,
      const std::string& scheme) {
    const GURL original_url(shell->web_contents()->GetLastCommittedURL());

    // Should see blocked message, should never see success message.
    const std::string blocked_message =
        base::StringPrintf(kNavigationBlockedMessage, scheme.c_str());
    BlockedURLWarningConsoleObserver console_observer(
        shell->web_contents(), kNavigationSuccessfulMessage, blocked_message);

    TestNavigationObserver navigation_observer(shell->web_contents());
    EXPECT_TRUE(ExecJs(rfh, javascript));
    console_observer.Wait();
    EXPECT_EQ(BlockedURLWarningConsoleObserver::SAW_FAILURE_MESSAGE,
              console_observer.status());

    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell->web_contents()->GetLastCommittedURL());
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }

  base::ScopedTempDir downloads_directory_;

  GURL data_url_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BlockedSchemeNavigationBrowserTest,
                         ::testing::Values(url::kDataScheme,
                                           url::kFileSystemScheme));

////////////////////////////////////////////////////////////////////////////////
// Blocked schemes with HTML mimetype
//
// Tests that a browser initiated navigation to a blocked scheme doesn't show a
// console warning and is not blocked.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       BrowserInitiated_Allow) {
  const GURL kUrl(CreateURLWithBlockedScheme(
      "test.html",
      "<html><script>console.log('NAVIGATION_SUCCESSFUL');</script></html>",
      "text/html"));
  if (IsDataURLTest()) {
    BlockedURLWarningConsoleObserver console_observer(
        shell()->web_contents(), kNavigationSuccessfulMessage,
        GetNavigationBlockedMessage());

    EXPECT_TRUE(NavigateToURL(shell(), kUrl));
    console_observer.Wait();
    EXPECT_TRUE(
        shell()->web_contents()->GetLastCommittedURL().SchemeIs(GetParam()));

  } else {
    // Navigate to a.com and create a filesystem URL on it.
    // For filesystem: tests we create a new shell and navigate that shell to
    // the filesystem: URL created above. Navigating the a tab away from the
    // original page may clear all filesystem: URLs associated with that origin,
    // so we keep the origin around in the original shell.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell()->web_contents(), "window.open('about:blank');"));
    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    BlockedURLWarningConsoleObserver console_observer(
        new_shell->web_contents(), kNavigationSuccessfulMessage,
        GetNavigationBlockedMessage());
    EXPECT_TRUE(NavigateToURL(new_shell, kUrl));

    console_observer.Wait();
    EXPECT_TRUE(
        new_shell->web_contents()->GetLastCommittedURL().SchemeIs(GetParam()));
  }
}

// Tests that a content initiated navigation to a blocked scheme is blocked.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       HTML_Navigation_Block) {
  Navigate(GetTestURL());
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

class DataUrlNavigationBrowserTestWithFeatureFlag
    : public BlockedSchemeNavigationBrowserTest {
 public:
  DataUrlNavigationBrowserTestWithFeatureFlag() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAllowContentInitiatedDataUrlNavigations);
  }

  DataUrlNavigationBrowserTestWithFeatureFlag(
      const DataUrlNavigationBrowserTestWithFeatureFlag&) = delete;
  DataUrlNavigationBrowserTestWithFeatureFlag& operator=(
      const DataUrlNavigationBrowserTestWithFeatureFlag&) = delete;

  ~DataUrlNavigationBrowserTestWithFeatureFlag() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a content initiated navigation to a data URL is allowed if
// blocking is disabled with a feature flag.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTestWithFeatureFlag,
                       HTML_Navigation_Allow_FeatureFlag) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/data_url_navigations.html")));
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(), url::kDataScheme,
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_ALLOWED);
}

// Tests that a window.open to a blocked scheme with HTML mime type is blocked.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       HTML_WindowOpen_Block) {
  Navigate(GetTestURL());
  ExecuteScriptAndCheckWindowOpen(
      shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('window-open-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that a form post to a blocked scheme with HTML mime type is blocked.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       HTML_FormPost_Block) {
  Navigate(GetTestURL());
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('form-post-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that clicking <a download> link downloads the URL even with a blocked
// scheme.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest, HTML_Download) {
  Navigate(GetTestURL());
  ExecuteScriptAndCheckDownload(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "document.getElementById('download-link').click()");
}

// Tests that navigating the main frame to a blocked scheme with HTML mimetype
// from a subframe is blocked.
// TODO: crbug.com/40943572 - Fix and re-enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_HTML_NavigationFromFrame_Block \
  DISABLED_HTML_NavigationFromFrame_Block
#else
#define MAYBE_HTML_NavigationFromFrame_Block HTML_NavigationFromFrame_Block
#endif
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       MAYBE_HTML_NavigationFromFrame_Block) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(
      shell()->web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL(
          "b.com", base::StringPrintf("/%s_url_navigations.html", GetParam())));

  TestNavigationFromFrame(
      GetParam(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that opening a new window with a blocked scheme from a subframe is
// blocked.
// TODO: crbug.com/40943572 - Fix and re-enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_HTML_WindowOpenFromFrame_Block \
  DISABLED_HTML_WindowOpenFromFrame_Block
#else
#define MAYBE_HTML_WindowOpenFromFrame_Block HTML_WindowOpenFromFrame_Block
#endif
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       MAYBE_HTML_WindowOpenFromFrame_Block) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(
      shell()->web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL(
          "b.com", base::StringPrintf("/%s_url_navigations.html", GetParam())));

  TestWindowOpenFromFrame(GetParam(),
                          "document.getElementById('window-open-html').click()",
                          NAVIGATION_BLOCKED);
}

// Tests that navigation to a blocked scheme is blocked even if the top frame is
// already has a blocked scheme.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       HTML_Navigation_SameScheme_Block) {
  if (IsDataURLTest()) {
    EXPECT_TRUE(NavigateToURL(shell(), data_url()));
    ExecuteScriptAndCheckNavigation(
        shell(), shell()->web_contents()->GetPrimaryMainFrame(),
        url::kDataScheme,
        "document.getElementById('navigate-top-frame-to-html').click()",
        NAVIGATION_BLOCKED);
  } else {
    // We need an origin to create a filesystem URL on, so navigate to one.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
    const GURL kFilesystemURL1(
        CreateFileSystemUrl("empty1.html", "empty1", "text/html"));
    const GURL kFilesystemURL2(
        CreateFileSystemUrl("empty2.html", "empty2", "text/html"));

    // Create a new shell and navigate that shell to the filesystem: URL created
    // above. Navigating the a tab away from the
    // original page may clear all filesystem: URLs associated with that origin,
    // so we keep the origin around in the original shell.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell()->web_contents(), "window.open('about:blank');"));
    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_TRUE(NavigateToURL(new_shell, kFilesystemURL1));
    ExecuteScriptAndCheckNavigation(
        new_shell, new_shell->web_contents()->GetPrimaryMainFrame(),
        url::kFileSystemScheme,
        base::StringPrintf("window.location='%s';",
                           kFilesystemURL2.spec().c_str()),
        NAVIGATION_BLOCKED);
  }
}

// Tests that a form post to a blocked scheme with HTML mime type is blocked
// even if the top frame is already a blocked scheme.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       HTML_FormPost_SameScheme_Block) {
  if (IsDataURLTest()) {
    EXPECT_TRUE(NavigateToURL(shell(), data_url()));
    ExecuteScriptAndCheckNavigation(
        shell(), shell()->web_contents()->GetPrimaryMainFrame(),
        url::kDataScheme,
        "document.getElementById('form-post-to-html').click()",
        NAVIGATION_BLOCKED);
  } else {
    // We need an origin to create a filesystem URL on, so navigate to one.
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
    const GURL kFilesystemURL1(
        CreateFileSystemUrl("target.html", "form target", "text/html"));
    const GURL kFilesystemURL2(CreateFileSystemUrl(
        "form.html",
        base::StringPrintf("<html><form id=f method=post action='%s'><input "
                           "type=submit "
                           "onclick=document.getElementById('f').click() "
                           "id=btn-submit></form></html>",
                           kFilesystemURL1.spec().c_str()),
        "text/html"));

    // Create a new shell and navigate that shell to the filesystem: URL created
    // above. Navigating the a tab away from the
    // original page may clear all filesystem: URLs associated with that origin,
    // so we keep the origin around in the original shell.
    ShellAddedObserver new_shell_observer;
    // TODO(crbug.com/40090464): about:blank might commit without needing to
    // wait.
    //                     Remove the wait.
    EXPECT_TRUE(ExecJs(shell()->web_contents(), "window.open('about:blank');"));
    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    EXPECT_TRUE(NavigateToURL(new_shell, kFilesystemURL2));
    ExecuteScriptAndCheckNavigation(
        new_shell, new_shell->web_contents()->GetPrimaryMainFrame(),
        url::kFileSystemScheme, "document.getElementById('btn-submit').click()",
        NAVIGATION_BLOCKED);
  }
}

// Tests that navigating the top frame to a blocked scheme with HTML mimetype is
// blocked even if the top frame already has a blocked scheme.
// TODO: crbug.com/40943572 - Fix and re-enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_HTML_NavigationFromFrame_TopFrameHasBlockedScheme_Block \
  DISABLED_HTML_NavigationFromFrame_TopFrameHasBlockedScheme_Block
#else
#define MAYBE_HTML_NavigationFromFrame_TopFrameHasBlockedScheme_Block \
  HTML_NavigationFromFrame_TopFrameHasBlockedScheme_Block
#endif
IN_PROC_BROWSER_TEST_P(
    BlockedSchemeNavigationBrowserTest,
    MAYBE_HTML_NavigationFromFrame_TopFrameHasBlockedScheme_Block) {
  EXPECT_TRUE(NavigateToURL(shell(), CreateEmptyURLWithBlockedScheme()));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(), GetTestURL());

  TestNavigationFromFrame(
      GetParam(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that opening a new window with a blocked scheme with HTML mimetype is
// blocked even if the top frame already has a blocked scheme.
IN_PROC_BROWSER_TEST_P(
    BlockedSchemeNavigationBrowserTest,
    HTML_WindowOpenFromFrame_TopFrameHasBlockedScheme_Block) {
  // Create an empty URL with a blocked scheme, navigate to it, and add a frame.
  EXPECT_TRUE(NavigateToURL(shell(), CreateEmptyURLWithBlockedScheme()));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(), GetTestURL());

  TestWindowOpenFromFrame(GetParam(),
                          "document.getElementById('window-open-html').click()",
                          NAVIGATION_BLOCKED);
}

////////////////////////////////////////////////////////////////////////////////
// Blocked schemes with octet-stream mimetype (binary)
//
// Test direct navigations to a binary mime types.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       OctetStream_BrowserInitiated) {
  const GURL kUrl(CreateURLWithBlockedScheme("test.html", "test",
                                             "application/octet-stream"));

  if (IsDataURLTest()) {
    // Navigations to data URLs with unknown mime types should end up as
    // downloads.
    NavigateAndCheckDownload(kUrl);
  } else {
    // Navigations to filesystem URLs never end up as downloads.
    EXPECT_TRUE(NavigateToURL(shell(), kUrl));
    EXPECT_EQ(kUrl, shell()->web_contents()->GetLastCommittedURL());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on android: https://crbug.com/734563
#define MAYBE_DataUrl_OctetStream_WindowOpen \
  DISABLED_DataUrl_OctetStream_WindowOpen
#else
#define MAYBE_DataUrl_OctetStream_WindowOpen DataUrl_OctetStream_WindowOpen
#endif

// Test window.open to a data URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       MAYBE_DataUrl_OctetStream_WindowOpen) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/data_url_navigations.html")));
  // Navigations to data URLs with unknown mime types should end up as
  // downloads.
  ExecuteScriptAndCheckWindowOpenDownload(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "document.getElementById('window-open-octetstream').click()");
}

// Test window.open to a filesystem URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       FilesystemUrl_OctetStream_WindowOpen) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/filesystem_url_navigations.html")));
  // Navigations to filesystem URLs never end up as downloads.
  ExecuteScriptAndCheckWindowOpen(
      shell()->web_contents()->GetPrimaryMainFrame(), url::kFileSystemScheme,
      "document.getElementById('window-open-octetstream').click()",
      NAVIGATION_BLOCKED);
}

// Test navigation to a data URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_OctetStream_Navigation) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/data_url_navigations.html")));
  // Navigations to data URLs with unknown mime types should end up as
  // downloads.
  ExecuteScriptAndCheckDownload(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "document.getElementById('navigate-top-frame-to-octetstream').click()");
}

// Test navigation to a filesystem URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       FilesystemUrl_OctetStream_Navigation) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/filesystem_url_navigations.html")));
  // Navigations to filesystem URLs never end up as downloads.
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(),
      url::kFileSystemScheme,
      "document.getElementById('navigate-top-frame-to-octetstream').click()",
      NAVIGATION_BLOCKED);
}

// Test form post to a data URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_OctetStream_FormPost) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/data_url_navigations.html")));
  // Form posts to data URLs with unknown mime types should end up as
  // downloads.
  ExecuteScriptAndCheckDownload(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "document.getElementById('form-post-to-octetstream').click()");
}

// Test form post to a filesystem URL with binary mimetype.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       FilesystemUrl_OctetStream_FormPost) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/filesystem_url_navigations.html")));
  // Navigations to filesystem URLs never end up as downloads.
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(),
      url::kFileSystemScheme,
      "document.getElementById('form-post-to-octetstream').click()",
      NAVIGATION_BLOCKED);
}

// Tests navigation of the main frame to a data URL with a binary mimetype
// from a subframe. These navigations should end up as downloads.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_OctetStream_NavigationFromFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(
      shell()->web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));
  TestDownloadFromFrame(
      "document.getElementById('navigate-top-frame-to-octetstream').click()");
}

// Tests navigation of the main frame to a filesystem URL with a binary mimetype
// from a subframe. Navigations to filesystem URLs never end up as downloads.
// TODO(crbug.com/40943572): Enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_FilesystemUrl_OctetStream_NavigationFromFrame \
  DISABLED_FilesystemUrl_OctetStream_NavigationFromFrame
#else
#define MAYBE_FilesystemUrl_OctetStream_NavigationFromFrame \
  FilesystemUrl_OctetStream_NavigationFromFrame
#endif
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       MAYBE_FilesystemUrl_OctetStream_NavigationFromFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(),
            embedded_test_server()->GetURL("b.com",
                                           "/filesystem_url_navigations.html"));

  TestNavigationFromFrame(
      url::kFileSystemScheme,
      "document.getElementById('navigate-top-frame-to-octetstream').click()",
      NAVIGATION_BLOCKED);
}

////////////////////////////////////////////////////////////////////////////////
// URLs with unknown mimetype
//
// Test direct navigation to an unknown mime type.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       UnknownMimeType_BrowserInitiated_Download) {
  const GURL kUrl(
      CreateURLWithBlockedScheme("test.html", "test", "unknown/mimetype"));

  if (IsDataURLTest()) {
    // Navigations to data URLs with unknown mime types should end up as
    // downloads.
    NavigateAndCheckDownload(kUrl);
  } else {
    // Navigations to filesystem URLs never end up as downloads.
    EXPECT_TRUE(NavigateToURL(shell(), kUrl));
    EXPECT_EQ(kUrl, shell()->web_contents()->GetLastCommittedURL());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on android: https://crbug.com/734563
#define MAYBE_UnknownMimeType_WindowOpen DISABLED_UnknownMimeType_WindowOpen
#else
#define MAYBE_UnknownMimeType_WindowOpen UnknownMimeType_WindowOpen
#endif

// Test window.open to a blocked scheme with an unknown mime type.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       MAYBE_UnknownMimeType_WindowOpen) {
  Navigate(GetTestURL());
  if (IsDataURLTest()) {
    // Navigations to data URLs with unknown mime types should end up as
    // downloads.
    ExecuteScriptAndCheckWindowOpenDownload(
        shell()->web_contents()->GetPrimaryMainFrame(),
        "document.getElementById('window-open-unknown-mimetype').click()");
  } else {
    // Navigations to filesystem URLs never end up as downloads.
    ExecuteScriptAndCheckWindowOpen(
        shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
        "document.getElementById('window-open-unknown-mimetype').click()",
        NAVIGATION_BLOCKED);
  }
}

// Test navigation to a data URL with an unknown mime type.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_UnknownMimeType_Navigation) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/data_url_navigations.html")));
  // Navigations to data URLs with unknown mime types should end up as
  // downloads.
  ExecuteScriptAndCheckDownload(shell()->web_contents()->GetPrimaryMainFrame(),
                                "document.getElementById('navigate-top-frame-"
                                "to-unknown-mimetype').click()");
}

// Test navigation to a filesystem URL with an unknown mime type.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       FilesystemUrl_UnknownMimeType_Navigation) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/filesystem_url_navigations.html")));
  // Navigations to filesystem URLs never end up as downloads.
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(),
      url::kFileSystemScheme,
      "document.getElementById('navigate-top-frame-to-unknown-mimetype')."
      "click()",
      NAVIGATION_BLOCKED);
}

// Test form post to a data URL with an unknown mime type.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_UnknownMimeType_FormPost) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/data_url_navigations.html")));
  // Form posts to data URLs with unknown mime types should end up as
  // downloads.
  ExecuteScriptAndCheckDownload(
      shell()->web_contents()->GetPrimaryMainFrame(),
      "document.getElementById('form-post-to-unknown-mimetype').click()");
}

// Test form post to a filesystem URL with an unknown mime type.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       FilesystemUrl_UnknownMimeType_FormPost) {
  Navigate(embedded_test_server()->GetURL(
      base::StringPrintf("/filesystem_url_navigations.html")));
  // Navigations to filesystem URLs never end up as downloads.
  ExecuteScriptAndCheckNavigation(
      shell(), shell()->web_contents()->GetPrimaryMainFrame(),
      url::kFileSystemScheme,
      "document.getElementById('form-post-to-unknown-mimetype').click()",
      NAVIGATION_BLOCKED);
}

// Test navigation of the main frame to a data URL with an unknown mimetype from
// a subframe. These navigations should end up as downloads.
IN_PROC_BROWSER_TEST_F(BlockedSchemeNavigationBrowserTest,
                       DataUrl_UnknownMimeType_NavigationFromFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(
      shell()->web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

  TestDownloadFromFrame(
      "document.getElementById('navigate-top-frame-to-unknown-mimetype')."
      "click()");
}

// Test navigation of the main frame to a filesystem URL with an unknown
// mimetype from a subframe. Navigations to filesystem URLs don't end up as
// downloads.
// TODO(crbug.com/40943572): Enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_FilesystemUrl_UnknownMimeType_NavigationFromFrame \
  DISABLED_FilesystemUrl_UnknownMimeType_NavigationFromFrame
#else
#define MAYBE_FilesystemUrl_UnknownMimeType_NavigationFromFrame \
  FilesystemUrl_UnknownMimeType_NavigationFromFrame
#endif
IN_PROC_BROWSER_TEST_F(
    BlockedSchemeNavigationBrowserTest,
    MAYBE_FilesystemUrl_UnknownMimeType_NavigationFromFrame) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(),
            embedded_test_server()->GetURL("b.com",
                                           "/filesystem_url_navigations.html"));

  TestNavigationFromFrame(url::kFileSystemScheme,
                          "document.getElementById('navigate-top-frame-to-"
                          "unknown-mimetype').click()",
                          NAVIGATION_BLOCKED);
}

////////////////////////////////////////////////////////////////////////////////
// URLs with PDF mimetype
//
// Tests that a browser initiated navigation to a blocked scheme URL with PDF
// mime type is allowed, or initiates a download on Android.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_BrowserInitiatedNavigation_Allow) {
  std::string pdf_base64 = base::Base64Encode(kPDF);
  const GURL kPDFUrl(CreateURLWithBlockedScheme(
      "test.pdf", IsDataURLTest() ? pdf_base64 : kPDF, "application/pdf"));

#if BUILDFLAG(ENABLE_PLUGINS)
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), kPDFUrl));
  EXPECT_EQ(kPDFUrl, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_TRUE(
      shell()->web_contents()->GetLastCommittedURL().SchemeIs(GetParam()));
#else
  NavigateAndCheckDownload(kPDFUrl);
#endif
}

// Tests that a window.open to a blocked scheme is blocked if the URL has a
// mime type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_WindowOpen_Block) {
  Navigate(GetTestURL());

#if BUILDFLAG(ENABLE_PLUGINS)
  ExecuteScriptAndCheckWindowOpen(
      shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('window-open-pdf').click()", NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    ExecuteScriptAndCheckDownload(
        shell()->web_contents()->GetPrimaryMainFrame(),
        "document.getElementById('window-open-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    ExecuteScriptAndCheckWindowOpen(
        shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
        "document.getElementById('window-open-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Test that a navigation to a blocked scheme URL is blocked if the URL has a
// mime type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_Navigation_Block) {
  Navigate(GetTestURL());

#if BUILDFLAG(ENABLE_PLUGINS)
  ExecuteScriptAndCheckPDFNavigation(
      shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    ExecuteScriptAndCheckDownload(
        shell()->web_contents()->GetPrimaryMainFrame(),
        "document.getElementById('navigate-top-frame-to-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    ExecuteScriptAndCheckPDFNavigation(
        shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
        "document.getElementById('navigate-top-frame-to-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Test that a form post to a blocked scheme is blocked if the URL has a mime
// type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest, PDF_FormPost_Block) {
  Navigate(GetTestURL());

#if BUILDFLAG(ENABLE_PLUGINS)
  ExecuteScriptAndCheckPDFNavigation(
      shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
      "document.getElementById('form-post-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    ExecuteScriptAndCheckDownload(
        shell()->web_contents()->GetPrimaryMainFrame(),
        "document.getElementById('form-post-to-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    ExecuteScriptAndCheckPDFNavigation(
        shell()->web_contents()->GetPrimaryMainFrame(), GetParam(),
        "document.getElementById('form-post-to-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Tests that navigating the main frame to a blocked scheme with PDF mimetype
// from a subframe is blocked, or is downloaded on Android.
// TODO: crbug.com/40943572 - Fix and re-enable the flaky test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_PDF_NavigationFromFrame_Block \
  DISABLED_PDF_NavigationFromFrame_Block
#else
#define MAYBE_PDF_NavigationFromFrame_Block PDF_NavigationFromFrame_Block
#endif
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       MAYBE_PDF_NavigationFromFrame_Block) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(
      shell()->web_contents()->GetPrimaryMainFrame(),
      embedded_test_server()->GetURL(
          "b.com", base::StringPrintf("/%s_url_navigations.html", GetParam())));

#if BUILDFLAG(ENABLE_PLUGINS)
  TestPDFNavigationFromFrame(
      GetParam(),
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckDownload(
        child, "document.getElementById('navigate-top-frame-to-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    TestPDFNavigationFromFrame(
        GetParam(),
        "document.getElementById('navigate-top-frame-to-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Tests that opening a window with a blocked scheme with PDF mimetype from a
// subframe is blocked, or is downloaded on Android.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_WindowOpenFromFrame_Block) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(),
            embedded_test_server()->GetURL(
                base::StringPrintf("/%s_url_navigations.html", GetParam())));

#if BUILDFLAG(ENABLE_PLUGINS)
  TestWindowOpenFromFrame(GetParam(),
                          "document.getElementById('window-open-pdf').click()",
                          NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckDownload(
        child, "document.getElementById('window-open-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    TestWindowOpenFromFrame(
        GetParam(), "document.getElementById('window-open-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Tests that navigating the top frame to a blocked scheme with PDF mimetype
// from a subframe is blocked even if the top frame already has a blocked
// scheme.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_NavigationFromFrame_TopFrameHasBlockedScheme_Block) {
  EXPECT_TRUE(NavigateToURL(shell(), CreateEmptyURLWithBlockedScheme()));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(), GetTestURL());

#if BUILDFLAG(ENABLE_PLUGINS)
  TestPDFNavigationFromFrame(
      GetParam(),
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckDownload(
        child, "document.getElementById('navigate-top-frame-to-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated and
    // should be blocked.
    TestPDFNavigationFromFrame(
        GetParam(),
        "document.getElementById('navigate-top-frame-to-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Tests that opening a window with a blocked scheme with PDF mimetype from a
// subframe is blocked even if the top frame already has a blocked scheme.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       PDF_WindowOpenFromFrame_TopFrameHasBlockedScheme_Block) {
  EXPECT_TRUE(NavigateToURL(shell(), CreateEmptyURLWithBlockedScheme()));
  AddIFrame(shell()->web_contents()->GetPrimaryMainFrame(), GetTestURL());

#if BUILDFLAG(ENABLE_PLUGINS)
  TestWindowOpenFromFrame(GetParam(),
                          "document.getElementById('window-open-pdf').click()",
                          NAVIGATION_BLOCKED);
#else
  if (IsDataURLTest()) {
    // When PDF Viewer is not available, data URL PDFs are downloaded upon
    // navigation.
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckDownload(
        child, "document.getElementById('window-open-pdf').click()");
  } else {
    // When PDF Viewer is not available, filesystem PDF URLs are navigated to
    // and should be blocked.
    TestWindowOpenFromFrame(
        GetParam(), "document.getElementById('window-open-pdf').click()",
        NAVIGATION_BLOCKED);
  }
#endif
}

// Test case to verify that redirects to blocked schemes are properly
// disallowed, even when invoked through history navigations. See
// https://crbug.com/723796.
IN_PROC_BROWSER_TEST_P(BlockedSchemeNavigationBrowserTest,
                       WindowOpenRedirectAndBack) {
  Navigate(GetTestURL());

  // This test will need to navigate the newly opened window.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             "document.getElementById('window-open-redirect').click()"));
  Shell* new_shell = new_shell_observer.GetShell();
  NavigationController* controller =
      &new_shell->web_contents()->GetController();
  WaitForLoadStop(new_shell->web_contents());

  // The window.open() should have resulted in an error page. The blocked
  // URL should be in both the actual and the virtual URL.
  {
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kFileSystemScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
    EXPECT_EQ(entry->GetURL(), entry->GetVirtualURL());
  }

  // Navigate forward and then go back to ensure the navigation to data: or
  // filesystem: URL is blocked. Use a browser-initiated back navigation,
  // equivalent to user pressing the back button.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1, controller->GetLastCommittedEntryIndex());
  {
    TestNavigationObserver observer(new_shell->web_contents());
    controller->GoBack();
    observer.Wait();

    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kFileSystemScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
    EXPECT_EQ(entry->GetURL(), entry->GetVirtualURL());
  }

  // Do another new navigation, but then use JavaScript to navigate back,
  // equivalent to document executing JS.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1, controller->GetLastCommittedEntryIndex());
  {
    TestNavigationObserver observer(new_shell->web_contents());
    EXPECT_TRUE(ExecJs(new_shell, "history.go(-1)"));
    observer.Wait();

    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kFileSystemScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
    EXPECT_EQ(entry->GetURL(), entry->GetVirtualURL());
  }
}

class FilesystemUrlNavigationBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  FilesystemUrlNavigationBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatureState(
          blink::features::kFileSystemUrlNavigation, GetParam());
    }
  }

  FilesystemUrlNavigationBrowserTest(
      const FilesystemUrlNavigationBrowserTest&) = delete;
  FilesystemUrlNavigationBrowserTest& operator=(
      const FilesystemUrlNavigationBrowserTest&) = delete;

  ~FilesystemUrlNavigationBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that navigation to external mounted filesystem: URLs are blocked
// unless FileSystemUrlNavigation feature flag is enabled (b/291526810).
IN_PROC_BROWSER_TEST_P(FilesystemUrlNavigationBrowserTest, External) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir mount_point;
  ASSERT_TRUE(mount_point.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(mount_point.GetPath().AppendASCII("file.html"),
                      "<html><script>console.log('success')</script></html>"));
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "mount-name", storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), mount_point.GetPath());

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      GetParam() ? "success" : "Not allowed to navigate to filesystem URL:*");
  EXPECT_EQ(GetParam(),
            NavigateToURL(shell(), GURL("filesystem:http://remote/"
                                        "external/mount-name/file.html")));
  ASSERT_TRUE(console_observer.Wait());

  storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      "mount-name");
}

INSTANTIATE_TEST_SUITE_P(All,
                         FilesystemUrlNavigationBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool> info) {
                           return info.param ? "FlagOn" : "FlagOff";
                         });

}  // namespace content
