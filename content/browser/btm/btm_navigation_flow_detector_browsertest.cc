// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_navigation_flow_detector.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_browsertest_utils.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#endif

namespace content {

namespace {

using AttributionData = std::set<AttributionDataModel::DataKey>;

std::vector<url::Origin> GetOrigins(const AttributionData& data) {
  std::vector<url::Origin> origins;
  std::ranges::transform(data, std::back_inserter(origins),
                         &AttributionDataModel::DataKey::reporting_origin);
  return origins;
}

std::string StringifyBooleanMetric(ukm::TestAutoSetUkmRecorder* ukm_recorder,
                                   const ukm::mojom::UkmEntry* entry,
                                   std::string metric_name) {
  const std::int64_t* metric = ukm_recorder->GetEntryMetric(entry, metric_name);
  if (metric == nullptr) {
    return "null";
  }
  return base::ToString(*metric);
}

const std::string StringifyNumericMetric(
    ukm::TestAutoSetUkmRecorder* ukm_recorder,
    const ukm::mojom::UkmEntry* entry,
    std::string metric_name) {
  const std::int64_t* metric = ukm_recorder->GetEntryMetric(entry, metric_name);
  if (metric == nullptr) {
    return "null";
  }
  return base::NumberToString(*metric);
}

std::string StringifyNavigationFlowNodeEntry(
    ukm::TestAutoSetUkmRecorder* ukm_recorder,
    const ukm::mojom::UkmEntry* entry) {
  return base::StringPrintf(
      "source url: %s, metrics: {\n"
      " WerePreviousAndNextSiteSame: %s\n"
      " DidHaveUserActivation: %s\n"
      " DidHaveSuccessfulWAA: %s\n"
      " WasEntryUserInitiated: %s\n"
      " WasExitUserInitiated: %s\n"
      " WereEntryAndExitRendererInitiated: %s\n"
      " VisitDurationMilliseconds: %s\n"
      "}",
      ukm_recorder->GetSourceForSourceId(entry->source_id)
          ->url()
          .spec()
          .c_str(),
      StringifyBooleanMetric(ukm_recorder, entry,
                             "WerePreviousAndNextSiteSame"),
      StringifyBooleanMetric(ukm_recorder, entry, "DidHaveUserActivation"),
      StringifyBooleanMetric(ukm_recorder, entry, "DidHaveSuccessfulWAA"),
      StringifyBooleanMetric(ukm_recorder, entry, "WasEntryUserInitiated"),
      StringifyBooleanMetric(ukm_recorder, entry, "WasExitUserInitiated"),
      StringifyBooleanMetric(ukm_recorder, entry,
                             "WereEntryAndExitRendererInitiated"),
      StringifyNumericMetric(ukm_recorder, entry, "VisitDurationMilliseconds")
          .c_str());
}

std::string_view kNavigationFlowNodeUkmEventName = "DIPS.NavigationFlowNode";
std::string_view kSuspectedTrackerFlowReferrerUkmEventName =
    "DIPS.SuspectedTrackerFlowReferrerV2";
std::string_view kSuspectedTrackerFlowEntrypointUkmEventName =
    "DIPS.SuspectedTrackerFlowEntrypointV2";
std::string_view kInFlowInteractionUkmEventName =
    "DIPS.TrustIndicator.InFlowInteractionV2";
std::string_view kInFlowSuccessorInteractionUkmEventName =
    "DIPS.TrustIndicator.InFlowSuccessorInteraction";
std::string_view kDirectNavigationUkmEventName =
    "DIPS.TrustIndicator.DirectNavigationV2";
std::string_view kSiteA = "a.test";
std::string_view kSiteB = "b.test";
std::string_view kSiteC = "c.test";
std::string_view kSiteD = "d.test";
}  // namespace

class BtmNavigationFlowDetectorTest : public ContentBrowserTest {
 public:
  BtmNavigationFlowDetectorTest()
      : embedded_https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~BtmNavigationFlowDetectorTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    embedded_https_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server_.Start());

    ukm_recorder_.emplace();

    SetTestClock();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(GetActiveWebContents());
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

 protected:
  // TODO: crbug.com/1509946 - When embedded_https_test_server() is added to
  // AndroidBrowserTest, switch to using
  // PlatformBrowserTest::embedded_https_test_server() and delete this.
  net::EmbeddedTestServer embedded_https_test_server_;
  base::SimpleTestClock test_clock_;

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return ukm_recorder_.value(); }

  void ExpectNoNavigationFlowNodeUkmEvents() {
    auto ukm_entries =
        ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
    EXPECT_TRUE(ukm_entries.empty())
        << "UKM entry count was " << ukm_entries.size()
        << ". First UKM entry below.\n"
        << StringifyNavigationFlowNodeEntry(&ukm_recorder(), ukm_entries.at(0));
  }

  void ExpectNoUkmEventsOfType(std::string_view event_name) {
    auto ukm_entries = ukm_recorder().GetEntriesByName(event_name);
    EXPECT_TRUE(ukm_entries.empty());
  }

  GURL GetSetCookieUrlForSite(std::string_view site) {
    // Path set in btm_test_utils.cc's NavigateToSetCookie().
    return embedded_https_test_server_.GetURL(site, "/set-cookie?name=value");
  }

  [[nodiscard]] testing::AssertionResult
  NavigateToSetCookieAndAwaitAccessNotification(WebContents* web_contents,
                                                std::string_view site) {
    URLCookieAccessObserver observer(
        web_contents, GetSetCookieUrlForSite(site),
        network::mojom::CookieAccessDetails_Type::kChange);
    bool success = NavigateToSetCookie(
        web_contents, &embedded_https_test_server_, site, false, false);
    if (success) {
      observer.Wait();
    }
    return testing::AssertionResult(success);
  }

  [[nodiscard]] testing::AssertionResult WaitUntilTransientActivationLost(
      RenderFrameHost* rfh,
      base::TimeDelta timeout) {
    const base::Time start_time;
    while (rfh->HasTransientUserActivation()) {
      if (base::Time() - start_time >= timeout) {
        return testing::AssertionFailure()
               << "Timed out waiting for the RFH to lose transient activation "
                  "after "
               << timeout.InSeconds() << " seconds";
      }
      Sleep(base::Milliseconds(50));
    }
    return testing::AssertionSuccess();
  }

  BtmNavigationFlowDetector* GetDetector() {
    return BtmNavigationFlowDetector::FromWebContents(GetActiveWebContents());
  }

  void SimulateBookmarkNavigation(WebContents* web_contents, const GURL& url) {
    NavigationController::LoadURLParams navigation_params(url);
    navigation_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    NavigateToURLBlockUntilNavigationsComplete(web_contents, navigation_params,
                                               1);
  }

 private:
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  void SetTestClock() { GetDetector()->SetClockForTesting(&test_clock_); }

  static void Sleep(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }
};

