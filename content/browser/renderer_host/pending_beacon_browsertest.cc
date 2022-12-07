// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>
#include <unordered_map>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

MATCHER(IsFrameVisible,
        base::StrCat({"Frame is", negation ? " not" : "", " visible"})) {
  return arg->GetVisibilityState() == PageVisibilityState::kVisible;
}

MATCHER(IsFrameHidden,
        base::StrCat({"Frame is", negation ? " not" : "", " hidden"})) {
  return arg->GetVisibilityState() == PageVisibilityState::kHidden;
}

class PendingBeaconTimeoutBrowserTestBase : public ContentBrowserTest {
 protected:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
    ContentBrowserTest::SetUp();
  }
  virtual const FeaturesType& GetEnabledFeatures() = 0;

  void SetUpOnMainThread() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    CheckPermissionStatus(blink::PermissionType::BACKGROUND_SYNC,
                          blink::mojom::PermissionStatus::GRANTED);
    // TODO(crbug.com/1293679): Update ContentBrowserTest to support overriding
    // permissions.

    host_resolver()->AddRule("*", "127.0.0.1");

    // Initializes an HTTPS server, as the PendingBeacon API is only supported
    // in secure context.
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_test_server_->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(https_test_server_.get());
    // Using `base::Unretained()` as `https_test_server()` is owned by this
    // class and and should not be able to outlive.
    https_test_server_->RegisterDefaultHandler(base::BindRepeating(
        &PendingBeaconTimeoutBrowserTestBase::HandleBeaconRequest,
        base::Unretained(this)));
  }
  void TearDownOnMainThread() override {
    histogram_tester_.reset();
    ContentBrowserTest::SetUpOnMainThread();
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

  // Runs JS `script` in page A, and then navigates to page B.
  void RunScriptInANavigateToB(const std::string& script) {
    RunScriptInA(script);

    // Navigate to B.
    ASSERT_TRUE(
        NavigateToURL(https_test_server()->GetURL("b.test", "/title1.html")));
  }

  // Runs JS `script` in page A.
  void RunScriptInA(const std::string& script) {
    // Navigate to A.
    ASSERT_TRUE(
        NavigateToURL(https_test_server()->GetURL("a.test", "/title1.html")));
    // Execute `script` in A.
    ASSERT_TRUE(ExecJs(web_contents(), script));
  }

  // Registers a request monitor to wait for `total_beacon` beacons received,
  // and then starts the test server.
  void RegisterBeaconRequestMonitor(const size_t total_beacon) {
    // Using base::Unretained() as `https_test_server()` is owned by
    // `content::BrowserTestBase` and should not be able to outlive.
    https_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &PendingBeaconTimeoutBrowserTestBase::MonitorBeaconRequest,
        base::Unretained(this), total_beacon));
    ASSERT_TRUE(https_test_server()->Start());
  }

  // Waits for `kBeaconEndpoint` to be requested `total_beacon` times.
  // If `sent_beacon_count_` does not yet reach `total_beacon`, a RunLoop will
  // be created and runs until it is stopped by `MonitorBeaconRequest`.
  void WaitForAllBeaconsSent(size_t total_beacon) {
    {
      base::AutoLock auto_lock(count_lock_);
      if (sent_beacon_count_ >= total_beacon)
        return;
    }
    {
      base::AutoLock auto_lock(count_lock_);
      waiting_run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
    }
    waiting_run_loop_->Run();
    {
      base::AutoLock auto_lock(count_lock_);
      waiting_run_loop_.reset();
    }

    // Tries to wait such that browser can process the responses from http
    // server. (No guarantee)
    base::RunLoop().RunUntilIdle();
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHostWrapper& current_document() {
    current_document_ =
        std::make_unique<RenderFrameHostWrapper>(current_frame_host());
    return *current_document_;
  }
  // Caution: the returned might already be killed if BFCache it not working.
  RenderFrameHostWrapper& previous_document() {
    CHECK(previous_document_);
    CHECK(!previous_document_->IsDestroyed());
    return *previous_document_;
  }
  bool WaitUntilPreviousDocumentDeleted() {
    CHECK(previous_document_);
    return previous_document_->WaitUntilRenderFrameDeleted();
  }

  bool NavigateToURL(const GURL& url) {
    previous_document_ =
        std::make_unique<RenderFrameHostWrapper>(current_frame_host());
    return content::NavigateToURL(web_contents(), url);
  }

  size_t sent_beacon_count() {
    base::AutoLock auto_lock(count_lock_);
    return sent_beacon_count_;
  }

  void CheckPermissionStatus(blink::PermissionType permission_type,
                             blink::mojom::PermissionStatus permission_status) {
    auto* permission_controller_delegate =
        web_contents()->GetBrowserContext()->GetPermissionControllerDelegate();

    base::MockOnceCallback<void(blink::mojom::PermissionStatus)> callback;
    EXPECT_CALL(callback, Run(permission_status));
    permission_controller_delegate->RequestPermission(
        permission_type, current_frame_host(), GURL("127.0.0.1"),
        /*user_gesture=*/true, callback.Get());
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  void ExpectActions(
      const std::unordered_map<PendingBeaconHost::Action, size_t>& actions) {
    for (const auto& [action, count] : actions) {
      histogram_tester().ExpectBucketCount("PendingBeaconHost.Action", action,
                                           count);
    }
  }
  void ExpectBatchActions(
      const std::unordered_map<PendingBeaconHost::BatchAction, size_t>&
          batch_actions) {
    for (const auto& [batch_action, count] : batch_actions) {
      histogram_tester().ExpectBucketCount("PendingBeaconHost.BatchAction",
                                           batch_action, count);
    }
  }
  void ExpectBatchAction(const PendingBeaconHost::BatchAction& batch_action,
                         size_t count) {
    for (auto e = PendingBeaconHost::BatchAction::kNone;
         e != PendingBeaconHost::BatchAction::kMaxValue;
         e = PendingBeaconHost::BatchAction(static_cast<int>(e) + 1)) {
      const size_t expected_count = e == batch_action ? count : 0;
      histogram_tester().ExpectBucketCount("PendingBeaconHost.BatchAction", e,
                                           expected_count);
    }
  }
  void ExpectNoBatchAction() {
    ExpectBatchActions(
        {{PendingBeaconHost::BatchAction::kSendAllOnNavigation, 0},
         {PendingBeaconHost::BatchAction::kSendAllOnProcessExit, 0},
         {PendingBeaconHost::BatchAction::kSendAllOnHostDestroy, 0}});
  }
  // Expect the given `count` number of beacons are sent by `batch_action`.
  void ExpectSendByBrowserBatchAction(
      const PendingBeaconHost::BatchAction& batch_action,
      size_t count) {
    ExpectActions({{PendingBeaconHost::Action::kCreate, count},
                   // Expect to not be sent by single beacon's sendNow().
                   {PendingBeaconHost::Action::kSend, 0},
                   {PendingBeaconHost::Action::kNetworkSend, count},
                   // No guarantee RFH can be alive when receiving response.
                   // {PendingBeaconHost::Action::kNetworkComplete, count},
                   {PendingBeaconHost::Action::kDelete, 0}});
    ExpectBatchAction(batch_action, count);
  }
  // Expect the given `count` number of beacons are sent by a `sendNow() request
  // from renderer. Note that `sendNow()` can be triggered either by JS call or
  // by timers for `backgroundTimeout`/`timeout`.
  void ExpectSendByRendererAction(size_t count) {
    ExpectActions({{PendingBeaconHost::Action::kCreate, count},
                   // Expect to be sent by beacon's sendNow() from renderer.
                   {PendingBeaconHost::Action::kSend, count},
                   {PendingBeaconHost::Action::kNetworkSend, count},
                   // No guarantee RFH can be alive when receiving response.
                   // {PendingBeaconHost::Action::kNetworkComplete, count},
                   {PendingBeaconHost::Action::kDelete, 0}});
    ExpectNoBatchAction();
  }

  static constexpr char kBeaconEndpoint[] = "/pending_beacon/timeout";

 private:
  // Waits until `total_beacon` beacons received and stops `waiting_run_loop_`.
  // Invoked on `https_test_server()`'s IO Thread, so it's required to use
  // a lock to protect shared data `sent_beacon_count_` access.
  void MonitorBeaconRequest(const size_t total_beacon,
                            const net::test_server::HttpRequest& request) {
    if (request.relative_url == kBeaconEndpoint) {
      {
        base::AutoLock auto_lock(count_lock_);
        sent_beacon_count_++;
        if (sent_beacon_count_ < total_beacon) {
          return;
        }
      }

      base::AutoLock auto_lock(count_lock_);
      if (waiting_run_loop_) {
        waiting_run_loop_->Quit();
      }
    }
  }

  // Invoked on `https_test_server()`'s IO Thread.
  // PendingBeacon doesn't really look into its response, so this method just
  // returns OK status.
  std::unique_ptr<net::test_server::HttpResponse> HandleBeaconRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kBeaconEndpoint) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    return response;
  }

  RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<net::EmbeddedTestServer> https_test_server_ = nullptr;

  base::Lock count_lock_;
  size_t sent_beacon_count_ GUARDED_BY(count_lock_) = 0;
  std::unique_ptr<base::RunLoop> waiting_run_loop_;

  std::unique_ptr<RenderFrameHostWrapper> current_document_ = nullptr;
  std::unique_ptr<RenderFrameHostWrapper> previous_document_ = nullptr;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class PendingBeaconWithBackForwardCacheMetricsBrowserTestBase
    : public PendingBeaconTimeoutBrowserTestBase,
      public BackForwardCacheMetricsTestMatcher {
 protected:
  void SetUpOnMainThread() override {
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    PendingBeaconTimeoutBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    PendingBeaconTimeoutBrowserTestBase::TearDownOnMainThread();
  }

  // `BackForwardCacheMetricsTestMatcher` implementation.
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }
  // `BackForwardCacheMetricsTestMatcher` implementation.
  const base::HistogramTester& histogram_tester() override {
    return PendingBeaconTimeoutBrowserTestBase::histogram_tester();
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

struct TestTimeoutType {
  std::string test_case_name;
  int32_t timeout;

  // Returns true if `timeout` is too short for the test browser to complete a
  // navigation.
  // Empirically picking 1 second here.
  bool IsShortTimeout() const { return timeout < 1000 && timeout >= 0; }
};

// Tests to cover PendingBeacon's backgroundTimeout & timeout behaviors when
// BackForwardCache is off.
//
// Disables BackForwardCache by setting its cache size to 0 such that a page is
// discarded right away on user navigating to another page. And on page
// discard, pending beacons should be sent out no matter what value its
// backgroundTimeout/timeout is.
class PendingBeaconTimeoutNoBackForwardCacheBrowserTest
    : public PendingBeaconTimeoutBrowserTestBase,
      public testing::WithParamInterface<TestTimeoutType> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kPendingBeaconAPI, {{"send_on_navigation", "true"}}},
        {features::kBackForwardCache, {{"cache_size", "0"}}}};
    return enabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PendingBeaconTimeoutNoBackForwardCacheBrowserTest,
    testing::ValuesIn<std::vector<TestTimeoutType>>({
        {"LongTimeout", 600000},
        {"OneSecondTimeout", 1000},
        {"ShortTimeout", 1},
        {"ZeroTimeout", 0},
        {"DefaultTimeout", -1},        // default.
        {"NegativeTimeout", -600000},  // behaves the same as default.
    }),
    [](const testing::TestParamInfo<TestTimeoutType>& info) {
      return info.param.test_case_name;
    });

