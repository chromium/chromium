// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/web_package/web_bundle_browsertest_base.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

// "%2F" is treated as an invalid character for file URLs.
constexpr char kInvalidFileUrl[] = "file:///tmp/test%2F/a.wbn";

}  // namespace

class InvalidTrustableWebBundleFileUrlBrowserTest : public ContentBrowserTest {
 public:
  InvalidTrustableWebBundleFileUrlBrowserTest(
      const InvalidTrustableWebBundleFileUrlBrowserTest&) = delete;
  InvalidTrustableWebBundleFileUrlBrowserTest& operator=(
      const InvalidTrustableWebBundleFileUrlBrowserTest&) = delete;

 protected:
  InvalidTrustableWebBundleFileUrlBrowserTest() = default;
  ~InvalidTrustableWebBundleFileUrlBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    kInvalidFileUrl);
  }

 private:
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  web_bundle_browsertest_utils::TestBrowserClient browser_client_;
};

IN_PROC_BROWSER_TEST_F(InvalidTrustableWebBundleFileUrlBrowserTest,
                       NoCrashOnNavigation) {
  base::RunLoop run_loop;
  web_bundle_browsertest_utils::FinishNavigationObserver
      finish_navigation_observer(shell()->web_contents(),
                                 run_loop.QuitClosure());
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), GURL(kInvalidFileUrl)));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code());
  EXPECT_EQ(net::ERR_INVALID_URL, *finish_navigation_observer.error_code());
}

class WebBundleTrustableFileBrowserTest
    : public testing::WithParamInterface<
          web_bundle_browsertest_utils::TestFilePathMode>,
      public web_bundle_browsertest_utils::WebBundleBrowserTestBase {
 public:
  WebBundleTrustableFileBrowserTest(const WebBundleTrustableFileBrowserTest&) =
      delete;
  WebBundleTrustableFileBrowserTest& operator=(
      const WebBundleTrustableFileBrowserTest&) = delete;

 protected:
  WebBundleTrustableFileBrowserTest() = default;
  ~WebBundleTrustableFileBrowserTest() override = default;

  void SetUp() override {
    InitializeTestDataUrl();
    web_bundle_browsertest_utils::WebBundleBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    test_data_url().spec());
  }

  void WriteWebBundleFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_FALSE(contents.empty());
    ASSERT_TRUE(base::WriteFile(test_data_file_path_, contents.data(),
                                contents.size()) > 0);
  }

  void WriteCommonWebBundleFile() {
    std::string contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(
          base::ReadFileToString(web_bundle_browsertest_utils::GetTestDataPath(
                                     "web_bundle_browsertest_b2.wbn"),
                                 &contents));
    }
    WriteWebBundleFile(contents);
  }

  const GURL& test_data_url() const { return test_data_url_; }

  void RunSharedNavigationTest(
      void (*setup_func)(net::EmbeddedTestServer*, GURL*, std::string*),
      void (*run_test_func)(WebContents*,
                            const GURL&,
                            const GURL&,
                            base::RepeatingCallback<GURL(const GURL&)>)) {
    GURL url_origin;
    std::string web_bundle_content;
    (*setup_func)(embedded_test_server(), &url_origin, &web_bundle_content);
    WriteWebBundleFile(web_bundle_content);

    (*run_test_func)(shell()->web_contents(), test_data_url(), url_origin,
                     base::BindRepeating([](const GURL& url) { return url; }));
  }

 private:
  void InitializeTestDataUrl() {
    base::FilePath file_path;
    CreateTemporaryWebBundleFile("", &file_path);
    if (GetParam() ==
        web_bundle_browsertest_utils::TestFilePathMode::kNormalFilePath) {
      test_data_file_path_ = file_path;
      test_data_url_ = net::FilePathToFileURL(file_path);
      return;
    }
#if BUILDFLAG(IS_ANDROID)
    DCHECK_EQ(web_bundle_browsertest_utils::TestFilePathMode::kContentURI,
              GetParam());
    web_bundle_browsertest_utils::CopyFileAndGetContentUri(
        file_path, &test_data_url_, &test_data_file_path_);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  GURL test_data_url_;
  base::FilePath test_data_file_path_;
};

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       TrustableWebBundleFile) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(
      test_data_url(), GURL(web_bundle_browsertest_utils::kTestPageUrl));
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, RangeRequest) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(
      test_data_url(), GURL(web_bundle_browsertest_utils::kTestPageUrl));
  RunTestScript("test-range-request.js");
}

// Flaky on Linux bots https://crbug.com/1406600.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_BasicNavigation DISABLED_BasicNavigation
#else
#define MAYBE_BasicNavigation BasicNavigation
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_BasicNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpBasicNavigationTest,
      &web_bundle_browsertest_utils::RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunBrowserInitiatedOutOfBundleNavigationTest);
}

// Flaky on Linux bots https://crbug.com/1406600.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_RendererInitiatedOutOfBundleNavigation \
  DISABLED_RendererInitiatedOutOfBundleNavigation
#else
#define MAYBE_RendererInitiatedOutOfBundleNavigation \
  RendererInitiatedOutOfBundleNavigation
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunRendererInitiatedOutOfBundleNavigationTest);
}

// Flaky on Linux bots https://crbug.com/1406600.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SameDocumentNavigation DISABLED_SameDocumentNavigation
#else
#define MAYBE_SameDocumentNavigation SameDocumentNavigation
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_SameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpSameDocumentNavigationTest,
      &web_bundle_browsertest_utils::RunSameDocumentNavigationTest);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_IframeNavigation DISABLED_IframeNavigation