class BtmNavigationFlowDetectorPrerenderTest
    : public BtmNavigationFlowDetectorTest {
 public:
  BtmNavigationFlowDetectorPrerenderTest() {
    prerender_test_helper_ =
        std::make_unique<test::PrerenderTestHelper>(base::BindRepeating(
            &BtmNavigationFlowDetectorTest::GetActiveWebContents,
            base::Unretained(this)));
  }
  ~BtmNavigationFlowDetectorPrerenderTest() override = default;

  void SetUpOnMainThread() override {
    prerender_test_helper_->RegisterServerRequestMonitor(
        embedded_https_test_server_);
    BtmNavigationFlowDetectorTest::SetUpOnMainThread();
  }

 protected:
  test::PrerenderTestHelper* prerender_test_helper() {
    return prerender_test_helper_.get();
  }

 private:
  std::unique_ptr<test::PrerenderTestHelper> prerender_test_helper_;
};

class BtmNavigationFlowDetectorPATApiTest
    : public BtmNavigationFlowDetectorTest {
 public:
  BtmNavigationFlowDetectorPATApiTest() {
    // Enable Privacy Sandbox APIs on all sites.
    scoped_feature_list_.InitWithFeatures(
        {features::kPrivacySandboxAdsAPIsOverride}, {});
  }

  void SetUpOnMainThread() override {
    RegisterTrustTokenTestHandler(&trust_token_request_handler_);
    browser_client_.emplace();
    browser_client().SetBlockThirdPartyCookiesByDefault(true);
    BtmNavigationFlowDetectorTest::SetUpOnMainThread();
  }

  base::expected<std::vector<url::Origin>, std::string>
  WaitForInterestGroupData() {
    WebContents* web_contents = GetActiveWebContents();
    InterestGroupManager* interest_group_manager =
        web_contents->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager();
    if (!interest_group_manager) {
      return base::unexpected("null interest group manager");
    }
    // Poll until data appears, failing if action_timeout() passes
    base::Time deadline = base::Time::Now() + TestTimeouts::action_timeout();
    while (base::Time::Now() < deadline) {
      base::test::TestFuture<std::vector<url::Origin>> future;
      interest_group_manager->GetAllInterestGroupJoiningOrigins(
          future.GetCallback());
      std::vector<url::Origin> data = future.Get();
      if (!data.empty()) {
        return data;
      }
      Sleep(TestTimeouts::tiny_timeout());
    }
    return base::unexpected("timed out waiting for interest group data");
  }

  base::expected<AttributionData, std::string> WaitForAttributionData() {
    WebContents* web_contents = GetActiveWebContents();
    AttributionDataModel* model = web_contents->GetBrowserContext()
                                      ->GetDefaultStoragePartition()
                                      ->GetAttributionDataModel();
    if (!model) {
      return base::unexpected("null attribution data model");
    }
    // Poll until data appears, failing if action_timeout() passes
    base::Time deadline = base::Time::Now() + TestTimeouts::action_timeout();
    while (base::Time::Now() < deadline) {
      base::test::TestFuture<AttributionData> future;
      model->GetAllDataKeys(future.GetCallback());
      AttributionData data = future.Get();
      if (!data.empty()) {
        return data;
      }
      Sleep(TestTimeouts::tiny_timeout());
    }
    return base::unexpected("timed out waiting for attribution data");
  }

  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      std::vector<std::string_view> hosts) {
    base::flat_map<url::Origin, std::string_view> origins_and_commitments;
    std::string key_commitments =
        trust_token_request_handler_.GetKeyCommitmentRecord();

    for (std::string_view host : hosts) {
      origins_and_commitments.insert_or_assign(
          embedded_https_test_server_.GetOrigin(std::string(host)),
          key_commitments);
    }

    if (origins_and_commitments.empty()) {
      origins_and_commitments = {
          {embedded_https_test_server_.GetOrigin(), key_commitments}};
    }

    base::RunLoop run_loop;
    GetNetworkService()->SetTrustTokenKeyCommitments(
        network::WrapKeyCommitmentsForIssuers(
            std::move(origins_and_commitments)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  TpcBlockingBrowserClient& browser_client() { return browser_client_->impl(); }

 private:
  void RegisterTrustTokenTestHandler(
      network::test::TrustTokenRequestHandler* handler) {
    embedded_https_test_server_.RegisterRequestHandler(
        base::BindLambdaForTesting(
            [handler, this](const net::test_server::HttpRequest& request)
                -> std::unique_ptr<net::test_server::HttpResponse> {
              if (request.relative_url != "/issue") {
                return nullptr;
              }
              if (!base::Contains(request.headers, "Sec-Private-State-Token") ||
                  !base::Contains(request.headers,
                                  "Sec-Private-State-Token-Crypto-Version")) {
                return MakeTrustTokenFailureResponse();
              }

              std::optional<std::string> operation_result =
                  handler->Issue(request.headers.at("Sec-Private-State-Token"));

              if (!operation_result) {
                return MakeTrustTokenFailureResponse();
              }

              return MakeTrustTokenResponse(*operation_result);
            }));
  }

  std::unique_ptr<net::test_server::HttpResponse>
  MakeTrustTokenFailureResponse() {
    // No need to report a failure HTTP code here: returning a vanilla OK should
    // fail the Trust Tokens operation client-side.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  // Constructs and returns an HTTP response bearing the given base64-encoded
  // Trust Tokens issuance or redemption protocol response message.
  std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
      std::string_view contents) {
    std::string temp;
    CHECK(base::Base64Decode(contents, &temp));

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Sec-Private-State-Token", std::string(contents));
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return response;
  }

  static void Sleep(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  network::test::TrustTokenRequestHandler trust_token_request_handler_;
  std::optional<ContentBrowserTestTpcBlockingBrowserClient> browser_client_;
};

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       DirectNavigationEmittedForTypedUrl) {
  WebContents* web_contents = GetActiveWebContents();
  GURL url = embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kDirectNavigationUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURL(web_contents, url));
  ukm_loop.Run();

  // Expect DirectNavigation UKM entry to be accurate.
  auto direct_navigation_entries =
      ukm_recorder().GetEntriesByName(kDirectNavigationUkmEventName);
  ASSERT_EQ(direct_navigation_entries.size(), 1u);
  auto direct_navigation_entry = direct_navigation_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(direct_navigation_entry, url);
  ukm_recorder().ExpectEntryMetric(
      direct_navigation_entry, "NavigationSource",
      static_cast<int64_t>(btm::DirectNavigationSource::kOmnibar));
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       DirectNavigationEmittedForBookmark) {
  WebContents* web_contents = GetActiveWebContents();
  GURL url = embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kDirectNavigationUkmEventName,
                                       ukm_loop.QuitClosure());
  SimulateBookmarkNavigation(web_contents, url);
  ukm_loop.Run();

  // Expect DirectNavigation UKM entry to be accurate.
  auto direct_navigation_entries =
      ukm_recorder().GetEntriesByName(kDirectNavigationUkmEventName);
  ASSERT_EQ(direct_navigation_entries.size(), 1u);
  auto direct_navigation_entry = direct_navigation_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(direct_navigation_entry, url);
  ukm_recorder().ExpectEntryMetric(
      direct_navigation_entry, "NavigationSource",
      static_cast<int64_t>(btm::DirectNavigationSource::kBookmark));
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       DirectNavigationEmittedForServerRedirect) {
  WebContents* web_contents = GetActiveWebContents();
  GURL redirector_url = embedded_https_test_server_.GetURL(
      kSiteA, "/cross-site-with-cookie/b.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kDirectNavigationUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURL(web_contents, redirector_url, final_url));
  ukm_loop.Run();

  // Expect DirectNavigation UKM entry to be accurate.
  auto direct_navigation_entries =
      ukm_recorder().GetEntriesByName(kDirectNavigationUkmEventName);
  ASSERT_EQ(direct_navigation_entries.size(), 1u);
  auto direct_navigation_entry = direct_navigation_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(direct_navigation_entry,
                                         redirector_url);
  ukm_recorder().ExpectEntryMetric(
      direct_navigation_entry, "NavigationSource",
      static_cast<int64_t>(btm::DirectNavigationSource::kOmnibar));
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       DirectNavigationNotEmittedWhenNoPageCommits) {
  WebContents* web_contents = GetActiveWebContents();
  GURL url = embedded_https_test_server_.GetURL(kSiteA, "/page204.html");
  base::RunLoop ukm_loop;
  ASSERT_TRUE(NavigateToURL(web_contents, url, GURL("")));

  ExpectNoUkmEventsOfType(kDirectNavigationUkmEventName);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       DirectNavigationNotEmittedForLinkClick) {
  WebContents* web_contents = GetActiveWebContents();
  GURL initial_url = embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kDirectNavigationUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURL(web_contents, initial_url));
  ukm_loop.Run();
  GURL link_target_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, link_target_url));

  // Expect DirectNavigation UKM entry to be accurate.
  auto direct_navigation_entries =
      ukm_recorder().GetEntriesByName(kDirectNavigationUkmEventName);
  ASSERT_EQ(direct_navigation_entries.size(), 1u);
  auto direct_navigation_entry = direct_navigation_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(direct_navigation_entry, initial_url);
  ukm_recorder().ExpectEntryMetric(
      direct_navigation_entry, "NavigationSource",
      static_cast<int64_t>(btm::DirectNavigationSource::kOmnibar));
  // Implied assert: no DirectNavigation UKM entry for link_target_url.
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       SuspectedTrackerFlowEmittedForServerRedirectExit) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Visit B, which writes a cookie in the server response, and also server
  // redirects to C. Wait for cookie access to register and for UKM to emit.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site-with-cookie/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();
  ukm_loop.Run();

  // Expect referrer event to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 1u);
  auto referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(referrer_entry, referrer_url);
  const int64_t flow_id =
      *ukm_recorder().GetEntryMetric(referrer_entry, "FlowId");
  // Expect entrypoint event to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 1u);
  auto entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entrypoint_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kServer));
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "FlowId", flow_id);
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "HadActiveStorageAccess",
                                   true);
  // It's not possible to interact with a page that server-redirected.
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowDoesNotReportCookieReadsInNavigationsAsStorageAccess) {
  // Pre-write a cookie on B so that it gets sent later.
  WebContents* web_contents = GetActiveWebContents();
  GURL cookie_write_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, cookie_write_url));
  FrameCookieAccessObserver cookie_write_observer(
      web_contents, web_contents->GetPrimaryMainFrame(),
      CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(),
                     "document.cookie = 'name=value;';"));
  cookie_write_observer.Wait();
  // Visit A.
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Visit B, which is sent the prewritten cookie in the request, and have B
  // server redirect to C. Wait for cookie access to register and for UKM to
  // emit.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_read_observer(web_contents, entrypoint_url,
                                               CookieOperation::kRead);
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  cookie_read_observer.Wait();
  ukm_loop.Run();

  // Expect referrer event to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 1u);
  auto referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(referrer_entry, referrer_url);
  const int64_t flow_id =
      *ukm_recorder().GetEntryMetric(referrer_entry, "FlowId");
  // Expect entrypoint event to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 1u);
  auto entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entrypoint_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kServer));
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "FlowId", flow_id);
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "HadActiveStorageAccess",
                                   false);
  // It's not possible to interact with a page that server-redirected.
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowEmittedForServerRedirectExitConsecutiveEvents) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Visit B, which writes a cookie in the server response, and also server
  // redirects to C. Wait for cookie access to register and for UKM to emit.
  GURL first_entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site-with-cookie/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer_1(web_contents, first_entrypoint_url,
                                            CookieOperation::kChange);
  base::RunLoop ukm_loop_1;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop_1.QuitClosure());
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_entrypoint_url, final_url));
  cookie_observer_1.Wait();
  ukm_loop_1.Run();
  // Repeat a similar navigation pattern to generate a second set of UKM events.
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  GURL second_entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteD, "/cross-site-with-cookie/c.test/title1.html");
  URLCookieAccessObserver cookie_observer_2(web_contents, second_entrypoint_url,
                                            CookieOperation::kChange);
  base::RunLoop ukm_loop_2;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop_2.QuitClosure());
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, second_entrypoint_url, final_url));
  cookie_observer_2.Wait();
  ukm_loop_2.Run();

  // Expect referrer events to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 2u);
  auto first_referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(first_referrer_entry, referrer_url);
  const int64_t* first_flow_id =
      ukm_recorder().GetEntryMetric(first_referrer_entry, "FlowId");
  auto second_referrer_entry = referrer_entries.at(1);
  ukm_recorder().ExpectEntrySourceHasUrl(second_referrer_entry, referrer_url);
  const int64_t* second_flow_id =
      ukm_recorder().GetEntryMetric(second_referrer_entry, "FlowId");
  EXPECT_NE(first_flow_id, second_flow_id);
  // Expect entrypoint events to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 2u);
  auto first_entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(first_entrypoint_entry,
                                         first_entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      first_entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kServer));
  ukm_recorder().ExpectEntryMetric(first_entrypoint_entry, "FlowId",
                                   *first_flow_id);
  ukm_recorder().ExpectEntryMetric(first_entrypoint_entry,
                                   "HadActiveStorageAccess", true);
  auto second_entrypoint_entry = entrypoint_entries.at(1);
  ukm_recorder().ExpectEntrySourceHasUrl(second_entrypoint_entry,
                                         second_entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      second_entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kServer));
  ukm_recorder().ExpectEntryMetric(second_entrypoint_entry, "FlowId",
                                   *second_flow_id);
  ukm_recorder().ExpectEntryMetric(second_entrypoint_entry,
                                   "HadActiveStorageAccess", true);
  // It's not possible to interact with a page that server-redirected.
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