IN_PROC_BROWSER_TEST_P(PendingBeaconTimeoutNoBackForwardCacheBrowserTest,
                       SendOnPageDiscardNotUsingBackgroundTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with various backgroundTimeout, which should all
  // be sent on page A discard.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: $2});
  )",
                                    kBeaconEndpoint, GetParam().timeout));
  ASSERT_TRUE(WaitUntilPreviousDocumentDeleted());

  // The beacon should have been sent out after the page is gone.
  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  // As "send_on_navigation" is on, beacons are not send on page discard.
  // TODO(crbug.com/1378833): Update to `kSendAllOnHostDestroy` once completed.
  ExpectSendByBrowserBatchAction(
      PendingBeaconHost::BatchAction::kSendAllOnNavigation, total_beacon);
}

IN_PROC_BROWSER_TEST_P(PendingBeaconTimeoutNoBackForwardCacheBrowserTest,
                       SendOnPageDiscardNotUsingLongerTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with various timeout, which should all be sent on
  // page A discard.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: $2});
  )",
                                    kBeaconEndpoint, GetParam().timeout));
  ASSERT_TRUE(WaitUntilPreviousDocumentDeleted());

  // The beacon should have been sent out after the page is gone.
  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  if (GetParam().IsShortTimeout()) {
    // If timeout is too short, beacon sending **may** complete before
    // navigating away from page A, which means it won't be triggered by the
    // "send on page discard" batch action but by the sendNow() from renderer.
    ExpectActions({{PendingBeaconHost::Action::kCreate, total_beacon},
                   // No guarantee to be sent by single beacon's sendNow().
                   // {PendingBeaconHost::Action::kSend, 0},
                   {PendingBeaconHost::Action::kNetworkSend, total_beacon},
                   // No guarantee RFH can be alive when receiving response.
                   // {PendingBeaconHost::Action::kNetworkComplete, count},
                   {PendingBeaconHost::Action::kDelete, 0}});
    // No guarantee it won't be send on navigation.
    ExpectBatchActions(
        {{PendingBeaconHost::BatchAction::kSendAllOnProcessExit, 0},
         {PendingBeaconHost::BatchAction::kSendAllOnHostDestroy, 0}});
  } else {
    // As "send_on_navigation" is on, beacons are not send on page discard.
    // TODO(crbug.com/1378833): Update to `kSendAllOnHostDestroy` once
    // completed.
    ExpectSendByBrowserBatchAction(
        PendingBeaconHost::BatchAction::kSendAllOnNavigation, total_beacon);
  }
}

