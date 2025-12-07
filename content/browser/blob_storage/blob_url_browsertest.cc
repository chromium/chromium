// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/pattern.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/blob/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
class MockContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  MockContentBrowserClient() = default;
  ~MockContentBrowserClient() override = default;

  MOCK_METHOD(void,
              LogWebFeatureForCurrentPage,
              (content::RenderFrameHost*, blink::mojom::WebFeature),
              (override));

  bool IsFullCookieAccessAllowed(
      content::BrowserContext* browser_context,
      content::WebContents* web_contents,
      const GURL& url,
      const blink::StorageKey& storage_key,
      net::CookieSettingOverrides overrides) override {
    return allow_cookie_access_;
  }

  bool allow_cookie_access_ = false;
};
}  // namespace

// Tests of the blob: URL scheme.
class BlobUrlBrowserTest : public ContentBrowserTest {
 public:
  BlobUrlBrowserTest() = default;
  BlobUrlBrowserTest(const BlobUrlBrowserTest&) = delete;
  BlobUrlBrowserTest& operator=(const BlobUrlBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    client_ = std::make_unique<MockContentBrowserClient>();

    SetupCrossSiteRedirector(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  MockContentBrowserClient& GetMockClient() { return *client_; }

  void TearDownOnMainThread() override { client_.reset(); }

 private:
  std::unique_ptr<MockContentBrowserClient> client_;
};

IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest, LinkToUniqueOriginBlob) {
  // Use a data URL to obtain a test page in a unique origin. The page
  // contains a link to a "blob:null/SOME-GUID-STRING" URL.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("data:text/html,<body><script>"
           "var link = document.body.appendChild(document.createElement('a'));"
           "link.innerText = 'Click Me!';"
           "link.href = URL.createObjectURL(new Blob(['potato']));"
           "link.target = '_blank';"
           "link.id = 'click_me';"
           "</script></body>")));

  // Click the link.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell(), "document.getElementById('click_me').click()"));

  // The link should create a new tab.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  EXPECT_TRUE(
      base::MatchPattern(new_contents->GetVisibleURL().spec(), "blob:null/*"));
  EXPECT_EQ(
      "null potato",
      EvalJs(new_contents, "self.origin + ' ' + document.body.innerText;"));
}

IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest, LinkToSameOriginBlob) {
  // Using an http page, click a link that opens a popup to a same-origin blob.
  GURL url = embedded_test_server()->GetURL("chromium.org", "/title1.html");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(
      shell(),
      "var link = document.body.appendChild(document.createElement('a'));"
      "link.innerText = 'Click Me!';"
      "link.href = URL.createObjectURL(new Blob(['potato']));"
      "link.target = '_blank';"
      "link.click()"));

  // The link should create a new tab.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  EXPECT_TRUE(base::MatchPattern(new_contents->GetVisibleURL().spec(),
                                 "blob:" + origin.Serialize() + "/*"));
  EXPECT_EQ(
      origin.Serialize() + " potato",
      EvalJs(new_contents, "    self.origin + ' ' + document.body.innerText;"));
}

// Regression test for https://crbug.com/646278
IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest, LinkToSameOriginBlobWithAuthority) {
  // Using an http page, click a link that opens a popup to a same-origin blob
  // that has a spoofy authority section applied. This should be blocked.
  GURL url = embedded_test_server()->GetURL("chromium.org", "/title1.html");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(
      shell(),
      "var link = document.body.appendChild(document.createElement('a'));"
      "link.innerText = 'Click Me!';"
      "link.href = 'blob:http://spoof.com@' + "
      "    URL.createObjectURL(new Blob(['potato'])).split('://')[1];"
      "link.rel = 'opener'; link.target = '_blank';"
      "link.click()"));

  // The link should create a new tab.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // The spoofy URL should not be shown to the user.
  EXPECT_FALSE(
      base::MatchPattern(new_contents->GetVisibleURL().spec(), "*spoof*"));
  // The currently implemented behavior is that the URL gets rewritten to
  // about:blank#blocked.
  EXPECT_EQ(kBlockedURL, new_contents->GetVisibleURL().spec());
  EXPECT_EQ(
      origin.Serialize() + " ",
      EvalJs(new_contents,
             "self.origin + ' ' + document.body.innerText;"));  // no potato
}

