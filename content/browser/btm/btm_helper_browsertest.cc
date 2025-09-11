// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_bounce_detector.h"
#include "content/browser/btm/btm_browsertest_utils.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/btm_service_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

using testing::Optional;
using testing::Pair;

namespace content {

class BtmTabHelperBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        {features::kBtm, {{"triggering_action", "bounce"}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    BtmWebContentsObserver::FromWebContents(GetActiveWebContents())
        ->SetClockForTesting(&test_clock_);
    browser_client_.emplace();

    browser_client_->impl().SetBlockThirdPartyCookiesByDefault(true);
    WebContents* web_contents = GetActiveWebContents();
    ASSERT_FALSE(btm::Are3PcsGenerallyEnabled(web_contents->GetBrowserContext(),
                                              web_contents));

    // We can only create extra browser contexts while single-threaded.
    extra_browser_context_ = CreateTestBrowserContext();
  }

  void TearDown() override {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          std::move(extra_browser_context_));
    ContentBrowserTest::TearDown();
  }

  WebContents* GetActiveWebContents() {
    if (!web_contents_) {
      web_contents_ = shell()->web_contents();
    }
    return web_contents_;
  }

  void SetBtmTime(base::Time time) { test_clock_.SetNow(time); }

  [[nodiscard]] bool NavigateToURLAndWaitForCookieWrite(const GURL& url) {
    URLCookieAccessObserver observer(GetActiveWebContents(), url,
                                     CookieAccessDetails::Type::kChange);
    bool success = NavigateToURL(GetActiveWebContents(), url);
    if (!success) {
      return false;
    }
    observer.Wait();
    return true;
  }

  base::Clock* test_clock() { return &test_clock_; }