// Tests to cover PendingBeacon's backgroundTimeout behaviors.
// Setting a long `PendingBeaconMaxBackgroundTimeoutInMs` (1min) > BFCache
// timeout (5s) so that beacon sending cannot be caused by reaching max
// background timeout limit but only by BFCache eviction if backgroundTimeout
// set >= 5s.
class PendingBeaconBackgroundTimeoutBrowserTest
    : public PendingBeaconTimeoutBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kPendingBeaconAPI,
         {{"PendingBeaconMaxBackgroundTimeoutInMs", "60000"},
          // Don't force sending out beacons on pagehide.
          {"send_on_navigation", "false"}}},
        {features::kBackForwardCache,
         {{"TimeToLiveInBackForwardCacheInSeconds", "5"}}},
        // Forces BFCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(PendingBeaconBackgroundTimeoutBrowserTest,
                       SendOnHiddenAfterNavigation) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with 0s backgroundTimeout.
  // It should be sent out right on entering `hidden` state after navigating
  // away from A.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 0});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  // Triggered by backgroundTimeout's 0s timer from renderer.
  ExpectSendByRendererAction(total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconBackgroundTimeoutBrowserTest,
                       SendOnBackgroundTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with backgroundTimeout (1s) < BFCache TTL (5s).
  // The beacon should be sent out on entering `hidden` state but before
  // page deletion.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 1000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  // Triggered by backgroundTimeout's timer from renderer.
  ExpectSendByRendererAction(total_beacon);
}