// Regression test for https://crbug.com/646278
IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest, ReplaceStateToAddAuthorityToBlob) {
  // history.replaceState from a validly loaded blob URL shouldn't allow adding
  // an authority to the inner URL, which would be spoofy.
  GURL url = embedded_test_server()->GetURL("chromium.org", "/title1.html");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell(),
                     "args = ['<body>potato</body>'];\n"
                     "b = new Blob(args, {type: 'text/html'});"
                     "window.open(URL.createObjectURL(b));"));

  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  const GURL non_spoofy_blob_url = new_contents->GetLastCommittedURL();

  // Now try to URL spoof by embedding an authority to the inner URL using
  // `replaceState()` to perform a same-document navigation.
  EXPECT_FALSE(
      ExecJs(new_contents,
             "let host_port = self.origin.split('://')[1];\n"
             "let spoof_url = 'blob:http://spoof.com@' + host_port + '/abcd';\n"
             "window.history.replaceState({}, '', spoof_url);\n"));

  // The spoofy URL should not be shown to the user.
  EXPECT_FALSE(
      base::MatchPattern(new_contents->GetVisibleURL().spec(), "*spoof*"));

  // The currently implemented behavior is a same-document navigation to a
  // blocked URL gets rewritten to the current document's URL, i.e.
  // `non_spoofy_blob_url`.
  // The content of the page stays the same.
  EXPECT_EQ(non_spoofy_blob_url, new_contents->GetVisibleURL());
  EXPECT_EQ(origin.Serialize(), EvalJs(new_contents, "origin"));
  EXPECT_EQ("potato", EvalJs(new_contents, "document.body.innerText"));

  std::string window_location =
      EvalJs(new_contents, "window.location.href;").ExtractString();
  EXPECT_FALSE(base::MatchPattern(window_location, "*spoof*"));
}

IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest,
                       TestUseCounterForCrossPartitionSameOriginBlobURLFetch) {
  GURL main_url = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* rfh_c = shell()->web_contents()->GetPrimaryMainFrame();

  std::string blob_url_string =
      EvalJs(
          rfh_c,
          "const blob_url = URL.createObjectURL(new "
          "Blob(['<!doctype html><body>potato</body>'], {type: 'text/html'}));"
          "blob_url;")
          .ExtractString();
  GURL blob_url(blob_url_string);

  RenderFrameHost* rfh_b = ChildFrameAt(rfh_c, 0);
  RenderFrameHost* rfh_c_2 = ChildFrameAt(rfh_b, 0);

  EXPECT_CALL(
      GetMockClient(),
      LogWebFeatureForCurrentPage(
          rfh_c,
          blink::mojom::WebFeature::kCrossPartitionSameOriginBlobURLFetch))
      .Times(0);

  EXPECT_CALL(
      GetMockClient(),
      LogWebFeatureForCurrentPage(
          rfh_b,
          blink::mojom::WebFeature::kCrossPartitionSameOriginBlobURLFetch))
      .Times(0);

  EXPECT_CALL(
      GetMockClient(),
      LogWebFeatureForCurrentPage(
          rfh_c_2,
          blink::mojom::WebFeature::kCrossPartitionSameOriginBlobURLFetch))
      .Times(1);

  std::string fetch_blob_url_js = JsReplace(
      "fetch($1).then("
      "  () => true,"
      "  () => false);",
      blob_url);

  EXPECT_EQ(true, EvalJs(rfh_c, fetch_blob_url_js));
  EXPECT_EQ(false, EvalJs(rfh_b, fetch_blob_url_js));
  EXPECT_EQ(false, EvalJs(rfh_c_2, fetch_blob_url_js));

  EXPECT_TRUE(ExecJs(rfh_c, JsReplace("URL.revokeObjectURL($1)", blob_url)));
}

IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest, TestBlobFetchRequestError) {
  base::HistogramTester histogram_tester;
  GURL url = embedded_test_server()->GetURL("chromium.org", "/title1.html");
  url::Origin origin = url::Origin::Create(url);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The data should not be accessible after being revoked.
  EXPECT_EQ("TypeError",
            EvalJs(shell(),
                   "async function test() {"
                   "let error;"
                   "const url = URL.createObjectURL(new Blob(['potato']));"
                   "URL.revokeObjectURL(url);"
                   "try { await fetch(url); } catch (e) { error = e };"
                   "return new Promise(resolve => { resolve(error.name); });"
                   "}"
                   "test();"));
  FetchHistogramsFromChildProcesses();
  // The blob error should be recorded in UMA.
  histogram_tester.ExpectUniqueSample("Net.BlobFetch.ResponseNetErrorCode",
                                      -net::Error::ERR_FILE_NOT_FOUND, 1u);
}

