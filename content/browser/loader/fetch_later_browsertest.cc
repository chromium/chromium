// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/loader/keep_alive_request_browsertest_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kFetchLaterEndpoint[] = "/fetch-later";

}  // namespace

// A base class to help testing JS fetchLater() API behaviors.
class FetchLaterBrowserTestBase : public KeepAliveRequestBrowserTestBase {
 protected:
  void SetUp() override {
    // fetchLater() API only supports HTTPS requests.
    SetUseHttps();
    KeepAliveRequestBrowserTestBase::SetUp();
  }

  bool NavigateToURL(const GURL& url) {
    previous_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    bool ret = content::NavigateToURL(web_contents(), url);
    current_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    return ret;
  }
  bool WaitUntilPreviousDocumentDeleted() {
    CHECK(previous_document_);
    // `previous_document_` might already be destroyed here.
    return previous_document_->WaitUntilRenderFrameDeleted();
  }
  // Caution: the returned document might already be killed if BFCache is not
  // working.
  RenderFrameHostImplWrapper& previous_document() {
    CHECK(previous_document_);
    CHECK(!previous_document_->IsDestroyed());
    return *previous_document_;
  }
  RenderFrameHostImplWrapper& current_document() {
    CHECK(previous_document_);
    return *current_document_;
  }

  // Navigates to an empty page, and executes `script` on it.
  void RunScript(const std::string& script) {
    ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
    ASSERT_TRUE(ExecJs(web_contents(), script));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Navigates to a page that executes `script`, and navigates to another page.
  void RunScriptAndNavigateAway(const std::string& script) {
    RunScript(script);

    // Navigate to cross-origin page to ensure the 1st page can be unloaded if
    // BackForwardCache is disabled.
    ASSERT_TRUE(
        NavigateToURL(server()->GetURL(kSecondaryHost, "/title2.html")));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Expects `total` number of FetchLater requests to be sent.
  // `total` must equal to the size of `request_handlers`.
  // `requester_handlers` are to wait for the FetchLater requests and to
  // respond.
  void ExpectFetchLaterRequests(
      size_t total,
      std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>&
          request_handlers) {
    SCOPED_TRACE(
        base::StringPrintf("ExpectFetchLaterRequests: %zu requests", total));
    ASSERT_EQ(total, request_handlers.size());
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), total);

    for (const auto& handler : request_handlers) {
      // Waits for a FetchLater request.
      handler->WaitForRequest();
      // Sends back final response to terminate in-browser request handling.
      handler->Send(k200TextResponse);
      // Triggers OnComplete.
      handler->Done();
    }

    loaders_observer().WaitForTotalOnReceiveResponse(total);
    // TODO(crbug.com/40236167): Check NumLoadersForTesting==0 after migrating
    // to in-browser ThrottlingURLLoader. Current implementation cannot ensure
    // receiving renderer disconnection. Also need to wait for TotalOnComplete
    // by `total`, not by states.
  }

  GURL GetFetchLaterPageURL(const std::string& host,
                            const std::string& method) const {
    std::string url = base::StrCat(
        {"/set-header-with-file/content/test/data/fetch_later.html?"
         "method=",
         method});
    return server()->GetURL(host, url);
  }

 private:
  std::unique_ptr<RenderFrameHostImplWrapper> current_document_ = nullptr;
  std::unique_ptr<RenderFrameHostImplWrapper> previous_document_ = nullptr;
};

class FetchLaterBasicBrowserTest : public FetchLaterBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {{}}}};
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(FetchLaterBasicBrowserTest, CallInMainDocument) {
  const std::string target_url = kFetchLaterEndpoint;
  ASSERT_TRUE(server()->Start());

  RunScript(JsReplace(R"(
    fetchLater($1);
  )",
                      target_url));
  ASSERT_FALSE(current_document().IsDestroyed());

  // The loader should still be connected as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