// When backgroundTimeout is set, its timer resets every time when the page
// becomes visible if it has not yet expired.
IN_PROC_BROWSER_TEST_F(
    PendingBeaconBackgroundTimeoutBrowserTest,
    NotSendWhenPageIsRestoredBeforeBackgroundTimeoutExpires) {
  const size_t total_beacon = 0;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with backgroundTimeout (3s) < BFCache TTL (5s).
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 3000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The page A becomes visible again, so backgroundTimeout timer should stop.
  ASSERT_THAT(current_document(), IsFrameVisible());

  // Verify that beacon is not sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconBackgroundTimeoutBrowserTest,
                       SendOnBackForwardCacheEviction) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with backgroundTimeout (8s) > BFCache TTL (5s)
  // The beacon should be sent out on page deletion.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 8000});
  )",
                                    kBeaconEndpoint));
  ASSERT_TRUE(previous_document().WaitUntilRenderFrameDeleted());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconBackgroundTimeoutBrowserTest,
                       SendMultipleOnBackgroundTimeout) {
  const size_t total_beacon = 5;
  RegisterBeaconRequestMonitor(total_beacon);

  RunScriptInANavigateToB(JsReplace(R"(
    let p1 = new PendingGetBeacon($1, {backgroundTimeout: 200});
    let p2 = new PendingGetBeacon($1, {backgroundTimeout: 100});
    let p3 = new PendingGetBeacon($1, {backgroundTimeout: 500});
    let p4 = new PendingGetBeacon($1, {backgroundTimeout: 700});
    let p5 = new PendingGetBeacon($1, {backgroundTimeout: 300});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  // Triggered by backgroundTimeout's timer from renderer.
  ExpectSendByRendererAction(total_beacon);
}

// Tests to cover PendingBeacon's timeout behaviors.
// Sets `BeaconTimeToLiveInMs` (10s) > BFCache timeout (5s) to also cover beacon
// sending on page eviction.
using PendingBeaconTimeoutBrowserTest =
    PendingBeaconBackgroundTimeoutBrowserTest;

IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest, SendOnZeroTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with 0s timeout. It should be sent out right away
  // (without the page entering 'hidden' state).
  RunScriptInA(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: 0});
  )",
                         kBeaconEndpoint));
  ASSERT_THAT(current_document(), IsFrameVisible());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// When timeout is set, it's not relevant whether the page is hidden or not.
IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest,
                       SendOnTimeoutWhenPageIsHidden) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with a timeout which should expire when the page A
  // is still hidden.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: 1000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  // Verify that beacon is sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// When timeout is set, it's not relevant whether the page is visible or not.
IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest,
                       SendOnTimeoutWhenPageIsVisible) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with a timeout longer enough such that page can
  // experience visible -> hidden -> visible.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: 4000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());
  // beacon is not yet sent.
  ASSERT_EQ(sent_beacon_count(), 0u);

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The page A becomes visible again, but timeout timer never stops.
  ASSERT_THAT(current_document(), IsFrameVisible());

  WaitForAllBeaconsSent(total_beacon);
  // Verify that beacon is sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest, SendOnShorterTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with long (5s) timeout. And then quickly updates
  // to a very short (0.01s) timeout. The beacon should be sent out right away.
  RunScriptInA(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: 5000});
    p.timeout = 10;
  )",
                         kBeaconEndpoint));
  ASSERT_THAT(current_document(), IsFrameVisible());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest, SendOnlyOnce) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon which should be sent out right way.
  // But it won't be sent out twice.
  RunScriptInA(JsReplace(R"(
    let p = new PendingGetBeacon($1, {timeout: 0});
    p.timeout = 1;
  )",
                         kBeaconEndpoint));
  ASSERT_THAT(current_document(), IsFrameVisible());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconTimeoutBrowserTest, SendMultipleOnTimeout) {
  const size_t total_beacon = 5;
  RegisterBeaconRequestMonitor(total_beacon);

  RunScriptInA(JsReplace(R"(
    let p1 = new PendingGetBeacon($1, {timeout: 200});
    let p2 = new PendingGetBeacon($1, {timeout: 100});
    let p3 = new PendingGetBeacon($1, {timeout: 500});
    let p4 = new PendingGetBeacon($1, {timeout: 700});
    let p5 = new PendingGetBeacon($1, {timeout: 300});
  )",
                         kBeaconEndpoint));
  ASSERT_THAT(current_document(), IsFrameVisible());

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// Tests to cover PendingBeacon's backgroundTimeout & timeout mutual behaviors.
// Sets a long BFCache timeout (1min) so that beacon won't be sent out due to
// page eviction.
class PendingBeaconMutualTimeoutWithLongBackForwardCacheTTLBrowserTest
    : public PendingBeaconWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kPendingBeaconAPI,
         {// Don't force sending out beacons on pagehide.
          {"send_on_navigation", "false"}}},
        {features::kBackForwardCache,
         {{"TimeToLiveInBackForwardCacheInSeconds", "60"}}},
        // Forces BFCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    PendingBeaconMutualTimeoutWithLongBackForwardCacheTTLBrowserTest,
    NotSendWhenPageIsRestoredBeforeBeingEvictedFromBackForwardCache) {
  const size_t total_beacon = 0;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with default backgroundTimeout & timeout.
  // It should not be sent out as long as the page is alive (not evicted from
  // BackForwardCache).
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1);
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());
  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The same page A is still alive.
  ExpectRestored(FROM_HERE);

  // Verify that beacon is not sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// When both backgroundTimeout & timeout is set, whichever expires earlier will