  // Make GetActiveWebContents() return the given value instead of the default.
  // Helpful for tests that use other WebContents (e.g. in incognito windows).
  void OverrideActiveWebContents(WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  void EndRedirectChain() {
    WebContents* web_contents = GetActiveWebContents();
    BtmServiceImpl* btm_service =
        BtmServiceImpl::Get(web_contents->GetBrowserContext());
    GURL expected_url = web_contents->GetLastCommittedURL();

    BtmRedirectChainObserver chain_observer(btm_service, expected_url);
    // Performing a browser-based navigation terminates the current redirect
    // chain.
    ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                                "a.test", "/title1.html")));
    chain_observer.Wait();
  }

  BrowserContext* extra_browser_context() {
    return extra_browser_context_.get();
  }

 private:
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents_ = nullptr;
  // browser_client_ is wrapped in optional<> to delay construction -- it won't
  // be registered properly if it's created too early.
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
  base::SimpleTestClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestBrowserContext> extra_browser_context_;
};

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       InteractionsRecordedInAncestorFrames) {
  GURL url_a =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  GURL url_b = embedded_test_server()->GetURL("b.test", "/title1.html");
  const std::string kIframeId =
      "test_iframe";  // defined in page_with_blank_iframe.html
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1);
  WebContents* web_contents = GetActiveWebContents();

  // The top-level page is on a.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url_a));
  SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);

  // Before clicking, no BTM state for either site.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url_a).has_value());
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url_b).has_value());

  // Click on the a.test top-level site.
  SetBtmTime(time);
  UserActivationObserver observer_a(web_contents,
                                    web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_a.Wait();

  // User activation is recorded for a.test (the top-level frame).
  std::optional<StateValue> state_a =
      GetBtmState(GetBtmService(web_contents), url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_EQ(std::make_optional(time), state_a->user_activation_times->first);

  // Update the top-level page to have an iframe pointing to b.test.
  ASSERT_TRUE(NavigateIframeToURL(web_contents, kIframeId, url_b));
  RenderFrameHost* iframe =
      FrameMatchingPredicate(web_contents->GetPrimaryPage(),
                             base::BindRepeating(&FrameIsChildOfMainFrame));
  // Wait until we can click on the iframe.
  WaitForHitTestData(iframe);

  // Click on the b.test iframe.
  base::Time frame_interaction_time = time + kBtmTimestampUpdateInterval;
  SetBtmTime(frame_interaction_time);
  UserActivationObserver observer_b(web_contents, iframe);

  // TODO(crbug.com/40247129): Remove the ExecJs workaround once
  // SimulateMouseClickOrTapElementWithId is able to activate iframes on Android
#if !BUILDFLAG(IS_ANDROID)
  SimulateMouseClickOrTapElementWithId(web_contents, kIframeId);
#else
  ASSERT_TRUE(ExecJs(iframe, "// empty script to activate iframe"));
#endif
  observer_b.Wait();

  // User activation on the top-level is updated by interacting with b.test
  // (the iframe).
  state_a = GetBtmState(GetBtmService(web_contents), url_a);
  ASSERT_TRUE(state_a.has_value());
  EXPECT_EQ(std::make_optional(frame_interaction_time),
            state_a->user_activation_times->second);

  // The iframe site doesn't have any state.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url_b).has_value());
}

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       MultipleUserInteractionsRecorded) {
  GURL url = embedded_test_server()->GetURL("a.test", "/title1.html");
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1);
  WebContents* web_contents = GetActiveWebContents();

  SetBtmTime(time);
  // Navigate to a.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url));
  SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  WaitForHitTestData(frame);  // Wait until we can click.

  // Before clicking, there's no BTM state for the site.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url).has_value());

  UserActivationObserver observer1(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer1.Wait();

  // One instance of user activation is recorded.
  std::optional<StateValue> state_1 =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_TRUE(state_1.has_value());
  EXPECT_EQ(std::make_optional(time), state_1->user_activation_times->first);
  EXPECT_EQ(state_1->user_activation_times->first,
            state_1->user_activation_times->second);

  SetBtmTime(time + kBtmTimestampUpdateInterval + base::Seconds(10));
  UserActivationObserver observer_2(web_contents, frame);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer_2.Wait();

  // A second, different, instance of user activation is recorded for the same
  // site.
  std::optional<StateValue> state_2 =
      GetBtmState(GetBtmService(web_contents), url);
  ASSERT_TRUE(state_2.has_value());
  EXPECT_NE(state_2->user_activation_times->second,
            state_2->user_activation_times->first);
  EXPECT_EQ(std::make_optional(time), state_2->user_activation_times->first);
  EXPECT_EQ(std::make_optional(time + kBtmTimestampUpdateInterval +
                               base::Seconds(10)),
            state_2->user_activation_times->second);
}

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest, StorageRecordedInSingleFrame) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url_a =
      embedded_test_server()->GetURL("a.test", "/page_with_blank_iframe.html");
  GURL url_b = https_server.GetURL("b.test", "/title1.html");
  const std::string kIframeId =
      "test_iframe";  // defined in page_with_blank_iframe.html
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1);
  WebContents* web_contents = GetActiveWebContents();

  // The top-level page is on a.test, containing an iframe pointing at b.test.
  ASSERT_TRUE(NavigateToURL(web_contents, url_a));
  ASSERT_TRUE(NavigateIframeToURL(web_contents, kIframeId, url_b));

  RenderFrameHost* iframe =
      FrameMatchingPredicate(web_contents->GetPrimaryPage(),
                             base::BindRepeating(&FrameIsChildOfMainFrame));

  // Initially, no BTM state for either site.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url_a).has_value());
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), url_b).has_value());

  // Write a cookie in the b.test iframe.
  SetBtmTime(time);
  FrameCookieAccessObserver observer(web_contents, iframe,
                                     CookieAccessDetails::Type::kChange);
  ASSERT_TRUE(ExecJs(iframe,
                     "document.cookie = 'foo=bar; SameSite=None; Secure';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();

  // Nothing recorded for a.test (the top-level frame).
  std::optional<StateValue> state_a =
      GetBtmState(GetBtmService(web_contents), url_a);
  EXPECT_FALSE(state_a.has_value());
  // Nothing recorded for b.test (the iframe), since we don't record non main
  // frame URLs to BTM state.
  std::optional<StateValue> state_b =
      GetBtmState(GetBtmService(web_contents), url_b);
  EXPECT_FALSE(state_b.has_value());
}

namespace {
class BrowsingDataRemovalObserver : public BrowsingDataRemover::Observer {
 public:
  explicit BrowsingDataRemovalObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

bool ClearBrowsingData(BrowsingDataRemover* remover,
                       base::TimeDelta time_period) {
  base::RunLoop run_loop;
  BrowsingDataRemovalObserver observer(run_loop.QuitClosure());
  remover->AddObserver(&observer);
  const base::Time now = base::Time::Now();
  remover->RemoveAndReply(now - time_period, now,
                          ContentBrowserClient::kDefaultBtmRemoveMask |
                              TpcBlockingBrowserClient::DATA_TYPE_HISTORY,
                          BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
                          &observer);
  run_loop.Run();
  remover->RemoveObserver(&observer);
  return !testing::Test::HasFailure();
}
}  // namespace

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       ChromeBrowsingDataRemover_Basic) {
  WebContents* web_contents = GetActiveWebContents();
  base::Time interaction_time = base::Time::Now() - base::Seconds(10);
  SetBtmTime(interaction_time);

  // Perform a click to get a.test added to the BTM DB.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();

  // Verify it was added.
  std::optional<StateValue> state_initial =
      GetBtmState(GetBtmService(web_contents), GURL("http://a.test"));
  ASSERT_TRUE(state_initial.has_value());
  ASSERT_TRUE(state_initial->user_activation_times.has_value());
  EXPECT_EQ(state_initial->user_activation_times->first, interaction_time);

  // Remove browsing data for the past day.
  ASSERT_TRUE(ClearBrowsingData(
      GetActiveWebContents()->GetBrowserContext()->GetBrowsingDataRemover(),
      base::Days(1)));

  // Verify that the user activation has been cleared from the BTM DB.
  std::optional<StateValue> state_final =
      GetBtmState(GetBtmService(web_contents), GURL("http://a.test"));
  EXPECT_FALSE(state_final.has_value());
}

// Makes a long URL involving several stateful stateful bounces on b.test,
// ultimately landing on c.test. Returns both the full redirect URL and the URL
// for the landing page. The landing page URL has a param appended to it to
// ensure it's unique to URLs from previous calls (to prevent caching).
std::pair<GURL, GURL> MakeRedirectAndFinalUrl(net::EmbeddedTestServer* server) {
  uint64_t unique_value = base::RandUint64();
  std::string final_dest =
      base::StrCat({"/title1.html?i=", base::NumberToString(unique_value)});
  std::string redirect_path =
      "/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/"
      "b.test/cross-site-with-cookie/b.test/cross-site-with-cookie/d.test";
  redirect_path += final_dest;
  return std::make_pair(server->GetURL("b.test", redirect_path),
                        server->GetURL("d.test", final_dest));
}

// Attempt to detect flakiness in waiting for BTM storage by repeatedly
// visiting long redirect chains, deleting the relevant rows, and verifying the
// rows don't come back.
IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       DetectRedirectHandlingFlakiness) {
  WebContents* web_contents = GetActiveWebContents();

  auto* btm_storage =
      BtmServiceImpl::Get(web_contents->GetBrowserContext())->storage();

  for (int i = 0; i < 10; i++) {
    const base::Time bounce_time = base::Time::FromSecondsSinceUnixEpoch(i + 1);
    SetBtmTime(bounce_time);
    LOG(INFO) << "*** i=" << i << " ***";
    // Make b.test statefully bounce.
    ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                                "a.test", "/title1.html")));
    auto [redirect_url, final_url] =
        MakeRedirectAndFinalUrl(embedded_test_server());
    ASSERT_TRUE(
        NavigateToURLFromRenderer(web_contents, redirect_url, final_url));
    // End the chain so the bounce is recorded.
    EndRedirectChain();

    // Verify the bounces were recorded.
    std::optional<StateValue> b_state =
        GetBtmState(GetBtmService(web_contents), GURL("http://b.test"));
    ASSERT_TRUE(b_state.has_value());
    ASSERT_THAT(b_state->bounce_times,
                Optional(Pair(bounce_time, bounce_time)));

    btm_storage->AsyncCall(&BtmStorage::RemoveRows)
        .WithArgs(std::vector<std::string>{"b.test"});

    // Verify the row was removed before repeating the test. If we did not
    // correctly wait for the whole chain to be handled before removing the row
    // for b.test, it will likely be written again and this check will fail.
    // (And if a write happens after this check, it will include a stale
    // timestamp and will cause one the of the checks above to fail on the next
    // loop iteration.)
    ASSERT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://b.test"))
                     .has_value());
  }
}

