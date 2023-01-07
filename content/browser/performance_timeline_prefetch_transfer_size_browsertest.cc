// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class PerformanceTimelinePrefetchTransferSizeBrowserTest
    : public ContentBrowserTest {
 public:
  PerformanceTimelinePrefetchTransferSizeBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  EvalJsResult Prefetch() {
    std::string script = R"(
        (() => {
          return addPrefetch();
        })();
    )";
    return EvalJs(shell(), script);
  }
  [[nodiscard]] EvalJsResult GetTransferSize() {
    std::string script = R"(
        (() => {
          return performance.getEntriesByType('navigation')[0].transferSize;
        })();
    )";
    return EvalJs(shell(), script);
  }
};

IN_PROC_BROWSER_TEST_F(PerformanceTimelinePrefetchTransferSizeBrowserTest,
                       PrefetchTransferSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL prefetch_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL landing_url(embedded_test_server()->GetURL(
      "a.com", "/performance_timeline/prefetch.html"));

  EXPECT_TRUE(NavigateToURL(shell(), landing_url));
  Prefetch();
  EXPECT_TRUE(NavigateToURL(shell(), prefetch_url));
  // Navigate to a prefetched url should result in a navigation timing entry
  // with 0 transfer size.
  EXPECT_EQ(0, GetTransferSize());
}

}  // namespace content