class BtmNavigationFlowDetectorClientRedirectTest
    : public BtmNavigationFlowDetectorTest,
      public testing::WithParamInterface<BtmClientRedirectMethod> {
 protected:
  BtmClientRedirectMethod client_redirect_type() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(
    BtmNavigationFlowDetectorClientRedirectTest,
    SuspectedTrackerFlowEmittedForClientRedirectWithInteraction) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Make A client-redirect to B, where B commits, then writes and reads cookies
  // with JS.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(
      ExecJs(frame, "document.cookie;", EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Interact with B.
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Make B client-redirect to C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop.QuitClosure());
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(
      PerformClientRedirect(client_redirect_type(), web_contents, final_url));
  ukm_loop.Run();

  // Expect referrer event to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 1u);
  auto referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(referrer_entry, referrer_url);
  const int64_t flow_id =
      *ukm_recorder().GetEntryMetric(referrer_entry, "FlowId");
  // Expect entrypoint event to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 1u);
  auto entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entrypoint_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kClient));
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "FlowId", flow_id);
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "HadActiveStorageAccess",
                                   true);
  // Expect InFlowInteraction to have been emitted appropriately.
  auto in_flow_interaction_entries =
      ukm_recorder().GetEntriesByName(kInFlowInteractionUkmEventName);
  ASSERT_EQ(in_flow_interaction_entries.size(), 1u);
  auto in_flow_interaction_entry = in_flow_interaction_entries.front();
  ukm_recorder().ExpectEntrySourceHasUrl(in_flow_interaction_entry,
                                         entrypoint_url);
  ukm_recorder().ExpectEntryMetric(in_flow_interaction_entry, "FlowId",
                                   flow_id);
}