#else
#define MAYBE_IframeNavigation IframeNavigation
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_IframeNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::
          RunIframeParentInitiatedOutOfBundleNavigationTest);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_IframeSameDocumentNavigation DISABLED_IframeSameDocumentNavigation
#else
#define MAYBE_IframeSameDocumentNavigation IframeSameDocumentNavigation
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_IframeSameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, BaseURI) {
  WriteCommonWebBundleFile();
  NavigateToBundleAndWaitForReady(
      test_data_url(), GURL(web_bundle_browsertest_utils::kTestPageUrl));
  EXPECT_EQ(web_bundle_browsertest_utils::ExecuteAndGetString(
                shell()->web_contents(), "(new Request('./foo/bar')).url"),
            "https://test.example.org/foo/bar");
  EXPECT_EQ(web_bundle_browsertest_utils::ExecuteAndGetString(
                shell()->web_contents(), R"(
            (() => {
              const base_element = document.createElement('base');
              base_element.href = 'https://example.org/piyo/';
              document.body.appendChild(base_element);
              return document.baseURI;
            })()
            )"),
            "https://example.org/piyo/");
  EXPECT_EQ(web_bundle_browsertest_utils::ExecuteAndGetString(
                shell()->web_contents(), "(new Request('./foo/bar')).url"),
            "https://example.org/piyo/foo/bar");
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, Iframe) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);
  WriteWebBundleFile(web_bundle_content);

  NavigateToBundleAndWaitForReady(test_data_url(),
                                  primary_url_origin.Resolve("/top"));
  web_bundle_browsertest_utils::RunSubPageTest(
      shell()->web_contents(), primary_url_origin, third_party_origin,
      &web_bundle_browsertest_utils::AddIframeAndWaitForMessage,
      true /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest, WindowOpen) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);
  WriteWebBundleFile(web_bundle_content);

  NavigateToBundleAndWaitForReady(test_data_url(),
                                  primary_url_origin.Resolve("/top"));
  web_bundle_browsertest_utils::RunSubPageTest(
      shell()->web_contents(), primary_url_origin, third_party_origin,
      &web_bundle_browsertest_utils::WindowOpenAndWaitForMessage,
      true /* support_third_party_wbn_page */);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NoPrimaryURLFound DISABLED_NoPrimaryURLFound
#else
#define MAYBE_NoPrimaryURLFound NoPrimaryURLFound
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_NoPrimaryURLFound) {
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        web_bundle_browsertest_utils::GetTestDataPath("same_origin_b2.wbn"),
        &contents));
  }
  WriteWebBundleFile(contents);

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url());

  EXPECT_EQ(web_bundle_utils::kNoPrimaryUrlErrorMessage, console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidExchangeUrl DISABLED_InvalidExchangeUrl
#else
#define MAYBE_InvalidExchangeUrl InvalidExchangeUrl
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_InvalidExchangeUrl) {
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(
        base::ReadFileToString(web_bundle_browsertest_utils::GetTestDataPath(
                                   "foo_base_url_bundle_b2.wbn"),
                               &contents));
  }
  WriteWebBundleFile(contents);

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url());

  EXPECT_EQ(web_bundle_utils::kInvalidExchangeUrlErrorMessage, console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidPrimaryUrl DISABLED_InvalidPrimaryUrl
#else
#define MAYBE_InvalidPrimaryUrl InvalidPrimaryUrl
#endif
IN_PROC_BROWSER_TEST_P(WebBundleTrustableFileBrowserTest,
                       MAYBE_InvalidPrimaryUrl) {
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(
        base::ReadFileToString(web_bundle_browsertest_utils::GetTestDataPath(
                                   "foo_primary_url_bundle_b2.wbn"),
                               &contents));
  }
  WriteWebBundleFile(contents);

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url());

  EXPECT_EQ(web_bundle_utils::kInvalidPrimaryUrlErrorMessage, console_message);
}

INSTANTIATE_TEST_SUITE_P(WebBundleTrustableFileBrowserTest,
                         WebBundleTrustableFileBrowserTest,
                         TEST_FILE_PATH_MODE_PARAMS);

class WebBundleTrustableFileNotFoundBrowserTest
    : public web_bundle_browsertest_utils::WebBundleBrowserTestBase {
 public:
  WebBundleTrustableFileNotFoundBrowserTest(
      const WebBundleTrustableFileNotFoundBrowserTest&) = delete;
  WebBundleTrustableFileNotFoundBrowserTest& operator=(
      const WebBundleTrustableFileNotFoundBrowserTest&) = delete;

 protected:
  WebBundleTrustableFileNotFoundBrowserTest() = default;
  ~WebBundleTrustableFileNotFoundBrowserTest() override = default;

  void SetUp() override {
    test_data_url_ = net::FilePathToFileURL(
        web_bundle_browsertest_utils::GetTestDataPath("not_found"));
    web_bundle_browsertest_utils::WebBundleBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTrustableWebBundleFileUrl,
                                    test_data_url().spec());
  }
  const GURL& test_data_url() const { return test_data_url_; }

 private:
  GURL test_data_url_;
};

// TODO(https://crbug.com/1227439): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NotFound DISABLED_NotFound
#else
#define MAYBE_NotFound NotFound
#endif
IN_PROC_BROWSER_TEST_F(WebBundleTrustableFileNotFoundBrowserTest,
                       MAYBE_NotFound) {
  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url());

  EXPECT_EQ("Failed to read metadata of Web Bundle file: FILE_ERROR_NOT_FOUND",
            console_message);
}
}  // namespace content
