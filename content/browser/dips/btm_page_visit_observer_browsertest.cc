// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_page_visit_observer.h"

#include "content/browser/dips/btm_page_visit_observer_test_utils.h"
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

  ASSERT_THAT(recorder.visits(),
              ElementsAre(AllOf(PreviousPage(HasUrl(GURL())), HasUrl(url1)),
                          AllOf(PreviousPage(HasUrl(url1)), HasUrl(url2)),
                          AllOf(PreviousPage(HasUrl(url2)), HasUrl(url3))));
}

}  // namespace
}  // namespace content