IN_PROC_BROWSER_TEST_P(
    BtmNavigationFlowDetectorClientRedirectTest,
    SuspectedTrackerFlowEmittedForClientRedirectWithoutInteraction) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Make A client-redirect to B, where B commits and writes cookies with JS.
  // Don't generate user activation on B.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
  // Make B client-redirect to C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop.QuitClosure());
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(
      PerformClientRedirect(client_redirect_type(), web_contents, final_url));
  ukm_loop.Run();

  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
  // Expect referrer event to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 1u);
  auto referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(referrer_entry, referrer_url);
  const int64_t flow_id =
      *ukm_recorder().GetEntryMetric(referrer_entry, "FlowId");
  // Expect entrypoint event to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 1u);
  auto entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entrypoint_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kClient));
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "FlowId", flow_id);
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "HadActiveStorageAccess",
                                   true);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowNotEmittedWhenServerRedirectIsMultiHop) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Visit B, which writes a cookie in the server response, and performs a
  // multi-hop server redirect to C. Wait for cookie access to register.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB,
      "/cross-site-with-cookie/d.test/cross-site-with-cookie/c.test/"
      "title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowEmittedWhenRedirectDoesNotAccessCookies) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Visit B, which does not write a cookie in the server response, and also
  // server redirects to C.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(
      kSuspectedTrackerFlowEntrypointUkmEventName, ukm_loop.QuitClosure());
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  ukm_loop.Run();

  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
  // Expect referrer event to be accurate.
  auto referrer_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowReferrerUkmEventName);
  ASSERT_EQ(referrer_entries.size(), 1u);
  auto referrer_entry = referrer_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(referrer_entry, referrer_url);
  const int64_t flow_id =
      *ukm_recorder().GetEntryMetric(referrer_entry, "FlowId");
  // Expect entrypoint event to be accurate.
  auto entrypoint_entries = ukm_recorder().GetEntriesByName(
      kSuspectedTrackerFlowEntrypointUkmEventName);
  ASSERT_EQ(entrypoint_entries.size(), 1u);
  auto entrypoint_entry = entrypoint_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entrypoint_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(
      entrypoint_entry, "ExitRedirectType",
      static_cast<int64_t>(BtmRedirectType::kServer));
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "FlowId", flow_id);
  ukm_recorder().ExpectEntryMetric(entrypoint_entry, "HadActiveStorageAccess",
                                   false);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       SuspectedTrackerFlowNotEmittedForSameSiteReferral) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Have A client-redirect to another page on A, which writes a cookie in the
  // server response, and also server redirects to C. Wait for cookie access to
  // register.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteA, "/cross-site-with-cookie/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       SuspectedTrackerFlowNotEmittedForSameSiteServerExit) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Have A client-redirect to B, which writes a cookie in the server response,
  // and also server redirects to another page on B. Wait for cookie access to
  // register.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site-with-cookie/b.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowNotEmittedWhenReferralIsUserInitiated) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Simulate user-initiated navigation from A to B, which writes a cookie in
  // the server response, and also server redirects to C. Wait for cookie access
  // to register.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site-with-cookie/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  ASSERT_TRUE(
      NavigateToURLFromRenderer(web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowNotEmittedWhenReferralIsBrowserInitiated) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Simulate browser-initiated navigation from A to B, which writes a cookie in
  // the server response, and also server redirects to C. Wait for cookie access
  // to register and for UKM to emit.
  GURL entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteB, "/cross-site-with-cookie/c.test/title1.html");
  GURL final_url = embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  URLCookieAccessObserver cookie_observer(web_contents, entrypoint_url,
                                          CookieOperation::kChange);
  ASSERT_TRUE(NavigateToURL(web_contents, entrypoint_url, final_url));
  cookie_observer.Wait();

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    SuspectedTrackerFlowNotEmittedForSameSiteClientSideExit) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Make A client-redirect to B, where B commits and reads cookies with JS.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
  // Make B client-redirect to another page on B.
  GURL final_url = embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, final_url));

  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowReferrerUkmEventName);
  ExpectNoUkmEventsOfType(kSuspectedTrackerFlowEntrypointUkmEventName);
  ExpectNoUkmEventsOfType(kInFlowInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_P(BtmNavigationFlowDetectorClientRedirectTest,
                       InFlowSuccessorInteractionEmittedForAllClientRedirects) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for this flow.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(PerformClientRedirect(client_redirect_type(), web_contents,
                                    entrypoint_url));
  // Client-redirect to another page on B, the successor for this flow, and
  // interact with the page.
  GURL successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(PerformClientRedirect(client_redirect_type(), web_contents,
                                    successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the flow, and wait for UKM to emit.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(PerformClientRedirect(client_redirect_type(), web_contents,
                                    flow_end_url));
  ukm_loop.Run();

  // Expect InFlowSuccessorInteraction to have been emitted correctly.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kInFlowSuccessorInteractionUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "SuccessorRedirectIndex", 1);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidEntrypointAccessStorage",
                                   false);
}

IN_PROC_BROWSER_TEST_P(
    BtmNavigationFlowDetectorClientRedirectTest,
    InFlowSuccessorInteractionEmittedForMixOfClientAndServerRedirects) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for this flow.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(PerformClientRedirect(client_redirect_type(), web_contents,
                                    entrypoint_url));
  // Client-redirect to another page on B, which server-redirects to yet another
  // page on B (the successor for this flow), and interact with the page.
  GURL successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  GURL server_redirector_url = embedded_https_test_server_.GetURL(
      kSiteB, "/server-redirect?/title2.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, server_redirector_url, successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the flow, and wait for UKM to emit.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(PerformClientRedirect(client_redirect_type(), web_contents,
                                    flow_end_url));
  ukm_loop.Run();

  // Expect InFlowSuccessorInteraction to have been emitted correctly.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kInFlowSuccessorInteractionUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "SuccessorRedirectIndex", 2);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidEntrypointAccessStorage",
                                   false);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    InFlowSuccessorInteractionEmittedForMultipleSuccessorInteractions) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for this flow.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  // Client-redirect to another page on B, the first successor for this flow,
  // and interact with the page.
  GURL first_successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to yet another page on B, the second successor for this
  // flow, and interact with the page.
  GURL second_successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title3.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the flow, and wait for UKM to emit.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, flow_end_url));
  ukm_loop.Run();

  // Expect InFlowSuccessorInteraction entries to have been emitted correctly.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kInFlowSuccessorInteractionUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 2u);
  auto ukm_entry_1 = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry_1, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry_1, "SuccessorRedirectIndex", 1);
  ukm_recorder().ExpectEntryMetric(ukm_entry_1, "DidEntrypointAccessStorage",
                                   false);
  auto ukm_entry_2 = ukm_entries.at(1);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry_2, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry_2, "SuccessorRedirectIndex", 2);
  ukm_recorder().ExpectEntryMetric(ukm_entry_2, "DidEntrypointAccessStorage",
                                   false);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       InFlowSuccessorInteractionEmittedForConsecutiveFlows) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for the first flow.
  GURL first_entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_entrypoint_url));
  // Access cookies during the visit on the entrypoint.
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
  // Client-redirect to another page on B, the successor for the first flow, and
  // interact with the page.
  GURL first_successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          first_successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the first flow, and wait for UKM to emit.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop_1;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop_1.QuitClosure());
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, flow_end_url));
  ukm_loop_1.Run();
  // Client-redirect to a page on site D, the entrypoint for the second flow,
  // and server-redirect within site D.
  GURL second_entrypoint_url = embedded_https_test_server_.GetURL(
      kSiteD,
      "/server-redirect-with-secure-cookie?/server-redirect%3F/title1.html");
  GURL entrypoint_nav_commit_url =
      embedded_https_test_server_.GetURL(kSiteD, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, second_entrypoint_url, entrypoint_nav_commit_url));
  // Client-redirect to another page on site D, the successor for the second
  // flow, and interact with the page.
  GURL second_successor_url =
      embedded_https_test_server_.GetURL(kSiteD, "/title2.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, second_successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the second flow, and wait for UKM to emit.
  base::RunLoop ukm_loop_2;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop_2.QuitClosure());
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, flow_end_url));
  ukm_loop_2.Run();

  // Expect InFlowSuccessorInteraction to have been emitted correctly.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kInFlowSuccessorInteractionUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 2u);

  auto first_ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(first_ukm_entry, first_entrypoint_url);
  ukm_recorder().ExpectEntryMetric(first_ukm_entry, "SuccessorRedirectIndex",
                                   1);
  // In-visit storage access should be reported.
  ukm_recorder().ExpectEntryMetric(first_ukm_entry,
                                   "DidEntrypointAccessStorage", true);

  auto second_ukm_entry = ukm_entries.at(1);
  ukm_recorder().ExpectEntrySourceHasUrl(second_ukm_entry,
                                         second_entrypoint_url);
  ukm_recorder().ExpectEntryMetric(second_ukm_entry, "SuccessorRedirectIndex",
                                   3);
  // Navigational storage access should be reported.
  ukm_recorder().ExpectEntryMetric(second_ukm_entry,
                                   "DidEntrypointAccessStorage", true);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       InFlowSuccessorInteractionNotEmittedWhenNoFlowEnd) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for this flow.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  // Client-redirect to another page on B, the successor for this flow, and
  // interact with the page.
  GURL successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));

  ExpectNoUkmEventsOfType(kInFlowSuccessorInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    InFlowSuccessorInteractionNotEmittedWhenMultipleCrossSiteServerRedirects) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to C, which server-redirects to D, which server-redirects
  // to B, the would-be entrypoint.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  GURL first_server_redirector_url = embedded_https_test_server_.GetURL(
      kSiteC, "/cross-site/d.test/cross-site/b.test/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, first_server_redirector_url, entrypoint_url));
  // Client-redirect to another page on B, the would-be successor, and interact
  // with the page.
  GURL successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, which would end a valid flow.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, flow_end_url));

  ExpectNoUkmEventsOfType(kInFlowSuccessorInteractionUkmEventName);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       InFlowSuccessorInteractionOnlyEmittedOncePerSuccessor) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL referrer_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, referrer_url));
  // Client-redirect to B, the entrypoint for this flow.
  GURL entrypoint_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                          entrypoint_url));
  // Client-redirect to another page on B, which server-redirects to yet another
  // page on B (the successor for this flow), and interact with the page
  // multiple times.
  GURL successor_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  GURL server_redirector_url = embedded_https_test_server_.GetURL(
      kSiteB, "/server-redirect?/title2.html");
  ASSERT_TRUE(NavigateToURLFromRendererWithoutUserGesture(
      web_contents, server_redirector_url, successor_url));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  SimulateUserActivation(web_contents);
  // TODO - crbug.com/389048223: Speed up this step
  ASSERT_TRUE(WaitUntilTransientActivationLost(
      web_contents->GetPrimaryMainFrame(), base::Seconds(5)));
  // Client-redirect to C, ending the flow, and wait for UKM to emit.
  GURL flow_end_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kInFlowSuccessorInteractionUkmEventName,
                                       ukm_loop.QuitClosure());
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents, flow_end_url));
  ukm_loop.Run();

  // Expect only one InFlowSuccessorInteraction event.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kInFlowSuccessorInteractionUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, entrypoint_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "SuccessorRedirectIndex", 2);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidEntrypointAccessStorage",
                                   false);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeNotEmittedWhenLessThanThreePagesVisited) {
  // Visit a page on site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that writes a cookie in its response headers.
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeNotEmittedWhenSameSiteWithPriorPage) {
  // Visit a page on site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a second page on site A that writes a cookie in its response headers.
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));
  // Visit site B.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeNotEmittedWhenSameSiteWithNextPage) {
  // Visit a page on site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that writes a cookie in its response headers.
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));
  // Visit a second page on site B.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title2.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeNotEmittedWhenSiteDidNotAccessStorage) {
  // Visit A->B->C without storage access on B.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeNotEmittedWhenCookiesReadViaHeaders) {
  // Pre-write a cookie for site B so it can be passed in request headers later.
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));

  // Visit A.
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, and wait to be notified of the cookie read event.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  URLCookieAccessObserver read_cookie_observer(web_contents, second_page_url,
                                               CookieOperation::kRead);
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  read_cookie_observer.Wait();
  // Visit C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

