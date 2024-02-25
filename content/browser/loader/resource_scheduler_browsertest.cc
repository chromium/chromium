// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

namespace {

class ResourceSchedulerBrowserTest : public ContentBrowserTest {
 public:
  ResourceSchedulerBrowserTest(const ResourceSchedulerBrowserTest&) = delete;
  ResourceSchedulerBrowserTest& operator=(const ResourceSchedulerBrowserTest&) =
      delete;

 protected:
  ResourceSchedulerBrowserTest() {}
  ~ResourceSchedulerBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ResourceSchedulerBrowserTest,
                       DISABLED_ResourceLoadingExperimentIncognito) {
  GURL url(embedded_test_server()->GetURL(
      "/resource_loading/resource_loading_non_mobile.html"));

  Shell* otr_browser = CreateOffTheRecordBrowser();
  EXPECT_TRUE(NavigateToURL(otr_browser, url));
  EXPECT_EQ(9, EvalJs(otr_browser, "getResourceNumber()"));
}

IN_PROC_BROWSER_TEST_F(ResourceSchedulerBrowserTest,
                       DISABLED_ResourceLoadingExperimentNormal) {
  GURL url(embedded_test_server()->GetURL(
      "/resource_loading/resource_loading_non_mobile.html"));
  Shell* browser = shell();
  EXPECT_TRUE(NavigateToURL(browser, url));
  EXPECT_EQ(9, EvalJs(browser, "getResourceNumber()"));
}

}  // anonymous namespace

}  // namespace content