// Regression test for crbug.com/426787402, where navigations to blob URLs with
// a media mime type also result in a resource load for the corresponding blob
// URL.
IN_PROC_BROWSER_TEST_F(BlobUrlBrowserTest,
                       NoPartitioningForMediaBlobUrlNavigations) {
  GURL main_url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c)");
  WebContents* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(web_contents, main_url));

  RenderFrameHost* rfh_b = web_contents->GetPrimaryMainFrame();
  RenderFrameHost* rfh_c_in_b = ChildFrameAt(rfh_b, 0);

  Shell* new_shell;
  {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(
        ExecJs(rfh_c_in_b,
               "var blob_url;"
               "var data_url = 'data:audio/wav;base64,"
               "UklGRnQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YVAAA"
               "ACAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAkI"
               "CAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgA==';"
               "fetch(data_url).then(async (res) => {"
               "  const blob = await res.blob();"
               "  blob_url = URL.createObjectURL(blob);"
               "  window.open(blob_url);"
               "});"));

    new_shell = new_shell_observer.GetShell();
    WebContents* new_contents = new_shell->web_contents();
    EXPECT_TRUE(WaitForLoadStop(new_contents));
  }

  static constexpr char check_video_element_status_js[] =
      "function check_video_element_status() {"
      "  const video = document.getElementsByTagName('video')[0];"
      "  if (video.readyState === 4) {"
      "    return video.readyState;"
      "  }"
      "  return new Promise(resolve => {"
      "    video.addEventListener('canplaythrough', () => {"
      "      resolve(video.readyState);"
      "    });"
      "  });"
      "}"
      "new Promise(resolve => {"
      "  if (document.readyState === 'complete') {"
      "    resolve(check_video_element_status());"
      "  } else {"
      "    window.addEventListener('load', () => {"
      "      resolve(check_video_element_status());"
      "    });"
      "  }"
      "});";

  int ready_state =
      EvalJs(new_shell, check_video_element_status_js).ExtractInt();
  // From local testing the HTMLMediaElement.readyState property returned 0
  // (HTMLMediaElement.HAVE_NOTHING) when partitioning blocked the resource load
  // and otherwise returned 4 (HTMLMediaElement.HAVE_ENOUGH_DATA). It's possible
  // that some intermediate states might be reached before the readyState is 4,
  // so our test code will wait for that. This means that if the bug is present
  // the call above will timeout, but otherwise readyState should equal 4 here.
  EXPECT_EQ(ready_state, 4);

  // This should also work if a site appends a fragment identifier to the blob
  // URL for some reason.
  {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(rfh_c_in_b, "window.open(blob_url + '#foo');"));

    new_shell = new_shell_observer.GetShell();
    WebContents* new_contents = new_shell->web_contents();
    EXPECT_TRUE(WaitForLoadStop(new_contents));
  }

  ready_state = EvalJs(new_shell, check_video_element_status_js).ExtractInt();
  // See comment above for why we check that `ready_state` is 4 here.
  EXPECT_EQ(ready_state, 4);
}