IN_PROC_BROWSER_TEST_F(FetchLaterBasicBrowserTest, CallInSameOriginChild) {
  ASSERT_TRUE(server()->Start());

  RunScript(JsReplace(
      R"(
    var childPromise = new Promise((resolve, reject) => {
      window.addEventListener('message', e => {
        if (e.data.type === 'fetchLater.done') {
          resolve(e.data.type);
        } else {
          reject(e.data.type);
        }
      });
    });

    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
      GetFetchLaterPageURL(kPrimaryHost, net::HttpRequestHeaders::kGetMethod)));
  ASSERT_FALSE(current_document().IsDestroyed());

  EXPECT_EQ("fetchLater.done", EvalJs(web_contents(), "childPromise"));
  // The loader should still be connected as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// By default of `deferred-fetch-minimal` policy, `fetchLater()` should be
// allowed in first X cross-origin child iframes.
IN_PROC_BROWSER_TEST_F(FetchLaterBasicBrowserTest, CallInCrossOriginChild) {
  const std::string target_url = kFetchLaterEndpoint;
  ASSERT_TRUE(server()->Start());

  RunScript(JsReplace(
      R"(
    var childPromise = new Promise((resolve, reject) => {
      window.addEventListener('message', e => {
        if (e.data.type === 'fetchLater.done') {
          resolve(e.data.type);
        } else {
          reject(e.data.type + ': ' + e.data.error);
        }
      });
    });

    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )",
      GetFetchLaterPageURL(kSecondaryHost,
                           net::HttpRequestHeaders::kGetMethod)));
  ASSERT_FALSE(current_document().IsDestroyed());

  EXPECT_EQ("fetchLater.done", EvalJs(web_contents(), "childPromise"));
  // The loader should still exist as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// A type to support parameterized testing for timeout-related tests.
struct TestTimeoutType {
  std::string test_case_name;
  int32_t timeout;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is off.
//
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
class FetchLaterNoBackForwardCacheBrowserTest
    : public FetchLaterBrowserTestBase,
      public testing::WithParamInterface<TestTimeoutType> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {{}}}};
    return enabled_features;
  }
  const DisabledFeaturesType& GetDisabledFeatures() override {
    static const DisabledFeaturesType disabled_features = {
        features::kBackForwardCache};
    return disabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FetchLaterNoBackForwardCacheBrowserTest,
    testing::ValuesIn<std::vector<TestTimeoutType>>({
        {"LongTimeout", 600000},      // 10 minutes
        {"OneMinuteTimeout", 60000},  // 1 minute
    }),
    [](const testing::TestParamInfo<TestTimeoutType>& info) {
      return info.param.test_case_name;
    });

// All pending FetchLater requests should be sent after the initiator page is
// gone, no matter how much time their activateAfter has left.
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
IN_PROC_BROWSER_TEST_P(FetchLaterNoBackForwardCacheBrowserTest,
                       SendOnPageDiscardBeforeActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url, target_url});
  ASSERT_TRUE(server()->Start());

  // Creates two FetchLater requests with various long activateAfter, which
  // should all be sent on page discard.
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: $2});
    fetchLater($1, {activateAfter: $2});
  )",
                                     target_url, GetParam().timeout));
  // Ensure the 1st page has been unloaded.
  ASSERT_TRUE(WaitUntilPreviousDocumentDeleted());

  // Loaders are disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 2u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(2, request_handlers);
}

class FetchLaterWithBackForwardCacheMetricsBrowserTestBase
    : public FetchLaterBrowserTestBase,
      public BackForwardCacheMetricsTestMatcher {
 protected:
  void SetUpOnMainThread() override {
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    FetchLaterBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    histogram_tester_.reset();
    FetchLaterBrowserTestBase::TearDownOnMainThread();
  }

  // `BackForwardCacheMetricsTestMatcher` implementation.
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }
  const base::HistogramTester& histogram_tester() override {
    return *histogram_tester_;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is on but does
// not come into play.
//
// Setting long `BackForwardCache TTL (1min)` so that FetchLater sending cannot
// be caused by page eviction out of BackForwardCache.
class FetchLaterNoActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// A pending FetchLater request with default options should be sent after the
// initiator page is gone.
// Similar to SendOnPageDiscardBeforeActivationTimeout.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       SendOnPageDeletion) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request in an iframe, which is removed after loaded.
  ASSERT_TRUE(NavigateToURL(
      server()->GetURL(kPrimaryHost, "/page_with_blank_iframe.html")));
  ASSERT_TRUE(ExecJs(web_contents(), R"(
    var promise = new Promise(resolve => {
      window.addEventListener('message', e => {
        const iframe = document.getElementById('test_iframe');
        iframe.remove();
        resolve(e.data);
      });
    });
  )"));
  auto* iframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(web_contents(), 0));
  EXPECT_TRUE(ExecJs(iframe, JsReplace(R"(
      fetchLater($1);
      window.parent.postMessage(true, "*");
    )",
                                       target_url)));
  // `iframe` is removed after it calls fetchLater().
  EXPECT_EQ(true, EvalJs(web_contents(), "promise"));

  // The loader is disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should have been sent after its page gets