// TODO - crbug.com/353556432: flaky on Linux release builds and on Android
#if (BUILDFLAG(IS_LINUX) && defined(NDEBUG)) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_NavigationFlowNodeNotEmittedForCookieAccessInPrerenders \
  DISABLED_NavigationFlowNodeNotEmittedForCookieAccessInPrerenders
#else
#define MAYBE_NavigationFlowNodeNotEmittedForCookieAccessInPrerenders \
  NavigationFlowNodeNotEmittedForCookieAccessInPrerenders
#endif
IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorPrerenderTest,
    MAYBE_NavigationFlowNodeNotEmittedForCookieAccessInPrerenders) {
  // Visit site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  // While still on that site B page, prerender a different page on site B that
  // accesses cookies with both response headers and Javascript.
  const GURL prerendering_url =
      embedded_https_test_server_.GetURL(kSiteB, "/set-cookie?name=value");
  const FrameTreeNodeId host_id =
      prerender_test_helper()->AddPrerender(prerendering_url);
  prerender_test_helper()->WaitForPrerenderLoadCompletion(prerendering_url);
  test::PrerenderHostObserver prerender_observer(*web_contents, host_id);
  EXPECT_FALSE(prerender_observer.was_activated());
  RenderFrameHost* prerender_frame =
      prerender_test_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);
  FrameCookieAccessObserver observer(web_contents, prerender_frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(prerender_frame, "document.cookie = 'name=newvalue;';"));
  observer.Wait();
  prerender_test_helper()->CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();
  // Visit a page on site C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorPATApiTest,
    NavigationFlowNodeNotEmittedWhenOnlyStorageAccessIsTopicsApi) {
  // Visit site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that accesses storage via the Topics API.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  ASSERT_TRUE(ExecJs(web_contents,
                     R"(
                                (async () => {
                                  await document.browsingTopics();
                                })();
                              )",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Visit site C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorPATApiTest,
    NavigationFlowNodeNotEmittedWhenOnlyStorageAccessIsProtectedAudienceApi) {
  // Visit site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that accesses storage by joining an ad interest
  // group via the Protected Audiences API.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(),
                     JsReplace(R"(
                                const pageOrigin = new URL($1).origin;
                                const interestGroup = {
                                  name: "exampleInterestGroup",
                                  owner: pageOrigin,
                                };

                                navigator.joinAdInterestGroup(
                                    interestGroup,
                                    // Pick an arbitrarily high duration to
                                    // guarantee that we never leave the ad
                                    // interest group while the test runs.
                                    /*durationSeconds=*/3000000);
                              )",
                               second_page_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_OK_AND_ASSIGN(std::vector<url::Origin> interest_group_joining_origins,
                       WaitForInterestGroupData());
  ASSERT_THAT(interest_group_joining_origins,
              testing::ElementsAre(url::Origin::Create(second_page_url)));
  // Visit site C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorPATApiTest,
    NavigationFlowNodeNotEmittedWhenOnlyStorageAccessIsPrivateStateTokensApi) {
  // Visit site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that accesses storage via the Private State Tokens
  // API.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ProvideRequestHandlerKeyCommitmentsToNetworkService({kSiteB});
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  ASSERT_TRUE(
      ExecJs(web_contents,
             JsReplace(
                 R"(
                                    (async () => {
                                      await fetch("/issue", {
                                        privateToken: {
                                          operation: "token-request",
                                          version: 1
                                        }
                                      });
                                      return await document.hasPrivateToken($1);
                                    })();
                                  )",
                 embedded_https_test_server_.GetOrigin(std::string(kSiteB))
                     .Serialize()),
             EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Visit site C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorPATApiTest,
    NavigationFlowNodeNotEmittedWhenOnlyStorageAccessIsAttributionReportingApi) {
  // Visit site A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit a page on site B that accesses storage via the Attribution Reporting
  // API.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  GURL attribution_url = embedded_https_test_server_.GetURL(
      kSiteD, "/attribution_reporting/register_source_headers.html");
  ASSERT_TRUE(ExecJs(web_contents,
                     JsReplace(
                         R"(
                                  let img = document.createElement('img');
                                  img.attributionSrc = $1;
                                  document.body.appendChild(img);)",
                         attribution_url),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_OK_AND_ASSIGN(AttributionData data, WaitForAttributionData());
  ASSERT_THAT(GetOrigins(data),
              testing::ElementsAre(url::Origin::Create(attribution_url)));
  // Visit site C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeEmitsWhenVisitingABA) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B changes cookies with JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';"));
  observer.Wait();
  base::TimeDelta visit_duration = base::Seconds(1);
  test_clock_.Advance(visit_duration);
  // Visit A again, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds",
                                   ukm::GetExponentialBucketMinForUserTiming(
                                       visit_duration.InMilliseconds()));
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeEmitsWhenWritingCookiesInHeaders) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B writes a cookie in its response headers.
  GURL second_page_url = GetSetCookieUrlForSite(kSiteB);
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));
  base::TimeDelta visit_duration = base::Minutes(1);
  test_clock_.Advance(visit_duration);
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds",
                                   ukm::GetExponentialBucketMinForUserTiming(
                                       visit_duration.InMilliseconds()));
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeEmitsWhenIframeWritesCookiesInHeaders) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B has an iframe that writes cookies in its response headers.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/iframe_clipped.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  GURL iframe_url =
      embedded_https_test_server_.GetURL(kSiteB, "/set-cookie?name=value");
  URLCookieAccessObserver observer(
      web_contents, iframe_url,
      network::mojom::CookieAccessDetails_Type::kChange);
  ASSERT_TRUE(NavigateIframeToURL(web_contents, "iframe", iframe_url));
  observer.Wait();
  base::TimeDelta visit_duration = base::Milliseconds(1);
  test_clock_.Advance(visit_duration);
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds",
                                   ukm::GetExponentialBucketMinForUserTiming(
                                       visit_duration.InMilliseconds()));
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeNotEmittedWhenReadingNonexistentCookiesWithJavascript) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B reads cookies with JS, but no cookies exist for B.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  ASSERT_TRUE(ExecJs(web_contents, "const cookie = document.cookie;"));
  // Visit C.
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));

  ExpectNoNavigationFlowNodeUkmEvents();
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeEmitsWhenReadingCookiesWithJavascript) {
  // Pre-write a cookie for site B so it can be read later.
  WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));
  // Visit A.
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B reads cookies with JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver cookie_read_observer(web_contents, frame,
                                                 CookieOperation::kRead);
  ASSERT_TRUE(ExecJs(frame, "const cookie = document.cookie;"));
  cookie_read_observer.Wait();
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds", 0l);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeEmitsWhenWritingCookiesWithJavascript) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B changes cookies with JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';"));
  observer.Wait();
  base::TimeDelta visit_duration = base::Hours(1);
  test_clock_.Advance(visit_duration);
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds",
                                   ukm::GetExponentialBucketMinForUserTiming(
                                       visit_duration.InMilliseconds()));
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeEmitsWhenLocalStorageAccessed) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B writes to local storage.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(),
                     JsReplace("localStorage.setItem('value', 'abc123');"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  base::TimeDelta visit_duration = base::Minutes(70);
  test_clock_.Advance(visit_duration);
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds",
                                   ukm::GetExponentialBucketMinForUserTiming(
                                       visit_duration.InMilliseconds()));
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeCorrectWhenEntryAndExitRendererInitiated) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B with a renderer-initiated navigation, where B changes cookies with
  // JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';"));
  observer.Wait();
  // Visit C with a renderer-initiated navigation, and wait for UKM to be
  // recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds", 0l);
}

