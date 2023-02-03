// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/pattern.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

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
  }
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
  EXPECT_TRUE(ExecJs(
      shell(),
      "var spoof_fn = function () {\n"
      "  host_port = self.origin.split('://')[1];\n"
      "  spoof_url = 'blob:http://spoof.com@' + host_port + '/abcd';\n"
      "  window.history.replaceState({}, '', spoof_url);\n"
      "};\n"
      "args = ['<body>potato<scr', 'ipt>(', spoof_fn, ')();</scri', 'pt>'];\n"
      "b = new Blob(args, {type: 'text/html'});"
      "window.open(URL.createObjectURL(b));"));

  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // The spoofy URL should not be shown to the user.
  EXPECT_FALSE(
      base::MatchPattern(new_contents->GetVisibleURL().spec(), "*spoof*"));

  // The currently implemented behavior is that the URL gets rewritten to
  // about:blank#blocked. The content of the page stays the same.
  EXPECT_EQ(kBlockedURL, new_contents->GetVisibleURL().spec());
  EXPECT_EQ(
      origin.Serialize() + " potato",
      EvalJs(new_contents, "self.origin + ' ' + document.body.innerText;"));

  // TODO(nick): Currently, window.location still reflects the spoof URL.
  // This seems unfortunate -- can we fix it?
  std::string window_location =
      EvalJs(new_contents, "window.location.href;").ExtractString();
  EXPECT_TRUE(base::MatchPattern(window_location, "*spoof*"));
}

enum class PartitionedBlobUrlBrowserTestTestCase {
  kSupportPartitionedBlobUrlDisabled,
  kSupportPartitionedBlobUrlEnabled,
};

class PartitionedBlobUrlBrowserTestP
    : public BlobUrlBrowserTest,
      public testing::WithParamInterface<
          PartitionedBlobUrlBrowserTestTestCase> {
 public:
  PartitionedBlobUrlBrowserTestP() {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kSupportPartitionedBlobUrl,
        GetParam() == PartitionedBlobUrlBrowserTestTestCase::
                          kSupportPartitionedBlobUrlEnabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PartitionedBlobUrlBrowserTestP,
                       BlobUrlStoreRegisterMetrics) {
  GURL main_url(embedded_test_server()->GetURL("chromium.org", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(ExecJs(shell(), "URL.createObjectURL(new Blob(['foo']))"));

  FetchHistogramsFromChildProcesses();

  if (base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl)) {
    histogram_tester.ExpectTotalCount(
        "Storage.Blob.RegisterURLTimeWithPartitioningSupport.Frame", 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Storage.Blob.RegisterURLTimeWithoutPartitioningSupport", 1);
  }
}

IN_PROC_BROWSER_TEST_P(PartitionedBlobUrlBrowserTestP,
                       BlobUrlStoreRegisterMetricsWorker) {
  GURL main_url(embedded_test_server()->GetURL("chromium.org", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  base::HistogramTester histogram_tester;

  EXPECT_TRUE(ExecJs(shell(), R"(
  function workerCode() {
    URL.createObjectURL(new Blob(['foo']));
    postMessage(true);
  }
  var workerBlob = new Blob(
    [workerCode.toString() + ';workerCode();'],
    {type: 'application/javascript'});
  new Promise((resolve) => {
    var w = new Worker(URL.createObjectURL(workerBlob));
    w.onmessage = (e) => {
      w.terminate();
      resolve();
    };
  });)"));

  FetchHistogramsFromChildProcesses();

  // We expect two PublicURLManager::RegisterURL calls, one from the frame when
  // it creates the worker and another when the worker creates a blob URL.
  if (base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl)) {
    histogram_tester.ExpectTotalCount(
        "Storage.Blob.RegisterURLTimeWithPartitioningSupport.Frame", 1);
    histogram_tester.ExpectTotalCount(
        "Storage.Blob.RegisterURLTimeWithPartitioningSupport.Worker", 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Storage.Blob.RegisterURLTimeWithoutPartitioningSupport", 2);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PartitionedBlobUrlBrowserTest,
    PartitionedBlobUrlBrowserTestP,
    ::testing::Values(PartitionedBlobUrlBrowserTestTestCase::
                          kSupportPartitionedBlobUrlDisabled,
                      PartitionedBlobUrlBrowserTestTestCase::
                          kSupportPartitionedBlobUrlEnabled));

}  // namespace content
