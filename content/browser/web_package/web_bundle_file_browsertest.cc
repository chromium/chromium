// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_package/web_bundle_browsertest_base.h"
#include "content/browser/web_package/web_bundle_utils.h"

namespace content {

class WebBundleFileBrowserTest
    : public testing::WithParamInterface<
          web_bundle_browsertest_utils::TestFilePathMode>,
      public web_bundle_browsertest_utils::WebBundleBrowserTestBase {
 public:
  WebBundleFileBrowserTest(const WebBundleFileBrowserTest&) = delete;
  WebBundleFileBrowserTest& operator=(const WebBundleFileBrowserTest&) = delete;

 protected:
  WebBundleFileBrowserTest() = default;
  ~WebBundleFileBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kWebBundles}, {});
    web_bundle_browsertest_utils::WebBundleBrowserTestBase::SetUp();
  }

  GURL GetTestUrlForFile(base::FilePath file_path) const {
    GURL content_uri;
    if (GetParam() ==
        web_bundle_browsertest_utils::TestFilePathMode::kNormalFilePath) {
      content_uri = net::FilePathToFileURL(file_path);
    } else {
#if BUILDFLAG(IS_ANDROID)
      DCHECK_EQ(web_bundle_browsertest_utils::TestFilePathMode::kContentURI,
                GetParam());
      web_bundle_browsertest_utils::CopyFileAndGetContentUri(
          file_path, &content_uri, nullptr /* new_file_path */);
#endif  // BUILDFLAG(IS_ANDROID)
    }
    return content_uri;
  }

  void RunSharedNavigationTest(
      void (*setup_func)(net::EmbeddedTestServer*, GURL*, std::string*),
      void (*run_test_func)(WebContents*,
                            const GURL&,
                            const GURL&,
                            base::RepeatingCallback<GURL(const GURL&)>)) {
    GURL url_origin;
    std::string web_bundle_content;
    (*setup_func)(embedded_test_server(), &url_origin, &web_bundle_content);

    base::FilePath file_path;
    CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
    const GURL test_data_url = GetTestUrlForFile(file_path);

    (*run_test_func)(
        shell()->web_contents(), test_data_url, url_origin,
        base::BindRepeating(&web_bundle_utils::GetSynthesizedUrlForWebBundle,
                            test_data_url));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, BasicNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpBasicNavigationTest,
      &web_bundle_browsertest_utils::RunBasicNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       BrowserInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpBrowserInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunBrowserInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       RendererInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(&web_bundle_browsertest_utils::
                              SetUpRendererInitiatedOutOfBundleNavigationTest,
                          &web_bundle_browsertest_utils::
                              RunRendererInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, SameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpSameDocumentNavigationTest,
      &web_bundle_browsertest_utils::RunSameDocumentNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       IframeParentInitiatedOutOfBundleNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::
          RunIframeParentInitiatedOutOfBundleNavigationTest);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeSameDocumentNavigation) {
  RunSharedNavigationTest(
      &web_bundle_browsertest_utils::SetUpIframeNavigationTest,
      &web_bundle_browsertest_utils::RunIframeSameDocumentNavigationTest);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidWebBundleFile DISABLED_InvalidWebBundleFile
#else
#define MAYBE_InvalidWebBundleFile InvalidWebBundleFile
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_InvalidWebBundleFile) {
  const GURL test_data_url = GetTestUrlForFile(
      web_bundle_browsertest_utils::GetTestDataPath("invalid_web_bundle.wbn"));

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url);

  EXPECT_EQ(
      "Failed to read metadata of Web Bundle file: Invalid bundle length.",
      console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidExchangeUrl DISABLED_InvalidExchangeUrl
#else
#define MAYBE_InvalidExchangeUrl InvalidExchangeUrl
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_InvalidExchangeUrl) {
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "foo_base_url_bundle_b2.wbn"));

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url);

  EXPECT_EQ(web_bundle_utils::kInvalidExchangeUrlErrorMessage, console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvalidPrimaryUrl DISABLED_InvalidPrimaryUrl
#else
#define MAYBE_InvalidPrimaryUrl InvalidPrimaryUrl
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_InvalidPrimaryUrl) {
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "foo_primary_url_bundle_b2.wbn"));

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url);

  EXPECT_EQ(web_bundle_utils::kInvalidPrimaryUrlErrorMessage, console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ResponseParseErrorInMainResource \
  DISABLED_ResponseParseErrorInMainResource
#else
#define MAYBE_ResponseParseErrorInMainResource ResponseParseErrorInMainResource
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       MAYBE_ResponseParseErrorInMainResource) {
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "broken_bundle_broken_first_entry_b2.wbn"));

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url);

  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest,
                       ResponseParseErrorInSubresource) {
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "broken_bundle_broken_script_entry_b2.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      web_bundle_utils::GetSynthesizedUrlForWebBundle(
          test_data_url, GURL(web_bundle_browsertest_utils::kTestPageUrl)));

  WebContents* web_contents = shell()->web_contents();
  WebContentsConsoleObserver console_observer(web_contents);

  ExecuteScriptAndWaitForTitle(R"(
    const script = document.createElement("script");
    script.onerror = () => { document.title = "load failed";};
    script.src = "script.js";
    document.body.appendChild(script);)",
                               "load failed");

  if (console_observer.messages().empty())
    ASSERT_TRUE(console_observer.Wait());

  ASSERT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Response headers map "
      "must have exactly one pseudo-header, :status.",
      base::UTF16ToUTF8(console_observer.messages()[0].message));
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, NoLocalFileScheme) {
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "web_bundle_browsertest_b2.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      web_bundle_utils::GetSynthesizedUrlForWebBundle(
          test_data_url, GURL(web_bundle_browsertest_utils::kTestPageUrl)));

  auto* expected_title = u"load failed";
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  title_watcher.AlsoWaitForTitle(u"Local Script");

  const GURL script_file_url = net::FilePathToFileURL(
      web_bundle_browsertest_utils::GetTestDataPath("local_script.js"));
  const std::string script = base::StringPrintf(R"(
      const script = document.createElement("script");
      script.onerror = () => { document.title = "load failed";};
      script.src = "%s";
      document.body.appendChild(script);)",
                                                script_file_url.spec().c_str());
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, DataDecoderRestart) {
  constexpr char kTestPage1Url[] = "https://test.example.org/page1.html";
  constexpr char kTestPage2Url[] = "https://test.example.org/page2.html";
  // The content of this file will be read as response body of any exchange.
  base::FilePath test_file_path =
      web_bundle_browsertest_utils::GetTestDataPath("mocked.wbn");
  web_bundle_browsertest_utils::MockParserFactory mock_factory(
      {GURL(web_bundle_browsertest_utils::kTestPageUrl), GURL(kTestPage1Url),
       GURL(kTestPage2Url)},
      test_file_path);
  const GURL test_data_url = GetTestUrlForFile(test_file_path);
  web_bundle_browsertest_utils::NavigateAndWaitForTitle(
      shell()->web_contents(), test_data_url,
      web_bundle_utils::GetSynthesizedUrlForWebBundle(
          test_data_url, GURL(web_bundle_browsertest_utils::kTestPageUrl)),
      web_bundle_browsertest_utils::kTestPageUrl);

  EXPECT_EQ(1, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage1Url), kTestPage1Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage1Url)));

  EXPECT_EQ(2, mock_factory.GetParserCreationCount());
  mock_factory.SimulateParserDisconnect();

  NavigateToURLAndWaitForTitle(GURL(kTestPage2Url), kTestPage2Url);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(),
            web_bundle_utils::GetSynthesizedUrlForWebBundle(
                test_data_url, GURL(kTestPage2Url)));

  EXPECT_EQ(3, mock_factory.GetParserCreationCount());
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_ParseMetadataCrash DISABLED_ParseMetadataCrash
#else
#define MAYBE_ParseMetadataCrash ParseMetadataCrash
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_ParseMetadataCrash) {
  base::FilePath test_file_path =
      web_bundle_browsertest_utils::GetTestDataPath("mocked.wbn");
  web_bundle_browsertest_utils::MockParserFactory mock_factory(
      {GURL(web_bundle_browsertest_utils::kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseMetadataCrash();

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(
          shell()->web_contents(), GetTestUrlForFile(test_file_path));

  EXPECT_EQ(
      "Failed to read metadata of Web Bundle file: Cannot connect to the "
      "remote parser service",
      console_message);
}

// TODO(https://crbug.com/1225178): flaky
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#define MAYBE_ParseResponseCrash DISABLED_ParseResponseCrash
#else
#define MAYBE_ParseResponseCrash ParseResponseCrash
#endif
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_ParseResponseCrash) {
  base::FilePath test_file_path =
      web_bundle_browsertest_utils::GetTestDataPath("mocked.wbn");
  web_bundle_browsertest_utils::MockParserFactory mock_factory(
      {GURL(web_bundle_browsertest_utils::kTestPageUrl)}, test_file_path);
  mock_factory.SimulateParseResponseCrash();

  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(
          shell()->web_contents(), GetTestUrlForFile(test_file_path));

  EXPECT_EQ(
      "Failed to read response header of Web Bundle file: Cannot connect to "
      "the remote parser service",
      console_message);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, IframeNavigationNoCrash) {
  // Regression test for crbug.com/1058721. There was a bug that navigation of
  // OOPIF's remote iframe in Web Bundle file cause crash.
  const GURL test_data_url =
      GetTestUrlForFile(web_bundle_browsertest_utils::GetTestDataPath(
          "web_bundle_browsertest_b2.wbn"));
  NavigateToBundleAndWaitForReady(
      test_data_url,
      web_bundle_utils::GetSynthesizedUrlForWebBundle(
          test_data_url, GURL(web_bundle_browsertest_utils::kTestPageUrl)));

  const std::string empty_page_path = "/web_bundle/empty_page.html";
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL empty_page_url = embedded_test_server()->GetURL(empty_page_path);

  ExecuteScriptAndWaitForTitle(
      base::StringPrintf(R"(
      (async function() {
        const empty_page_url = '%s';
        const iframe = document.createElement('iframe');
        const onload = () => {
          iframe.removeEventListener('load', onload);
          document.title = 'Iframe loaded';
        }
        iframe.addEventListener('load', onload);
        iframe.src = empty_page_url;
        document.body.appendChild(iframe);
      })();)",
                         empty_page_url.spec().c_str()),
      "Iframe loaded");

  ExecuteScriptAndWaitForTitle(R"(
      (async function() {
        const iframe = document.querySelector("iframe");
        const onload = () => {
          document.title = 'Iframe loaded again';
        }
        iframe.addEventListener('load', onload);
        iframe.src = iframe.src + '?';
      })();)",
                               "Iframe loaded again");
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, Iframe) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);

  base::FilePath file_path;
  CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
  const GURL test_data_url = GetTestUrlForFile(file_path);
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, primary_url_origin.Resolve("/top")));
  web_bundle_browsertest_utils::RunSubPageTest(
      shell()->web_contents(), primary_url_origin, third_party_origin,
      &web_bundle_browsertest_utils::AddIframeAndWaitForMessage,
      true /* support_third_party_wbn_page */);
}

IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, WindowOpen) {
  net::EmbeddedTestServer third_party_server;
  GURL primary_url_origin;
  GURL third_party_origin;
  std::string web_bundle_content;
  web_bundle_browsertest_utils::SetUpSubPageTest(
      embedded_test_server(), &third_party_server, &primary_url_origin,
      &third_party_origin, &web_bundle_content);

  base::FilePath file_path;
  CreateTemporaryWebBundleFile(web_bundle_content, &file_path);
  const GURL test_data_url = GetTestUrlForFile(file_path);
  NavigateToBundleAndWaitForReady(
      test_data_url, web_bundle_utils::GetSynthesizedUrlForWebBundle(
                         test_data_url, primary_url_origin.Resolve("/top")));
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
IN_PROC_BROWSER_TEST_P(WebBundleFileBrowserTest, MAYBE_NoPrimaryURLFound) {
  const GURL test_data_url = GetTestUrlForFile(
      web_bundle_browsertest_utils::GetTestDataPath("same_origin_b2.wbn"));
  std::string console_message = web_bundle_browsertest_utils::
      ExpectNavigationFailureAndReturnConsoleMessage(shell()->web_contents(),
                                                     test_data_url);

  EXPECT_EQ(web_bundle_utils::kNoPrimaryUrlErrorMessage, console_message);
}

INSTANTIATE_TEST_SUITE_P(WebBundleFileBrowserTest,
                         WebBundleFileBrowserTest,
                         TEST_FILE_PATH_MODE_PARAMS);
}  // namespace content