IN_PROC_BROWSER_TEST_F(
    BtmNavigationFlowDetectorTest,
    NavigationFlowNodeCorrectWhenOnlyEntryRendererInitiated) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B with a renderer-initiated navigation, where B changes cookies with
  // JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';"));
  observer.Wait();
  // Visit C with a browser-initiated navigation, and wait for UKM to be
  // recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds", 0l);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeCorrectWhenOnlyExitRendererInitiated) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B with a browser-initiated navigation, where B changes cookies with
  // JS.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, second_page_url));
  RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(frame, "document.cookie = 'name=value;';"));
  observer.Wait();
  // Visit C with a renderer-initiated navigation, and wait for UKM to be
  // recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds", 0l);
}

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorTest,
                       NavigationFlowNodeReportsNegativeDurationAsZero) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B writes a cookie in its response headers. Fake a clock
  // rewind to cause a negative visit duration.
  GURL second_page_url = GetSetCookieUrlForSite(kSiteB);
  ASSERT_TRUE(
      NavigateToSetCookieAndAwaitAccessNotification(web_contents, kSiteB));
  test_clock_.Advance(base::Milliseconds(-1));
  // Visit C, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteC, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "VisitDurationMilliseconds", 0l);
}

// WebAuthn tests do not work on Android because there is currently no way to
// install a virtual authenticator.
// TODO(crbug.com/40269763): Implement automated testing once the infrastructure
// permits it (Requires mocking the Android Platform Authenticator i.e. GMS
// Core).
#if !BUILDFLAG(IS_ANDROID)
class BtmNavigationFlowDetectorWebAuthnTest : public ContentBrowserTest {
 public:
  BtmNavigationFlowDetectorWebAuthnTest()
      : embedded_https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  BtmNavigationFlowDetectorWebAuthnTest(
      const BtmNavigationFlowDetectorWebAuthnTest&) = delete;
  BtmNavigationFlowDetectorWebAuthnTest& operator=(
      const BtmNavigationFlowDetectorWebAuthnTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // Allowlist all certs for the HTTPS server.
    mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    embedded_https_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server_.Start());

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();