// Regression test for the issue described in
// https://crbug.com/399308041#comment7 where blob URL partitioning was bypassed
// for all contexts when third-party cookies were enabled.
IN_PROC_BROWSER_TEST_F(
    BlobUrlBrowserTest,
    BlobUrlPartitioningNotAlwaysBypassedWithThirdPartyCookieEnabled) {
  GetMockClient().allow_cookie_access_ = true;
  GURL main_url = embedded_https_test_server().GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* rfh_c = shell()->web_contents()->GetPrimaryMainFrame();

  std::string blob_url_string =
      EvalJs(
          rfh_c,
          "const blob_url = URL.createObjectURL(new "
          "Blob(['<!doctype html><body>potato</body>'], {type: 'text/html'}));"
          "blob_url;")
          .ExtractString();
  GURL blob_url(blob_url_string);

  RenderFrameHost* rfh_b = ChildFrameAt(rfh_c, 0);
  RenderFrameHost* rfh_c_2 = ChildFrameAt(rfh_b, 0);

  std::string fetch_blob_url_js = JsReplace(
      "fetch($1).then("
      "  () => true,"
      "  () => false);",
      blob_url);

  EXPECT_EQ(true, EvalJs(rfh_c, fetch_blob_url_js));

  // This access shouldn't succeed even though third-party cookies are enabled.
  EXPECT_EQ(false, EvalJs(rfh_c_2, fetch_blob_url_js));

  // Note: the SAA spec carves out an auto-resolve case when the requesting and
  // embedding origins are same-site: step 16.7 of
  // https://privacycg.github.io/storage-access/#dom-document-requeststorageaccess.
  // However, Chrome implements that in //chrome
  // (`StorageAccessGrantPermissionContext::DecidePermission`), so //content
  // can't rely on it. Thus, we must manually grant the permission here.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> future;
  static_cast<PermissionControllerImpl*>(
      rfh_c_2->GetBrowserContext()->GetPermissionController())
      ->SetPermissionOverride(
          /*requesting_origin=*/url::Origin::Create(main_url),
          /*embedding_origin=*/url::Origin::Create(main_url),
          blink::PermissionType::STORAGE_ACCESS_GRANT,
          blink::mojom::PermissionStatus::GRANTED, future.GetCallback());
  ASSERT_EQ(future.Get(),
            PermissionControllerImpl::OverrideStatus::kOverrideSet);

  EXPECT_TRUE(content::ExecJs(rfh_c_2, "document.requestStorageAccess()"));

  // After requesting storage access, this third-party context should nwo be
  // able to access the first-party blob URL.
  EXPECT_EQ(true, EvalJs(rfh_c_2, fetch_blob_url_js));

  EXPECT_TRUE(ExecJs(rfh_c, JsReplace("URL.revokeObjectURL($1)", blob_url)));
}

class BlobUrlDevToolsIssueTest : public ContentBrowserTest {
 protected:
  BlobUrlDevToolsIssueTest() {
    feature_list_.InitWithFeatures(
        {features::kBlockCrossPartitionBlobUrlFetching,
         blink::features::kEnforceNoopenerOnBlobURLNavigation},
        {});
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    client_ = std::make_unique<MockContentBrowserClient>();
  }

  void TearDownOnMainThread() override { client_.reset(); }

  void WaitForIssueAndCheckUrl(const std::string& url,
                               TestDevToolsProtocolClient* client,
                               const std::string& expected_info_enum) {
    // Wait for notification of a Partitioning Blob URL Issue.
    base::Value::Dict params = client->WaitForMatchingNotification(
        "Audits.issueAdded",
        base::BindRepeating([](const base::Value::Dict& params) {
          const std::string* issue_code =
              params.FindStringByDottedPath("issue.code");
          return issue_code && *issue_code == "PartitioningBlobURLIssue";
        }));

    EXPECT_THAT(params, base::test::IsSupersetOfValue(
                            base::test::ParseJson(content::JsReplace(
                                R"({
                  "issue": {
                    "code": "PartitioningBlobURLIssue",
                    "details": {
                      "partitioningBlobURLIssueDetails": {
                        "url": $1,
                        "partitioningBlobURLInfo": $2,
                      }
                    }
                  }
                })",
                                url, expected_info_enum))));

    // Clear existing notifications so subsequent calls don't fail by checking
    // `url` against old notifications.
    client->ClearNotifications();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockContentBrowserClient> client_;
};

