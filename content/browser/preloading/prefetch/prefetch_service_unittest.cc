// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kPrefetchProxyAddress[] = "https://testprefetchproxy.com";

const char kApiKey[] = "APIKEY";

const int kTotalTimeDuration = 4321;

const int kConnectTimeDuration = 123;

const int kHeaderLatency = 456;

const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";

class MockPrefetchServiceDelegate : public PrefetchServiceDelegate {
 public:
  explicit MockPrefetchServiceDelegate(int num_on_prefetch_likely_calls = 1) {
    // Sets default behavior for the delegate.
    ON_CALL(*this, GetDefaultPrefetchProxyHost)
        .WillByDefault(testing::Return(GURL(kPrefetchProxyAddress)));
    ON_CALL(*this, GetAPIKey).WillByDefault(testing::Return(kApiKey));
    ON_CALL(*this, IsOriginOutsideRetryAfterWindow(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(*this, DisableDecoysBasedOnUserSettings)
        .WillByDefault(testing::Return(false));
    ON_CALL(*this, IsSomePreloadingEnabled)
        .WillByDefault(testing::Return(true));
    ON_CALL(*this, IsExtendedPreloadingEnabled)
        .WillByDefault(testing::Return(false));
    ON_CALL(*this, IsDomainInPrefetchAllowList(testing::_))
        .WillByDefault(testing::Return(true));

    EXPECT_CALL(*this, OnPrefetchLikely(testing::_))
        .Times(num_on_prefetch_likely_calls);
  }

  ~MockPrefetchServiceDelegate() override = default;

  MockPrefetchServiceDelegate(const MockPrefetchServiceDelegate&) = delete;
  MockPrefetchServiceDelegate& operator=(const MockPrefetchServiceDelegate) =
      delete;

  // PrefetchServiceDelegate.
  MOCK_METHOD(std::string, GetMajorVersionNumber, (), (override));
  MOCK_METHOD(std::string, GetAcceptLanguageHeader, (), (override));
  MOCK_METHOD(GURL, GetDefaultPrefetchProxyHost, (), (override));
  MOCK_METHOD(std::string, GetAPIKey, (), (override));
  MOCK_METHOD(GURL, GetDefaultDNSCanaryCheckURL, (), (override));
  MOCK_METHOD(GURL, GetDefaultTLSCanaryCheckURL, (), (override));
  MOCK_METHOD(void,
              ReportOriginRetryAfter,
              (const GURL&, base::TimeDelta),
              (override));
  MOCK_METHOD(bool, IsOriginOutsideRetryAfterWindow, (const GURL&), (override));
  MOCK_METHOD(void, ClearData, (), (override));
  MOCK_METHOD(bool, DisableDecoysBasedOnUserSettings, (), (override));
  MOCK_METHOD(bool, IsSomePreloadingEnabled, (), (override));
  MOCK_METHOD(bool, IsExtendedPreloadingEnabled, (), (override));
  MOCK_METHOD(bool, IsDomainInPrefetchAllowList, (const GURL&), (override));
  MOCK_METHOD(void, OnPrefetchLikely, (WebContents*), (override));
};

class ScopedPrefetchServiceContentBrowserClient
    : public TestContentBrowserClient {
 public:
  explicit ScopedPrefetchServiceContentBrowserClient(
      std::unique_ptr<MockPrefetchServiceDelegate>
          mock_prefetch_service_delegate)
      : mock_prefetch_service_delegate_(
            std::move(mock_prefetch_service_delegate)) {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedPrefetchServiceContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  void SetDataSaverEnabledForTesting(bool data_saver_enabled) {
    data_saver_enabled_ = data_saver_enabled;
  }

  // ContentBrowserClient.
  std::unique_ptr<PrefetchServiceDelegate> CreatePrefetchServiceDelegate(
      content::BrowserContext*) override {
    return std::move(mock_prefetch_service_delegate_);
  }

  bool IsDataSaverEnabled(BrowserContext*) override {
    return data_saver_enabled_;
  }

  void OverrideWebkitPrefs(WebContents*,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate_;

  bool data_saver_enabled_{false};
};

// This is only used to test the proxy lookup.
class TestNetworkContext : public network::TestNetworkContext {
 public:
  explicit TestNetworkContext(absl::optional<net::ProxyInfo> proxy_info)
      : proxy_info_(proxy_info) {}

  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_isolation_key,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          pending_proxy_lookup_client) override {
    mojo::Remote<network::mojom::ProxyLookupClient> proxy_lookup_client(
        std::move(pending_proxy_lookup_client));
    proxy_lookup_client->OnProxyLookupComplete(net::OK, proxy_info_);
  }

 private:
  absl::optional<net::ProxyInfo> proxy_info_;
};

class PrefetchServiceTest : public RenderViewHostTestHarness {
 public:
  PrefetchServiceTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());

    InitScopedFeatureList();

    PrefetchService::SetURLLoaderFactoryForTesting(
        test_shared_url_loader_factory_.get());

    PrefetchService::SetHostNonUniqueFilterForTesting(
        [](base::StringPiece) { return false; });
    PrefetchService::SetServiceWorkerContextForTesting(
        &service_worker_context_);
  }

  void TearDown() override {
    if (PrefetchDocumentManager::GetForCurrentDocument(main_rfh()))
      PrefetchDocumentManager::DeleteForCurrentDocument(main_rfh());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    mock_navigation_handle_.reset();
    prefetch_service_.reset();
    PrefetchService::SetURLLoaderFactoryForTesting(nullptr);
    PrefetchService::SetHostNonUniqueFilterForTesting(nullptr);
    PrefetchService::SetServiceWorkerContextForTesting(nullptr);
    PrefetchService::SetURLLoaderFactoryForTesting(nullptr);
    test_content_browser_client_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  virtual void InitScopedFeatureList() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"}});
  }

  void MakePrefetchService(std::unique_ptr<MockPrefetchServiceDelegate>
                               mock_prefetch_service_delegate) {
    test_content_browser_client_ =
        std::make_unique<ScopedPrefetchServiceContentBrowserClient>(
            std::move(mock_prefetch_service_delegate));

    prefetch_service_ = PrefetchService::CreateIfPossible(browser_context());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }

  // Creates a prefetch request for |url| on the current main frame.
  void MakePrefetchOnMainFrame(const GURL& url,
                               const PrefetchType& prefetch_type) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
    prefetch_document_manager->PrefetchUrl(url, prefetch_type,
                                           blink::mojom::Referrer(), nullptr);
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  void VerifyCommonRequestState(const GURL& url, bool use_prefetch_proxy) {
    SCOPED_TRACE(url.spec());
    EXPECT_EQ(RequestCount(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_TRUE(request->request.enable_load_timing);
    EXPECT_EQ(request->request.load_flags,
              net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kInclude);

    std::string purpose_value;
    EXPECT_TRUE(request->request.headers.GetHeader("Purpose", &purpose_value));
    EXPECT_EQ(purpose_value, "prefetch");

    std::string sec_purpose_value;
    EXPECT_TRUE(
        request->request.headers.GetHeader("Sec-Purpose", &sec_purpose_value));
    EXPECT_EQ(sec_purpose_value,
              use_prefetch_proxy ? "prefetch;anonymous-client-ip" : "prefetch");

    std::string accept_value;
    EXPECT_TRUE(request->request.headers.GetHeader("Accept", &accept_value));
    EXPECT_EQ(accept_value, FrameAcceptHeaderValue(/*allow_sxg_responses=*/true,
                                                   browser_context()));

    std::string upgrade_insecure_request_value;
    EXPECT_TRUE(request->request.headers.GetHeader(
        "Upgrade-Insecure-Requests", &upgrade_insecure_request_value));
    EXPECT_EQ(upgrade_insecure_request_value, "1");

    EXPECT_TRUE(request->request.trusted_params.has_value());
    VerifyIsolationInfo(request->request.trusted_params->isolation_info);
  }

  void VerifyIsolationInfo(const net::IsolationInfo& isolation_info) {
    EXPECT_FALSE(isolation_info.IsEmpty());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
    EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_FALSE(isolation_info.site_for_cookies().IsNull());
  }

  void MakeResponseAndWait(
      net::HttpStatusCode http_status,
      net::Error net_error,
      const std::string mime_type,
      bool use_prefetch_proxy,
      std::vector<std::pair<std::string, std::string>> headers,
      const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = network::CreateURLResponseHead(http_status);

    head->response_time = base::Time::Now();
    head->request_time =
        head->response_time - base::Milliseconds(kTotalTimeDuration);

    head->load_timing.connect_timing.connect_end =
        base::TimeTicks::Now() - base::Minutes(2);
    head->load_timing.connect_timing.connect_start =
        head->load_timing.connect_timing.connect_end -
        base::Milliseconds(kConnectTimeDuration);

    head->load_timing.receive_headers_end = base::TimeTicks::Now();
    head->load_timing.request_start = head->load_timing.receive_headers_end -
                                      base::Milliseconds(kHeaderLatency);

    head->proxy_server =
        use_prefetch_proxy
            ? net::ProxyServer::FromSchemeHostAndPort(
                  net::ProxyServer::Scheme::SCHEME_HTTPS,
                  PrefetchProxyHost(GURL(kPrefetchProxyAddress)).spec(),
                  absl::nullopt)
            : net::ProxyServer::Direct();

    head->mime_type = mime_type;
    for (const auto& header : headers) {
      head->headers->AddHeader(header.first, header.second);
    }
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    test_url_loader_factory_.ClearResponses();
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));

    EXPECT_TRUE(cookie.get());

    bool result = false;
    base::RunLoop run_loop;

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        *cookie.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

  void Navigate(const GURL& url,
                const GlobalRenderFrameHostId& previous_rfh_id) {
    mock_navigation_handle_ =
        std::make_unique<testing::NiceMock<MockNavigationHandle>>(
            web_contents());
    mock_navigation_handle_->set_url(url);

    ON_CALL(*mock_navigation_handle_, GetPreviousRenderFrameHostId)
        .WillByDefault(testing::Return(previous_rfh_id));

    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
    prefetch_document_manager->DidStartNavigation(
        mock_navigation_handle_.get());
  }

  absl::optional<PrefetchServingPageMetrics>
  GetMetricsForMostRecentNavigation() {
    if (!mock_navigation_handle_)
      return absl::nullopt;

    return PrefetchServingPageMetrics::GetForNavigationHandle(
        *mock_navigation_handle_);
  }

  ScopedPrefetchServiceContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
  }

 protected:
  FakeServiceWorkerContext service_worker_context_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PrefetchService> prefetch_service_;

  std::unique_ptr<testing::NiceMock<MockNavigationHandle>>
      mock_navigation_handle_;

  std::unique_ptr<ScopedPrefetchServiceContentBrowserClient>
      test_content_browser_client_;
};

TEST_F(PrefetchServiceTest, CreateServiceWhenFeatureEnabled) {
  // Enable feature, which means that we should be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_TRUE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, DontCreateServiceWhenFeatureDisabled) {
  // Disable feature, which means that we shouldn't be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_FALSE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NoPrefetchingPreloadingDisabled) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0);

  // When preloading is disabled, then |PrefetchService| doesn't take the
  // prefetch at all.
  EXPECT_CALL(*mock_prefetch_service_delegate, IsSomePreloadingEnabled)
      .Times(1)
      .WillOnce(testing::Return(false));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_FALSE(serving_page_metrics->prefetch_status);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, NoPrefetchingDomainNotInAllowList) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0);

  // When referring page is not in allow list, then |PrefetchService| doesn't
  // take the prefetch at all.
  EXPECT_CALL(*mock_prefetch_service_delegate,
              IsDomainInPrefetchAllowList(testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_FALSE(serving_page_metrics->prefetch_status);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

class PrefetchServiceAllowAllDomainsTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"},
         {"allow_all_domains", "true"}});
  }
};

