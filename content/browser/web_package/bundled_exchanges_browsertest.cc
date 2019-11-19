// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif  // OS_ANDROID

namespace content {
namespace {

// "%2F" is treated as an invalid character for file URLs.
constexpr char kInvalidFileUrl[] = "file:///tmp/test%2F/a.wbn";

constexpr char kTestPageUrl[] = "https://test.example.org/";
constexpr char kTestPage1Url[] = "https://test.example.org/page1.html";
constexpr char kTestPage2Url[] = "https://test.example.org/page2.html";
constexpr char kTestPageForHashUrl[] =
    "https://test.example.org/hash.html#hello";

base::FilePath GetTestDataPath(base::StringPiece file) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir
      .Append(base::FilePath(
          FILE_PATH_LITERAL("content/test/data/bundled_exchanges")))
      .AppendASCII(file);
}

#if defined(OS_ANDROID)
GURL CopyFileAndGetContentUri(const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath tmp_dir;
  CHECK(base::GetTempDir(&tmp_dir));
  // The directory name "bundled_exchanges" must be kept in sync with
  // content/shell/android/browsertests_apk/res/xml/file_paths.xml
  base::FilePath tmp_wbn_dir = tmp_dir.AppendASCII("bundled_exchanges");
  CHECK(base::CreateDirectoryAndGetError(tmp_wbn_dir, nullptr));
  base::FilePath tmp_dir_in_tmp_wbn_dir;
  CHECK(
      base::CreateTemporaryDirInDir(tmp_wbn_dir, "", &tmp_dir_in_tmp_wbn_dir));
  base::FilePath temp_file = tmp_dir_in_tmp_wbn_dir.Append(file.BaseName());
  CHECK(base::CopyFile(file, temp_file));
  return GURL(base::GetContentUriFromFilePath(temp_file).value());
}
#endif  // OS_ANDROID

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(DownloadManager* manager) : manager_(manager) {
    manager_->AddObserver(this);
  }
  ~DownloadObserver() override { manager_->RemoveObserver(this); }

  void WaitUntilDownloadCreated() { run_loop_.Run(); }
  const GURL& observed_url() const { return url_; }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    url_ = item->GetURL();
    run_loop_.Quit();
  }

 private:
  DownloadManager* manager_;
  base::RunLoop run_loop_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadObserver);
};

class BundledExchangesBrowserTestBase : public ContentBrowserTest {
 protected:
  BundledExchangesBrowserTestBase() = default;
  ~BundledExchangesBrowserTestBase() override = default;

  void NavigateToBundleAndWaitForReady(const GURL& test_data_url,
                                       const GURL& expected_commit_url) {
    base::string16 expected_title = base::ASCIIToUTF16("Ready");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell()->web_contents(), test_data_url,
                              expected_commit_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void RunTestScript(const std::string& script) {
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                              "loadScript('" + script + "');"));
    base::string16 ok = base::ASCIIToUTF16("OK");
    TitleWatcher title_watcher(shell()->web_contents(), ok);
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
    EXPECT_EQ(ok, title_watcher.WaitAndGetTitle());
  }

  void ExecuteScriptAndWaitForTitle(const std::string& script,
                                    const std::string& title) {
    base::string16 title16 = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), title16);
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

  void NavigateToURLAndWaitForTitle(const GURL& url, const std::string& title) {
    ExecuteScriptAndWaitForTitle(
        base::StringPrintf("location.href = '%s';", url.spec().c_str()), title);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesBrowserTestBase);
};

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool CanAcceptUntrustedExchangesIfNeeded() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserClient);
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  explicit FinishNavigationObserver(WebContents* contents,
                                    base::OnceClosure done_closure)
      : WebContentsObserver(contents), done_closure_(std::move(done_closure)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    error_code_ = navigation_handle->GetNetErrorCode();
    std::move(done_closure_).Run();
  }

  const base::Optional<net::Error>& error_code() const { return error_code_; }

 private:
  base::OnceClosure done_closure_;
  base::Optional<net::Error> error_code_;

  DISALLOW_COPY_AND_ASSIGN(FinishNavigationObserver);
};

ContentBrowserClient* MaybeSetBrowserClientForTesting(
    ContentBrowserClient* browser_client) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/864403): It seems that we call unsupported Android APIs on
  // KitKat when we set a ContentBrowserClient. Don't call such APIs and make
  // this test available on KitKat.
  int32_t major_version = 0, minor_version = 0, bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  if (major_version < 5)
    return nullptr;
