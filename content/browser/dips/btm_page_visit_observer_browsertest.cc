// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_page_visit_observer.h"

#include "content/browser/dips/btm_page_visit_observer_test_utils.h"
#include "content/browser/dips/dips_test_utils.h"
#include "content/browser/dips/dips_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;

namespace content {
namespace {

class BtmPageVisitObserverBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, SmokeTest) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  ASSERT_THAT(recorder.visits(),
              ElementsAre(AllOf(PreviousPage(HasUrl(GURL())), HasUrl(url1)),
                          AllOf(PreviousPage(HasUrl(url1)), HasUrl(url2)),
                          AllOf(PreviousPage(HasUrl(url2)), HasUrl(url3))));
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, Redirects) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  // url2 redirects to url2b which redirects to url3.
  const GURL url2 = embedded_https_test_server().GetURL(
      "b.test",
      "/server-redirect-with-cookie?%2Fcross-site%3Fc.test%252Fempty.html");
  const GURL url2b = embedded_https_test_server().GetURL(
      "b.test", "/cross-site?c.test%2Fempty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2, url3));
  ASSERT_TRUE(recorder.WaitForSize(2));

  // The first redirect accessed cookies, and the second did not.
  ASSERT_THAT(
      recorder.visits(),
      ElementsAre(AllOf(PreviousPage(HasUrl(GURL())),
                        Navigation(ServerRedirects(IsEmpty())), HasUrl(url1)),
                  AllOf(PreviousPage(HasUrl(url1)),
                        Navigation(ServerRedirects(ElementsAre(
                            AllOf(HasUrl(url2), DidWriteCookies(true)),
                            AllOf(HasUrl(url2b), DidWriteCookies(false))))),
                        HasUrl(url3))));
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, DocumentCookie) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(ExecJs(web_contents, "document.cookie = 'foo=bar';"));
  ASSERT_TRUE(NavigateToURL(web_contents, url3));
  ASSERT_TRUE(recorder.WaitForSize(3));

  // url2 accessed cookies; no other page did.
  ASSERT_THAT(
      recorder.visits(),
      ElementsAre(AllOf(PreviousPage(AllOf(HasUrl(GURL()),
                                           HadQualifyingStorageAccess(false))),
                        HasUrl(url1)),
                  AllOf(PreviousPage(AllOf(HasUrl(url1),
                                           HadQualifyingStorageAccess(false))),
                        HasUrl(url2)),
                  AllOf(PreviousPage(AllOf(HasUrl(url2),
                                           HadQualifyingStorageAccess(true))),
                        HasUrl(url3))));
}

IN_PROC_BROWSER_TEST_F(BtmPageVisitObserverBrowserTest, UserActivation) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  WebContents* web_contents = shell()->web_contents();
  BtmPageVisitRecorder recorder(web_contents);

  ASSERT_TRUE(NavigateToURL(web_contents, url1));
  SimulateUserActivation(web_contents);
  ASSERT_TRUE(NavigateToURL(web_contents, url2));
  ASSERT_TRUE(recorder.WaitForSize(2));

  ASSERT_THAT(
      recorder.visits(),
      ElementsAre(
          AllOf(PreviousPage(
                    AllOf(HasUrl(GURL()), ReceivedUserActivation(false))),
                HasUrl(url1)),
          AllOf(PreviousPage(AllOf(HasUrl(url1), ReceivedUserActivation(true))),
                HasUrl(url2))));
}

}  // namespace
}  // namespace content