TEST_F(PrefetchServiceAllowAllDomainsTest, AllowAllDomains) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  // When "allow_all_domains" is set to true, then we can prefetch from all
  // domains, not just those in the allow list.
  EXPECT_CALL(*mock_prefetch_service_delegate,
              IsDomainInPrefetchAllowList(testing::_))
      .Times(0);

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

class PrefetchServiceAllowAllDomainsForExtendedPreloadingTest
    : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"},
         {"allow_all_domains_for_extended_preloading", "true"}});
  }
};

TEST_F(PrefetchServiceAllowAllDomainsForExtendedPreloadingTest,
       ExtendedPreloadingEnabled) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  // Allow all domains if and only if extended preloading is enabled.
  EXPECT_CALL(*mock_prefetch_service_delegate, IsExtendedPreloadingEnabled)
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_prefetch_service_delegate,
              IsDomainInPrefetchAllowList(testing::_))
      .Times(0);

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceAllowAllDomainsForExtendedPreloadingTest,
       ExtendedPreloadingDisabled) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0);

  // If extended preloading is disabled, then we check the allow list.
  EXPECT_CALL(*mock_prefetch_service_delegate, IsExtendedPreloadingEnabled)
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_prefetch_service_delegate,
              IsDomainInPrefetchAllowList(testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_FALSE(serving_page_metrics->prefetch_status);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, NotEligibleHostnameNonUnique) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  PrefetchService::SetHostNonUniqueFilterForTesting(
      [](base::StringPiece) { return true; });

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, NotEligibleDataSaverEnabled) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  test_content_browser_client()->SetDataSaverEnabledForTesting(true);

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, NotEligibleNonHttps) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("http://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("http://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, NotEligiblePrefetchProxyNotAvailable) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  // If the prefetch proxy URL is invalid, then we can't make prefetches that
  // require the proxy. However, non-proxied prefetches are fine.
  EXPECT_CALL(*mock_prefetch_service_delegate, GetDefaultPrefetchProxyHost)
      .Times(1)
      .WillOnce(testing::Return(GURL("")));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchProxyNotAvailable));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest,
       EligiblePrefetchProxyNotAvailableNonProxiedPrefetch) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  // If the prefetch proxy URL is invalid, then we can't make prefetches that
  // require the proxy. However, non-proxied prefetches are fine.
  EXPECT_CALL(*mock_prefetch_service_delegate, GetDefaultPrefetchProxyHost)
      .Times(1)
      .WillOnce(testing::Return(GURL("")));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/false));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/false);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_FALSE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NotEligibleOriginWithinRetryAfterWindow) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  EXPECT_CALL(*mock_prefetch_service_delegate,
              IsOriginOutsideRetryAfterWindow(GURL("https://example.com")))
      .Times(1)
      .WillOnce(testing::Return(false));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchIneligibleRetryAfter));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, EligibleNonHttpsNonProxiedPotentiallyTrustworthy) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://localhost"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/false));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://localhost"),
                           /*use_prefetch_proxy=*/false);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://localhost"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_FALSE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://localhost"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NotEligibleServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://example.com"))));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(
                PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, EligibleServiceWorkerNotRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://other.com"))));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NotEligibleUserHasCookies) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleUserHasCookies));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, EligibleUserHasCookiesForDifferentUrl) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://other.com"), "testing"));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, EligibleSameOriginPrefetchCanHaveExistingCookies) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/false,
                                       /*use_prefetch_proxy=*/false));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/false);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_FALSE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NotEligibleExistingConnectProxy) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("proxy.com");
  TestNetworkContext network_context_for_proxy_lookup(proxy_info);
  PrefetchService::SetNetworkContextForProxyLookupForTesting(
      &network_context_for_proxy_lookup);

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleExistingProxy));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  PrefetchService::SetNetworkContextForProxyLookupForTesting(nullptr);
}