// trigger beacon sending (part 1).
IN_PROC_BROWSER_TEST_F(
    PendingBeaconMutualTimeoutWithLongBackForwardCacheTTLBrowserTest,
    SendOnEarlierTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with long backgroundTimeout (60s) & short
  // timeout (1s).
  // The shorter one, i.e. timeout, should be reachable such that the beacon
  // can be sent before this test case times out.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 60000, timeout: 1000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  // Verify that beacon is sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// When both backgroundTimeout & timeout is set, whichever expires earlier will
// trigger beacon sending (part 2).
IN_PROC_BROWSER_TEST_F(
    PendingBeaconMutualTimeoutWithLongBackForwardCacheTTLBrowserTest,
    SendOnEarlierBackgroundTimeout) {
  const size_t total_beacon = 1;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon with short backgroundTimeout (1s) & long
  // timeout (60s).
  // The shorter one, i.e. backgroundTimeout, should be reachable such that the
  // beacon can be sent before this test case times out.
  RunScriptInANavigateToB(JsReplace(R"(
    let p = new PendingGetBeacon($1, {backgroundTimeout: 1000, timeout: 60000});
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  WaitForAllBeaconsSent(total_beacon);
  // Verify that beacon is sent.
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

// Tests to cover PendingBeacon's behaviors when enabled forced sending on
// pagehide event.
//
// Setting a long `PendingBeaconMaxBackgroundTimeoutInMs` (1min), and a long
// BFCache timeout (1min) so that beacon sending cannot be caused by reaching
// max background timeout limit, and cannot be caused by BFCache eviction.
class PendingBeaconSendOnPagehideBrowserTest
    : public PendingBeaconWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kPendingBeaconAPI,
         {{"PendingBeaconMaxBackgroundTimeoutInMs", "60000"},
          {"send_on_navigation", "true"}}},
        {features::kBackForwardCache,
         {{"TimeToLiveInBackForwardCacheInSeconds", "60"}}},
        // Forces BFCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(PendingBeaconSendOnPagehideBrowserTest,
                       SendOnPagehideWhenPageIsPersisted) {
  const size_t total_beacon = 3;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates 3 pending beacons with default backgroundTimeout & timeout.
  // They should be sent out on transitioning to pagehide event.
  RunScriptInANavigateToB(JsReplace(R"(
    document.title = '';
    let p1 = new PendingGetBeacon($1);
    let p2 = new PendingPostBeacon($1);
    let p3 = new PendingGetBeacon($1);
    window.addEventListener('pagehide', (e) => {
      document.title = e.persisted + '/' + p1.pending + '/' + p2.pending +
                       '/' + p3.pending;
    });
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The same page A is still alive.
  ExpectRestored(FROM_HERE);
  // All beacons should have been sent out before previous pagehide.
  std::u16string expected_title = u"true/false/false/false";
  TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconSendOnPagehideBrowserTest,
                       SendOnPagehideBeforeBackgroundTimeout) {
  const size_t total_beacon = 3;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates 3 pending beacons with long backgroundTimeout < BFCache TTL (1min).
  // They should be sent out on transitioning to pagehide but before the end of
  // backgroundTimeout and before BFCache TTL.
  RunScriptInANavigateToB(JsReplace(R"(
    document.title = '';
    let p1 = new PendingGetBeacon($1, {backgroundTimeout: 20000});
    let p2 = new PendingPostBeacon($1, {backgroundTimeout: 15000});
    let p3 = new PendingGetBeacon($1, {backgroundTimeout: 10000});
    window.addEventListener('pagehide', (e) => {
      document.title = e.persisted + '/' + p1.pending + '/' + p2.pending +
                       '/' + p3.pending;
    });
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The same page A is still alive.
  ExpectRestored(FROM_HERE);
  // All beacons should have been sent out.
  std::u16string expected_title = u"true/false/false/false";
  TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

IN_PROC_BROWSER_TEST_F(PendingBeaconSendOnPagehideBrowserTest,
                       SendOnPagehideBeforeTimeout) {
  const size_t total_beacon = 3;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates 3 pending beacons with long timeout < BFCache TTL (1min).
  // They should be sent out on transitioning to pagehide but before the end of
  // timeout and before BFCache TTL.
  RunScriptInANavigateToB(JsReplace(R"(
    document.title = '';
    let p1 = new PendingGetBeacon($1, {timeout: 20000});
    let p2 = new PendingPostBeacon($1, {timeout: 10000});
    let p3 = new PendingGetBeacon($1, {timeout: 15000});
    window.addEventListener('pagehide', (e) => {
      document.title = e.persisted + '/' + p1.pending + '/' + p2.pending +
                       '/' + p3.pending;
    });
  )",
                                    kBeaconEndpoint));
  ASSERT_THAT(previous_document(), IsFrameHidden());

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // The same page A is still alive.
  ExpectRestored(FROM_HERE);
  // All beacons should have been sent out.
  std::u16string expected_title = u"true/false/false/false";
  TitleWatcher title_watcher(web_contents(), expected_title);
  EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
}

class PendingBeaconRendererProcessExitBrowserTest
    : public PendingBeaconTimeoutBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kPendingBeaconAPI, {}},
        // Forces BFCache to work in low memory device so that a page won't be
        // killed by normal page discard.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

#if BUILDFLAG(IS_MAC)
// Disabled due to failures on various Mac builders.
// TODO(crbug.com/1382713) Reenable the test.
#define MAYBE_SendAllOnProcessCrash DISABLED_SendAllOnProcessCrash
#else
#define MAYBE_SendAllOnProcessCrash SendAllOnProcessCrash
#endif
IN_PROC_BROWSER_TEST_F(PendingBeaconRendererProcessExitBrowserTest,
                       MAYBE_SendAllOnProcessCrash) {
  const size_t total_beacon = 2;
  RegisterBeaconRequestMonitor(total_beacon);

  // Creates a pending beacon that will only be sent on page discard or crash.
  // Make beacon creations within a Promise to ensure they can all be created
  // before executing next statement.
  RunScriptInA(JsReplace(R"(
    new Promise(resolve => {
      new PendingGetBeacon($1);
      new PendingPostBeacon($1);
      resolve();
    });
  )",
                         kBeaconEndpoint));
  ASSERT_EQ(sent_beacon_count(), 0u);
  ASSERT_TRUE(
      PendingBeaconHost::GetForCurrentDocument(current_document().get()));

  // Make the renderer crash.
  CrashTab(web_contents());
  // The RenderFrame is dead but the attached beacon host is still alive.
  ASSERT_FALSE(current_document()->IsRenderFrameLive());
  ASSERT_TRUE(
      PendingBeaconHost::GetForCurrentDocument(current_document().get()));

  WaitForAllBeaconsSent(total_beacon);
  EXPECT_EQ(sent_beacon_count(), total_beacon);
  ExpectSendByBrowserBatchAction(
      PendingBeaconHost::BatchAction::kSendAllOnProcessExit, total_beacon);
}

}  // namespace content