    virtual_device_factory->mutable_state()->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4}, authn_hostname,
        std::vector<uint8_t>{5, 6, 7, 8}, "Foo", "Foo Bar");

    device::VirtualCtap2Device::Config config;
    config.resident_key_support = true;
    virtual_device_factory->SetCtap2Config(std::move(config));

    auth_env_ = std::make_unique<ScopedAuthenticatorEnvironmentForTesting>(
        std::move(virtual_device_factory));

    ukm_recorder_.emplace();
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(GetActiveWebContents());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
  }

  void PostRunTestOnMainThread() override {
    auth_env_.reset();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  WebContents* GetActiveWebContents() { return shell()->web_contents(); }

  void GetWebAuthnAssertion() {
    ASSERT_EQ("OK", EvalJs(GetActiveWebContents(), R"(
    let cred_id = new Uint8Array([1,2,3,4]);
    navigator.credentials.get({
      publicKey: {
        challenge: cred_id,
        userVerification: 'preferred',
        allowCredentials: [{
          type: 'public-key',
          id: cred_id,
          transports: ['usb', 'nfc', 'ble'],
        }],
        timeout: 10000
      }
    }).then(c => 'OK',
      e => e.toString());
  )",
                           EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return ukm_recorder_.value(); }

  ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

 protected:
  const std::string authn_hostname = std::string(kSiteB);
  net::EmbeddedTestServer embedded_https_test_server_;

 private:
  ContentMockCertVerifier mock_cert_verifier_;
  std::unique_ptr<ScopedAuthenticatorEnvironmentForTesting> auth_env_;
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(BtmNavigationFlowDetectorWebAuthnTest,
                       NavigationFlowNodeReportsWAA) {
  // Visit A.
  WebContents* web_contents = GetActiveWebContents();
  GURL first_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, first_page_url));
  // Visit B, where B writes a cookie in its response headers.
  GURL second_page_url =
      embedded_https_test_server_.GetURL(kSiteB, "/set-cookie?name=value");
  URLCookieAccessObserver observer(
      web_contents, second_page_url,
      network::mojom::CookieAccessDetails_Type::kChange);
  ASSERT_TRUE(NavigateToSetCookie(web_contents, &embedded_https_test_server_,
                                  kSiteB, false, false));
  observer.Wait();
  GetWebAuthnAssertion();
  // Visit A again, and wait for UKM to be recorded.
  base::RunLoop ukm_loop;
  ukm_recorder().SetOnAddEntryCallback(kNavigationFlowNodeUkmEventName,
                                       ukm_loop.QuitClosure());
  GURL third_page_url =
      embedded_https_test_server_.GetURL(kSiteA, "/title1.html");
  ASSERT_TRUE(NavigateToURL(web_contents, third_page_url));
  ukm_loop.Run();

  // Expect metrics to be accurate.
  auto ukm_entries =
      ukm_recorder().GetEntriesByName(kNavigationFlowNodeUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto ukm_entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(ukm_entry, second_page_url);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WerePreviousAndNextSiteSame",
                                   true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveUserActivation", false);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "DidHaveSuccessfulWAA", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasEntryUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry, "WasExitUserInitiated", true);
  ukm_recorder().ExpectEntryMetric(ukm_entry,
                                   "WereEntryAndExitRendererInitiated", false);
}
#endif  // !BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(
    All,
    BtmNavigationFlowDetectorClientRedirectTest,
    kAllBtmClientRedirectMethods,
    [](const testing::TestParamInfo<
        BtmNavigationFlowDetectorClientRedirectTest::ParamType>& param_info) {
      BtmClientRedirectMethod client_redirect_method = param_info.param;
      return StringifyBtmClientRedirectMethod(client_redirect_method);
    });

}  // namespace content
