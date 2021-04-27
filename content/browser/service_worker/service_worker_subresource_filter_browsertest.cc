// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// This script asks the fetch_event_pass_through service worker what fetch
// events it saw.
const std::string script = R"(
      (async () => {
        const saw_message = new Promise(resolve => {
          navigator.serviceWorker.onmessage = event => {
            resolve(event.data);
          };
        });
        const registration = await navigator.serviceWorker.ready;
        registration.active.postMessage('');
        return await saw_message;
      })();
  )";

class ServiceWorkerSubresourceFilterBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  ServiceWorkerSubresourceFilterBrowserTest() {
    feature_list_.InitWithFeatureState(
        features::kServiceWorkerSubresourceFilter, FeatureIsEnabled());
  }
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  void NavigateAndFetch(std::string url, bool expect_only_matching_filter) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(shell(), "register('fetch_event_pass_through.js');"));

    GURL page_url = embedded_test_server()->GetURL(url);
    GURL fetch_url = embedded_test_server()->GetURL("/echo");
    GURL fetch_url_with_fragment = embedded_test_server()->GetURL("/echo#foo");
    GURL fetch_url_with_fragment_substring =
        embedded_test_server()->GetURL("/echo#afooz");
    GURL fetch_url_with_other_fragment =
        embedded_test_server()->GetURL("/echo#bar");

    EXPECT_TRUE(NavigateToURL(shell(), page_url));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)", fetch_url)));
    EXPECT_EQ("Echo", EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                                fetch_url_with_fragment)));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                        fetch_url_with_fragment_substring)));
    EXPECT_EQ("Echo",
              EvalJs(shell(), JsReplace("fetch_from_page($1)",
                                        fetch_url_with_other_fragment)));

    base::Value list(base::Value::Type::LIST);
    if (expect_only_matching_filter) {
      list.Append(page_url.spec());
      list.Append(fetch_url_with_fragment.spec());
      list.Append(fetch_url_with_fragment_substring.spec());
    } else {
      list.Append(page_url.spec());
      list.Append(fetch_url.spec());
      list.Append(fetch_url_with_fragment.spec());
      list.Append(fetch_url_with_fragment_substring.spec());
      list.Append(fetch_url_with_other_fragment.spec());
    }

    EXPECT_EQ(list, EvalJs(shell(), script));
  }

  bool FeatureIsEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(EnabledDisabled,
                         ServiceWorkerSubresourceFilterBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest, WithFilter) {
  // If the feature is disabled, all URLs should be seen by the Service Worker.
  // If the feature is enabled, only the initial navigation URL and URLs
  // matching the filter should be seen by the Service Worker.
  NavigateAndFetch("/service_worker/subresource_filter.html",
                   FeatureIsEnabled());
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest,
                       WithoutFilter) {
  // All URLs should be seen by the Service Worker regardless of whether or not
  // the feature is enabled.
  NavigateAndFetch("/service_worker/fetch_from_page.html", false);
}

IN_PROC_BROWSER_TEST_P(ServiceWorkerSubresourceFilterBrowserTest,
                       WithEmptyFilter) {
  // All URLs should be seen by the Service Worker regardless of whether or not
  // the feature is enabled.
  NavigateAndFetch("/service_worker/subresource_filter_empty.html", false);
}

}  // namespace
}  // namespace content