IN_PROC_BROWSER_TEST_F(BlobUrlDevToolsIssueTest, PartitioningBlobUrlIssue) {
  // TODO(https://crbug.com/395911627): convert browser_tests to
  // inspector-protocol test
  GURL main_url = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* rfh_c = shell()->web_contents()->GetPrimaryMainFrame();

  std::string blob_url_string =
      EvalJs(
          rfh_c,
          "const blob_url = URL.createObjectURL(new "
          "Blob(['<!doctype html><body>potato</body>'], {type: 'text/html'}));"
          "blob_url;")
          .ExtractString();
  GURL blob_url(blob_url_string);

  RenderFrameHost* rfh_b = ChildFrameAt(rfh_c, 0);
  RenderFrameHost* rfh_c_2 = ChildFrameAt(rfh_b, 0);

  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> future;
  static_cast<PermissionControllerImpl*>(
      rfh_c_2->GetBrowserContext()->GetPermissionController())
      ->SetPermissionOverride(
          /*requesting_origin=*/std::nullopt,
          /*embedding_origin=*/std::nullopt,
          blink::PermissionType::STORAGE_ACCESS_GRANT,
          blink::mojom::PermissionStatus::DENIED, future.GetCallback());
  ASSERT_EQ(future.Get(),
            PermissionControllerImpl::OverrideStatus::kOverrideSet);

  std::unique_ptr<content::TestDevToolsProtocolClient> client =
      std::make_unique<content::TestDevToolsProtocolClient>();
  client->AttachToFrameTreeHost(rfh_c_2);
  client->SendCommandSync("Audits.enable");
  client->ClearNotifications();

  EXPECT_FALSE(ExecJs(
      rfh_c_2,
      JsReplace(
          "async function test() {"
          "const blob = await fetch($1).then(response => response.blob());"
          "await blob.text();}"
          "test();",
          blob_url)));
  WaitForIssueAndCheckUrl(blob_url_string, client.get(),
                          "BlockedCrossPartitionFetching");
  client->DetachProtocolClient();
}

IN_PROC_BROWSER_TEST_F(BlobUrlDevToolsIssueTest,
                       PartitioningBlobUrlNavigationIssue) {
  // TODO(https://crbug.com/395911627): convert browser_tests to
  // inspector-protocol test
  // 1. Navigate to c.com.
  GURL main_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  WebContents* web_contents = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(web_contents, main_url));
  RenderFrameHost* rfh_c = web_contents->GetPrimaryMainFrame();

  std::string blob_url_string =
      EvalJs(
          rfh_c,
          "const blob_url = URL.createObjectURL(new "
          "Blob(['<!doctype html><body>potato</body>'], {type: 'text/html'}));"
          "blob_url;")
          .ExtractString();

  // 2. Create blob_url in c.com (blob url origin is c.com).
  GURL blob_url(blob_url_string);

  // 3. window.open b.com with c.com embedded.
  // 3a. Navigate to b.com.
  GURL b_url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c)");

  // 3b. Open new tab from b.com context.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      content::ExecJs(rfh_c, content::JsReplace("window.open($1)", b_url)));

  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  RenderFrameHost* rfh_b = new_contents->GetPrimaryMainFrame();
  RenderFrameHost* rfh_c_in_b = ChildFrameAt(rfh_b, 0);

  // 4. Attach DevTools client to the innermost frame (c.com inside b.com).
  std::unique_ptr<TestDevToolsProtocolClient> client =
      std::make_unique<TestDevToolsProtocolClient>();
  client->AttachToFrameTreeHost(rfh_c_in_b);
  client->SendCommandSync("Audits.enable");
  client->ClearNotifications();

  // 4. Do the window.open of blob url from c.com.
  EXPECT_TRUE(
      ExecJs(rfh_c_in_b, JsReplace("handle = window.open($1);", blob_url)));

  WaitForIssueAndCheckUrl(blob_url_string, client.get(),
                          "EnforceNoopenerForNavigation");
  client->DetachProtocolClient();
}

class BlobURLBrowserTestP : public base::test::WithFeatureOverride,
                            public BlobUrlBrowserTest {
 public:
  BlobURLBrowserTestP()
      : base::test::WithFeatureOverride(
            blink::features::kEnforceNoopenerOnBlobURLNavigation) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(BlobURLBrowserTestP);

// Tests that an opaque origin document is able to window.open a Blob URL it
// created.
IN_PROC_BROWSER_TEST_P(BlobURLBrowserTestP,
                       NavigationWithOpaqueTopLevelOrigin) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<script></script>")));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(
      shell(),
      "const blob_url = URL.createObjectURL(new "
      "Blob(['<!doctype html><body>potato</body>'], {type: 'text/html'}));"
      "var handle = window.open(blob_url);"));

  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  bool handle_null = EvalJs(shell(), "handle === null;").ExtractBool();
  EXPECT_FALSE(handle_null);
}

}  // namespace content