#endif  // defined(OS_ANDROID)
  return SetBrowserClientForTesting(browser_client);
}

}  // namespace

class InvalidTrustableBundledExchangesFileUrlBrowserTest
    : public ContentBrowserTest {
 protected:
  InvalidTrustableBundledExchangesFileUrlBrowserTest() = default;
  ~InvalidTrustableBundledExchangesFileUrlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = MaybeSetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableBundledExchangesFileUrl,
                                    kInvalidFileUrl);
  }

  ContentBrowserClient* original_client_ = nullptr;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(InvalidTrustableBundledExchangesFileUrlBrowserTest);
};

IN_PROC_BROWSER_TEST_F(InvalidTrustableBundledExchangesFileUrlBrowserTest,
                       NoCrashOnNavigation) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), GURL(kInvalidFileUrl)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_URL, *finish_navigation_observer.error_code());
}

class BundledExchangesTrustableFileBrowserTestBase
    : public BundledExchangesBrowserTestBase {
 protected:
  BundledExchangesTrustableFileBrowserTestBase() = default;
  ~BundledExchangesTrustableFileBrowserTestBase() override = default;

  void SetUp() override { BundledExchangesBrowserTestBase::SetUp(); }

  void SetUpOnMainThread() override {
    BundledExchangesBrowserTestBase::SetUpOnMainThread();
    original_client_ = MaybeSetBrowserClientForTesting(&browser_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableBundledExchangesFileUrl,
                                    test_data_url().spec());
  }

  void TearDownOnMainThread() override {
    BundledExchangesBrowserTestBase::TearDownOnMainThread();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  const GURL& test_data_url() const { return test_data_url_; }

  ContentBrowserClient* original_client_ = nullptr;
  GURL test_data_url_;

 private:
  TestBrowserClient browser_client_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesTrustableFileBrowserTestBase);
};

enum class TestFilePathMode {
  kNormalFilePath,
#if defined(OS_ANDROID)
  kContentURI,
#endif  // OS_ANDROID
};

class BundledExchangesTrustableFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public BundledExchangesTrustableFileBrowserTestBase {
 protected:
  BundledExchangesTrustableFileBrowserTest() {
    if (GetParam() == TestFilePathMode::kNormalFilePath) {
      test_data_url_ = net::FilePathToFileURL(
          GetTestDataPath("bundled_exchanges_browsertest.wbn"));
      return;
    }
#if defined(OS_ANDROID)
    DCHECK_EQ(TestFilePathMode::kContentURI, GetParam());
    test_data_url_ = CopyFileAndGetContentUri(
        GetTestDataPath("bundled_exchanges_browsertest.wbn"));
#endif  // OS_ANDROID
  }
  ~BundledExchangesTrustableFileBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BundledExchangesTrustableFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest,
                       TrustableBundledExchangesFile) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest, RangeRequest) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  RunTestScript("test-range-request.js");
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest, Navigations) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  // Move to page 1.
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));
  // Move to page 2.
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));
  // Back to page 1.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Back to the initial page.
  ExecuteScriptAndWaitForTitle("history.back();", "Ready");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL(kTestPageUrl));

  // Move to page 1.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Reload.
  ExecuteScriptAndWaitForTitle("document.title = 'reset';", "reset");
  ExecuteScriptAndWaitForTitle("location.reload();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage1Url));

  // Move to page 2.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            GURL(kTestPage2Url));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesTrustableFileBrowserTest,
                       NavigationWithHash) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;
  NavigateToBundleAndWaitForReady(test_data_url(), GURL(kTestPageUrl));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
}

INSTANTIATE_TEST_SUITE_P(BundledExchangesTrustableFileBrowserTests,
                         BundledExchangesTrustableFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

class BundledExchangesTrustableFileNotFoundBrowserTest
    : public BundledExchangesTrustableFileBrowserTestBase {
 protected:
  BundledExchangesTrustableFileNotFoundBrowserTest() {
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    test_data_url_ =
        net::FilePathToFileURL(test_data_dir.AppendASCII("not_found"));
  }
  ~BundledExchangesTrustableFileNotFoundBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(BundledExchangesTrustableFileNotFoundBrowserTest,
                       NotFound) {
  // Don't run the test if we couldn't override BrowserClient. It happens only
  // on Android Kitkat or older systems.
  if (!original_client_)
    return;

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url()));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());
  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ("Failed to read metadata of Web Bundle file: FILE_ERROR_FAILED",
            console_delegate.message());
}