TEST_F(PrefetchServiceTest, EligibleExistingConnectProxyButSameOriginPrefetch) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("proxy.com");
  TestNetworkContext network_context_for_proxy_lookup(proxy_info);
  PrefetchService::SetNetworkContextForProxyLookupForTesting(
      &network_context_for_proxy_lookup);

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/false,
                                       /*use_prefetch_proxy=*/false));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/false);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_FALSE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));

  PrefetchService::SetNetworkContextForProxyLookupForTesting(nullptr);
}

TEST_F(PrefetchServiceTest, FailedNon2XXResponseCode) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchFailedNon2XX));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, FailedNetError) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", std::abs(net::ERR_FAILED),
      1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchFailedNetError));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, HandleRetryAfterResponse) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  EXPECT_CALL(
      *mock_prefetch_service_delegate,
      ReportOriginRetryAfter(GURL("https://example.com"), base::Seconds(1234)))
      .Times(1);

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  // Simulate the origin responding with a "retry-after" header.
  MakeResponseAndWait(net::HTTP_SERVICE_UNAVAILABLE, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"Retry-After", "1234"}, {"X-Testing", "Hello World"}},
                      "");

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode",
      net::HTTP_SERVICE_UNAVAILABLE, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchFailedNon2XX));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceTest, SuccessNonHTML) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  std::string body = "fake PDF";
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/pdf",
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, body);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", body.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));
}