// Flaky on Android: https://crbug.com/369717773
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_UserClearedSitesAreNotReportedToUKM \
  DISABLED_UserClearedSitesAreNotReportedToUKM
#else
#define MAYBE_UserClearedSitesAreNotReportedToUKM \
  UserClearedSitesAreNotReportedToUKM
#endif
IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       MAYBE_UserClearedSitesAreNotReportedToUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  WebContents* web_contents = GetActiveWebContents();
  BtmServiceImpl* btm_service =
      BtmServiceImpl::Get(web_contents->GetBrowserContext());
  // A time more than an hour ago.
  base::Time old_bounce_time = base::Time::Now() - base::Hours(2);
  // A time within the past hour.
  base::Time recent_bounce_time = base::Time::Now() - base::Minutes(10);

  SetBtmTime(old_bounce_time);
  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  const GURL bounce_url1 = embedded_test_server()->GetURL(
      "b.test", "/cross-site-with-cookie/d.test/title1.html");
  URLCookieAccessObserver observer1(web_contents, bounce_url1,
                                    CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents, bounce_url1,
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  observer1.Wait();
  // End the chain so the bounce is recorded.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  SetBtmTime(recent_bounce_time);
  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  const GURL bounce_url2 = embedded_test_server()->GetURL(
      "c.test", "/cross-site-with-cookie/d.test/title1.html");
  URLCookieAccessObserver observer2(web_contents, bounce_url2,
                                    CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents, bounce_url2,
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  observer2.Wait();
  EndRedirectChain();

  // Verify the bounces were recorded. b.test:
  std::optional<StateValue> state =
      GetBtmState(GetBtmService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(state.has_value());
  ASSERT_EQ(state->user_activation_times, std::nullopt);
  // c.test:
  state = GetBtmState(GetBtmService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(state.has_value());
  ASSERT_EQ(state->user_activation_times, std::nullopt);

  // Remove browsing data for the past hour. This should include c.test but not
  // b.test.
  ASSERT_TRUE(ClearBrowsingData(
      web_contents->GetBrowserContext()->GetBrowsingDataRemover(),
      base::Hours(1)));

  // Verify only the BTM record for c.test was deleted.
  ASSERT_TRUE(GetBtmState(GetBtmService(web_contents), GURL("http://b.test"))
                  .has_value());
  ASSERT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://c.test"))
                   .has_value());

  // Trigger the BTM timer which will delete tracker data.
  SetBtmTime(recent_bounce_time + features::kBtmGracePeriod.Get() +
             base::Milliseconds(1));
  btm_service->OnTimerFiredForTesting();
  btm_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that both BTM records are now gone.
  ASSERT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://b.test"))
                   .has_value());
  ASSERT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://c.test"))
                   .has_value());

  // Only b.test was reported to UKM.
  EXPECT_THAT(ukm_recorder, EntryUrlsAre("DIPS.Deletion", {"http://b.test/"}));
}

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest, SitesInOpenTabsAreExempt) {
  WebContents* web_contents = GetActiveWebContents();
  BtmServiceImpl* btm_service =
      BtmServiceImpl::Get(web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetBtmTime(bounce_time);

  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "c.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounces through b.test and c.test were recorded.
  std::optional<StateValue> b_state =
      GetBtmState(GetBtmService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(b_state.has_value());
  ASSERT_EQ(b_state->user_activation_times, std::nullopt);

  std::optional<StateValue> c_state =
      GetBtmState(GetBtmService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(c_state.has_value());
  ASSERT_EQ(c_state->user_activation_times, std::nullopt);

  // Open b.test in a new tab.
  auto new_tab = OpenInNewTab(
      web_contents, embedded_test_server()->GetURL("c.test", "/title1.html"));
  ASSERT_TRUE(new_tab.has_value()) << new_tab.error();

  // Navigate to c.test in the new tab.
  ASSERT_TRUE(NavigateToURL(
      *new_tab, embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Trigger the BTM timer which would delete tracker data.
  SetBtmTime(bounce_time + features::kBtmGracePeriod.Get() +
             base::Milliseconds(1));
  btm_service->OnTimerFiredForTesting();
  btm_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that the BTM record for b.test is now gone, because there is no
  // open tab on b.test.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://b.test"))
                   .has_value());

  // Verify that the BTM record for c.test is still present, because there is
  // an open tab on c.test.
  EXPECT_TRUE(GetBtmState(GetBtmService(web_contents), GURL("http://c.test"))
                  .has_value());
}

IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       SitesInDestroyedTabsAreNotExempt) {
  WebContents* web_contents = GetActiveWebContents();
  BtmServiceImpl* btm_service =
      BtmServiceImpl::Get(web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetBtmTime(bounce_time);

  // Make b.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "b.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounce through b.test was recorded.
  std::optional<StateValue> b_state =
      GetBtmState(GetBtmService(web_contents), GURL("http://b.test"));
  ASSERT_TRUE(b_state.has_value());
  ASSERT_EQ(b_state->user_activation_times, std::nullopt);

  // Open b.test in a new tab.
  auto new_tab = OpenInNewTab(
      web_contents, embedded_test_server()->GetURL("c.test", "/title1.html"));
  ASSERT_TRUE(new_tab.has_value()) << new_tab.error();

  // Close the new tab with b.test.
  CloseTab(*new_tab);

  // Trigger the BTM timer which would delete tracker data.
  SetBtmTime(bounce_time + features::kBtmGracePeriod.Get() +
             base::Milliseconds(1));
  btm_service->OnTimerFiredForTesting();
  btm_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // Verify that the BTM record for b.test is now gone, because there is no
  // open tab on b.test.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://b.test"))
                   .has_value());
}

// Multiple running profiles is not supported on Android or ChromeOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BtmTabHelperBrowserTest,
                       SitesInOpenTabsForDifferentProfilesAreNotExempt) {
  WebContents* web_contents = GetActiveWebContents();
  BtmServiceImpl* btm_service =
      BtmServiceImpl::Get(web_contents->GetBrowserContext());

  // A time within the past hour.
  base::Time bounce_time = base::Time::Now() - base::Minutes(10);
  SetBtmTime(bounce_time);

  // Make c.test statefully bounce to d.test.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));
  ASSERT_TRUE(NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL(
          "c.test", "/cross-site-with-cookie/d.test/title1.html"),
      embedded_test_server()->GetURL("d.test", "/title1.html")));
  EndRedirectChain();

  // Verify the bounce through c.test was recorded.
  std::optional<StateValue> c_state =
      GetBtmState(GetBtmService(web_contents), GURL("http://c.test"));
  ASSERT_TRUE(c_state.has_value());
  ASSERT_EQ(c_state->user_activation_times, std::nullopt);

  // Open c.test on a new tab in a new window/profile.
  Shell* another_window = Shell::CreateNewWindow(
      extra_browser_context(), GURL(url::kAboutBlankURL), nullptr, gfx::Size());
  ASSERT_TRUE(
      NavigateToURL(another_window->web_contents(),
                    embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Trigger the BTM timer which would delete tracker data.
  SetBtmTime(bounce_time + features::kBtmGracePeriod.Get() +
             base::Milliseconds(1));
  btm_service->OnTimerFiredForTesting();
  btm_service->storage()->FlushPostedTasksForTesting();
  base::RunLoop().RunUntilIdle();

  // The BTM record for c.test was removed, because open tabs in a different
  // profile are not exempt.
  EXPECT_FALSE(GetBtmState(GetBtmService(web_contents), GURL("http://c.test"))
                   .has_value());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

}  // namespace content