// restored from BackForwardCache before getting evicted. It is because, by
// default, pending requests are all flushed on BFCache no matter
// BackgroundSync is on or not. See http://crbug.com/310541607#comment28.
IN_PROC_BROWSER_TEST_F(
    FetchLaterNoActivationTimeoutBrowserTest,
    FlushedWhenPageIsRestoredBeforeBeingEvictedFromBackForwardCache) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1);
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Navigate back to the 1st page.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The same page is still alive.
  ExpectRestored(FROM_HERE);
  // The FetchLater requests should've been sent.
  ExpectFetchLaterRequests(1, request_handlers);
}

// Without an activateAfter set, a pending FetchLater request should not be
// sent out during its page frozen state.
// Similar to ResetActivationTimeoutTimerOnPageResume.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       NotSendWhenPageIsResumedAfterBeingFrozen) {
  const std::string target_url = kFetchLaterEndpoint;
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with NO activateAfter.
  // It should be impossible to send out during page frozen.
  ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    fetchLater($1);
  )",
                                               target_url)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Forces to freeze the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(true);

  // The FetchLater request should not be sent.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

  // Forces to wake up the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(false);
  // The FetchLater request should not be sent.
  // TODO(crbug.com/40276121): Verify FetchLaterResult once
  // https://crrev.com/c/4820528 is submitted.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Tests to cover FetchLater's activateAfter behaviors when BackForwardCache
// is on and may come into play.
//
// BackForwardCache eviction is simulated by calling
// `DisableBFCacheForRFHForTesting(previous_document())` instead of relying on
// its TTL.
class FetchLaterActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        // Sets to a long timeout, as tests below should not rely on it.
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// When setting activateAfter=0, a pending FetchLater request should be sent
// "roughly" immediately.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnZeroActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with activateAfter=0s.
  RunScript(JsReplace(R"(
    fetchLater($1, {activateAfter: 0});
  )",
                      target_url));
  ASSERT_FALSE(current_document().IsDestroyed());

  // The loader should still exist as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  // The FetchLater request should be sent, triggered by its activateAfter.
  ExpectFetchLaterRequests(1, request_handlers);
}

// When setting activateAfter>0, a pending FetchLater request should be sent
// after around the specified time, if no navigation happens.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with activateAfter=2s.
  // It should be sent out after 2s.
  RunScript(JsReplace(R"(
    fetchLater($1, {activateAfter: 2000});
  )",
                      target_url));
  ASSERT_FALSE(current_document().IsDestroyed());

  // The loader should still exist as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  // The FetchLater request should be sent, triggered by its activateAfter.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should be sent when its page is evicted out of
// BackForwardCache.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnBackForwardCachedEviction) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with long activateAfter (3min)
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: 180000});
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Forces evicting previous page. This will also post a task that destroys it.
  DisableBFCacheForRFHForTesting(previous_document()->GetGlobalId());
  ASSERT_TRUE(previous_document()->is_evicted_from_back_forward_cache());
  // Eviction happens immediately, but RFH deletion may be delayed.
  ASSERT_TRUE(previous_document().WaitUntilRenderFrameDeleted());

  // The loader is disconnected after the page is evicted by browser process to
  // start loading the request. However, it may happen earlier or later, so it's
  // difficult to assert the existence of the disconnected loader.

  // At the end, the FetchLater request should be sent, and the loader is
  // expected to process the response.
  ExpectFetchLaterRequests(1, request_handlers);
}

// All other send-on-BFCache behaviors are covered in
// send-on-deactivate.tentative.https.window.js

}  // namespace content