class BundledExchangesFileBrowserTest
    : public testing::WithParamInterface<TestFilePathMode>,
      public BundledExchangesBrowserTestBase {
 protected:
  BundledExchangesFileBrowserTest() = default;
  ~BundledExchangesFileBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundles}, {});
    BundledExchangesBrowserTestBase::SetUp();
  }

  GURL GetTestUrlForFile(base::FilePath file_path) const {
    switch (GetParam()) {
      case TestFilePathMode::kNormalFilePath:
        return net::FilePathToFileURL(file_path);
#if defined(OS_ANDROID)
      case TestFilePathMode::kContentURI:
        return CopyFileAndGetContentUri(file_path);
#endif  // OS_ANDROID
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesFileBrowserTest);
};

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, BasicNavigation) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, Navigations) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
  // Move to page 1.
  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage1Url)));
  // Move to page 2.
  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage2Url)));

  // Back to page 1.
  ExecuteScriptAndWaitForTitle("history.back();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage1Url)));
  // Back to the initial page.
  ExecuteScriptAndWaitForTitle("history.back();", "Ready");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPageUrl)));

  // Move to page 1.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage1Url)));

  // Reload.
  ExecuteScriptAndWaitForTitle("document.title = 'reset';", "reset");
  ExecuteScriptAndWaitForTitle("location.reload();", "Page 1");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage1Url)));

  // Move to page 2.
  ExecuteScriptAndWaitForTitle("history.forward();", "Page 2");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPage2Url)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, NavigationWithHash) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));
  NavigateToURLAndWaitForTitle(GURL(kTestPageForHashUrl), "#hello");
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                test_data_url, GURL(kTestPageForHashUrl)));
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest,
                       InvalidBundledExchangeFile) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("invalid_bundled_exchanges.wbn"));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ("Failed to read metadata of Web Bundle file: Wrong magic bytes.",
            console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest,
                       ResponseParseErrorInMainResource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_first_entry.wbn"));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(web_contents,
                                                      run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(web_contents, test_data_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
            *finish_navigation_observer.error_code());

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest,
                       ResponseParseErrorInSubresource) {
  const GURL test_data_url = GetTestUrlForFile(
      GetTestDataPath("broken_bundle_broken_script_entry.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));

  WebContents* web_contents = shell()->web_contents();
  ConsoleObserverDelegate console_delegate(web_contents, "*");
  web_contents->SetDelegate(&console_delegate);

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "load failed");

  if (console_delegate.messages().empty())
    console_delegate.Wait();

  EXPECT_FALSE(console_delegate.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_delegate.message());
}

IN_PROC_BROWSER_TEST_P(BundledExchangesFileBrowserTest, NoLocalFileScheme) {
  const GURL test_data_url =
      GetTestUrlForFile(GetTestDataPath("bundled_exchanges_browsertest.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
          test_data_url, GURL(kTestPageUrl)));

  auto expected_title = base::ASCIIToUTF16("load failed");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Local Script"));

  const GURL script_file_url =
      net::FilePathToFileURL(GetTestDataPath("local_script.js"));
  const std::string script = base::StringPrintf(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "%s";
    document.body.appendChild(script);)",
                                                script_file_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

INSTANTIATE_TEST_SUITE_P(BundledExchangesFileBrowserTest,
                         BundledExchangesFileBrowserTest,
                         testing::Values(TestFilePathMode::kNormalFilePath
#if defined(OS_ANDROID)
                                         ,
                                         TestFilePathMode::kContentURI
#endif  // OS_ANDROID
                                         ));

class BundledExchangesNetworkBrowserTest
    : public BundledExchangesBrowserTestBase {
 protected:
  // Keep consistent with NETWORK_TEST_PORT in generate-test-wbns.sh.
  static constexpr int kNetworkTestPort = 39600;

  BundledExchangesNetworkBrowserTest() = default;
  ~BundledExchangesNetworkBrowserTest() override = default;

  void SetUpOnMainThread() override {
    BundledExchangesBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundlesFromNetwork}, {});
    BundledExchangesBrowserTestBase::SetUp();
  }

  void RegisterRequestHandler(const std::string& relative_url,
                              const std::string& headers,
                              const std::string& contents) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const std::string& relative_url, const std::string& headers,
           const std::string& contents,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != relative_url)
            return nullptr;
          return std::make_unique<net::test_server::RawHttpResponse>(headers,
                                                                     contents);
        },
        relative_url, headers, contents));
  }

  std::string GetTestFile(const std::string& file_name) const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string contents;
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath test_path = src_dir.Append(
        FILE_PATH_LITERAL("content/test/data/bundled_exchanges"));
    CHECK(base::ReadFileToString(test_path.AppendASCII(file_name), &contents));
    return contents;
  }

  void TestNavigationFailure(const GURL& url,
                             const std::string& expected_console_error) {
    WebContents* web_contents = shell()->web_contents();
    ConsoleObserverDelegate console_delegate(web_contents, "*");
    web_contents->SetDelegate(&console_delegate);
    base::RunLoop run_loop;
    FinishNavigationObserver finish_navigation_observer(web_contents,
                                                        run_loop.QuitClosure());
    EXPECT_FALSE(NavigateToURL(web_contents, url));
    run_loop.Run();
    ASSERT_TRUE(finish_navigation_observer.error_code());
    EXPECT_EQ(net::ERR_INVALID_BUNDLED_EXCHANGES,
              *finish_navigation_observer.error_code());
    if (console_delegate.messages().empty())
      console_delegate.Wait();
    EXPECT_FALSE(console_delegate.messages().empty());
    EXPECT_EQ(expected_console_error, console_delegate.message());
  }

  static GURL GetTestUrl(const std::string& host) {
    return GURL(base::StringPrintf("http://%s:%d/bundled_exchanges/test.wbn",
                                   host.c_str(), kNetworkTestPort));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesNetworkBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, Simple) {
  const std::string test_bundle =
      GetTestFile("bundled_exchanges_browsertest_network.wbn");
  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  NavigateToBundleAndWaitForReady(
      GetTestUrl("localhost"),
      GURL(base::StringPrintf("http://localhost:%d/bundled_exchanges/network/",
                              kNetworkTestPort)));
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, Download) {
  const std::string test_bundle =
      GetTestFile("bundled_exchanges_browsertest_network.wbn");
  // Web Bundle file with attachment Content-Disposition must trigger download.
  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Disposition:attachment; filename=test.wbn\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  const GURL url = GetTestUrl("localhost");
  WebContents* web_contents = shell()->web_contents();
  std::unique_ptr<DownloadObserver> download_observer =
      std::make_unique<DownloadObserver>(BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext()));
  EXPECT_FALSE(NavigateToURL(web_contents, url));
  download_observer->WaitUntilDownloadCreated();
  EXPECT_EQ(url, download_observer->observed_url());
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, NoContentLength) {
  const std::string test_bundle =
      GetTestFile("bundled_exchanges_browsertest_network.wbn");
  RegisterRequestHandler("/bundled_exchanges/test.wbn",
                         "HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n",
                         test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "Web Bundle response must have valid Content-Length header.");
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, NonSecureUrl) {
  const std::string test_bundle =
      GetTestFile("bundled_exchanges_browsertest_network.wbn");
  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("example.com"),
      "Web Bundle response must be served from HTTPS or localhost HTTP.");
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, PrimaryURLNotFound) {
  const std::string test_bundle = GetTestFile(
      "bundled_exchanges_browsertest_network_primary_url_not_found.wbn");

  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "The primary URL resource is not found in the web bundle.");
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, OriginMismatch) {
  const std::string test_bundle =
      GetTestFile("bundled_exchanges_browsertest_network.wbn");
  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("127.0.0.1"),
      "The origin of primary URL doesn't match with the origin of the web "
      "bundle.");
}

IN_PROC_BROWSER_TEST_F(BundledExchangesNetworkBrowserTest, InvalidFile) {
  const std::string test_bundle = GetTestFile("invalid_bundled_exchanges.wbn");
  RegisterRequestHandler(
      "/bundled_exchanges/test.wbn",
      base::StringPrintf("HTTP/1.1 200 OK\n"
                         "Content-Type:application/webbundle\n"
                         "Content-Length: %" PRIuS "\n",
                         test_bundle.size()),
      test_bundle);
  ASSERT_TRUE(embedded_test_server()->Start(kNetworkTestPort));
  TestNavigationFailure(
      GetTestUrl("localhost"),
      "Failed to read metadata of Web Bundle file: Wrong magic bytes.");
}

}  // namespace content