TEST_F(PrefetchServiceTest, NotServeableNavigationInDifferentRenderFrameHost) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Since the navigation is occurring in a RenderFrameHost other than where the
  // prefetch was requested from, we cannot use it.
  GlobalRenderFrameHostId other_rfh_id(
      main_rfh()->GetGlobalId().child_id + 1,
      main_rfh()->GetGlobalId().frame_routing_id + 1);
  ASSERT_NE(other_rfh_id, main_rfh()->GetGlobalId());
  Navigate(GURL("https://example.com"), other_rfh_id);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  EXPECT_FALSE(serving_page_metrics);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

class PrefetchServiceLimitedPrefetchesTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"},
         {"max_srp_prefetches", "2"}});
  }
};

TEST_F(PrefetchServiceLimitedPrefetchesTest, LimitedNumberOfPrefetches) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/3));

  // Make 3 prefetches from the same page. PrefetchService should make requests
  // for the first two prefetches but not the third due to the limit on the
  // number of prefetches.
  MakePrefetchOnMainFrame(GURL("https://example1.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();
  MakePrefetchOnMainFrame(GURL("https://example2.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();
  MakePrefetchOnMainFrame(GURL("https://example3.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example1.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  VerifyCommonRequestState(GURL("https://example2.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 3);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 2);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 3);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 3);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 2);

  Navigate(GURL("https://example1.com"), main_rfh()->GetGlobalId());

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics1 =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics1);
  EXPECT_TRUE(serving_page_metrics1->prefetch_status);
  EXPECT_EQ(serving_page_metrics1->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics1->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics1->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics1->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics1->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container1 =
      prefetch_service_->GetPrefetchToServe(GURL("https://example1.com"));
  ASSERT_TRUE(serveable_prefetch_container1);
  EXPECT_TRUE(serveable_prefetch_container1->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container1->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container1->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));

  Navigate(GURL("https://example2.com"), main_rfh()->GetGlobalId());

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics2 =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics2);
  EXPECT_TRUE(serving_page_metrics2->prefetch_status);
  EXPECT_EQ(serving_page_metrics2->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics2->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics2->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics2->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics2->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container2 =
      prefetch_service_->GetPrefetchToServe(GURL("https://example2.com"));
  ASSERT_TRUE(serveable_prefetch_container2);
  EXPECT_TRUE(serveable_prefetch_container2->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container2->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container2->HasValidPrefetchedResponse(
      base::TimeDelta::Max()));

  Navigate(GURL("https://example3.com"), main_rfh()->GetGlobalId());

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics3 =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics3);
  EXPECT_TRUE(serving_page_metrics3->prefetch_status);
  EXPECT_EQ(serving_page_metrics3->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchNotStarted));
  EXPECT_TRUE(serving_page_metrics3->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics3->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics3->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container3 =
      prefetch_service_->GetPrefetchToServe(GURL("https://example3.com"));
  EXPECT_FALSE(serveable_prefetch_container3);
}

class PrefetchServiceWithHTMLOnlyTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"},
         {"html_only", "true"}});
  }
};

TEST_F(PrefetchServiceWithHTMLOnlyTest, FailedNonHTMLWithHTMLOnly) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  std::string body = "fake PDF";
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/pdf",
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, body);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", body.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchFailedMIMENotSupported));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

class PrefetchServiceAlwaysMakeDecoyRequestTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "1"},
         {"prefetch_container_lifetime_s", "-1"}});
  }
};

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest, DecoyRequest) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchIsPrivacyDecoy));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest,
       NoDecoyRequestDisableDecoysBasedOnUserSettings) {
  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  EXPECT_CALL(*mock_prefetch_service_delegate, DisableDecoysBasedOnUserSettings)
      .Times(1)
      .WillOnce(testing::Return(true));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(GURL("https://example.com"),
                          PrefetchType(/*use_isolated_network_context=*/true,
                                       /*use_prefetch_proxy=*/true));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  absl::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 0);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(
      serving_page_metrics->prefetch_status.value(),
      static_cast<int>(PrefetchStatus::kPrefetchNotEligibleUserHasCookies));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      prefetch_service_->GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
}

// TODO(https://crbug.com/1299059): Add test for incognito mode.

}  // namespace
}  // namespace content
