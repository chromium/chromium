// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/parsed_headers.h"
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

PreloadingEligibility ToPreloadingEligibility(PrefetchStatus status) {
  if (status == PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled) {
    return PreloadingEligibility::kDataSaverEnabled;
  }
  return static_cast<PreloadingEligibility>(
      static_cast<int>(status) +
      static_cast<int>(PreloadingEligibility::kPreloadingEligibilityCommonEnd));
}

PreloadingFailureReason ToPreloadingFailureReason(PrefetchStatus status) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonCommonEnd));
}

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
        .WillByDefault(testing::Return(PreloadingEligibility::kEligible));
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
  MOCK_METHOD(PreloadingEligibility, IsSomePreloadingEnabled, (), (override));
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
    off_the_record_context_ = std::make_unique<TestBrowserContext>();
    off_the_record_context_->set_is_off_the_record(true);
  }

  ~ScopedPrefetchServiceContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  // ContentBrowserClient.
  std::unique_ptr<PrefetchServiceDelegate> CreatePrefetchServiceDelegate(
      BrowserContext*) override {
    return std::move(mock_prefetch_service_delegate_);
  }

  void UseOffTheRecordContextForStoragePartition(bool use) {
    use_off_the_record_context_for_storage_paritition_ = use;
  }
  // `BrowserContext::GetStoragePartitionForUrl` eventually calls this method
  // on the browser client to get the config. Overwrite it so the prefetch can
  // be rejected due to a non-default storage partition.
  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override {
    if (use_off_the_record_context_for_storage_paritition_) {
      return StoragePartitionConfig::CreateDefault(
          off_the_record_context_.get());
    }
    return TestContentBrowserClient::GetStoragePartitionConfigForSite(
        browser_context, site);
  }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate_;
  // This browser context is used to generate a different storage partition if
  // `use_off_the_record_context_for_storage_paritition_` is set to true.
  std::unique_ptr<TestBrowserContext> off_the_record_context_;
  bool use_off_the_record_context_for_storage_paritition_{false};
};

// This is only used to test the proxy lookup.
class TestNetworkContext : public network::TestNetworkContext {
 public:
  explicit TestNetworkContext(absl::optional<net::ProxyInfo> proxy_info)
      : proxy_info_(proxy_info) {}

  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
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

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);

    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {network::features::kPrefetchNoVarySearch});
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
                               const PrefetchType& prefetch_type,
                               bool enable_no_vary_search_header = false) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
    if (enable_no_vary_search_header)
      prefetch_document_manager->EnableNoVarySearchSupport();
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

  network::mojom::URLResponseHeadPtr CreateURLResponseHeadForPrefetch(
      net::HttpStatusCode http_status,
      const std::string mime_type,
      bool use_prefetch_proxy,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const GURL& request_url) {
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
    if (!head->parsed_headers) {
      head->parsed_headers =
          network::PopulateParsedHeaders(head->headers.get(), request_url);
    }

    return head;
  }

  void MakeResponseAndWait(
      net::HttpStatusCode http_status,
      net::Error net_error,
      const std::string mime_type,
      bool use_prefetch_proxy,
      std::vector<std::pair<std::string, std::string>> headers,
      const std::string& body,
      network::TestURLLoaderFactory::Redirects redirects =
          network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags rp_flags =
          network::TestURLLoaderFactory::kResponseDefault) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);

    auto head = CreateURLResponseHeadForPrefetch(http_status, mime_type,
                                                 use_prefetch_proxy, headers,
                                                 request->request.url);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status, std::move(redirects),
                                         rp_flags);
    task_environment()->RunUntilIdle();
    // Clear responses in the network service so we can inspect the next request
    // that comes in before it is responded to.
    test_url_loader_factory_.ClearResponses();
  }

  void SendHeadOfResponseAndWait(
      net::HttpStatusCode http_status,
      const std::string mime_type,
      bool use_prefetch_proxy,
      std::vector<std::pair<std::string, std::string>> headers,
      uint32_t expected_total_body_size) {
    ASSERT_FALSE(producer_handle_);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);

    auto head = CreateURLResponseHeadForPrefetch(http_status, mime_type,
                                                 use_prefetch_proxy, headers,
                                                 request->request.url);

    mojo::ScopedDataPipeConsumerHandle body;
    EXPECT_EQ(
        mojo::CreateDataPipe(expected_total_body_size, producer_handle_, body),
        MOJO_RESULT_OK);

    request->client->OnReceiveResponse(std::move(head), std::move(body),
                                       absl::nullopt);
    task_environment()->RunUntilIdle();
  }

  void SendBodyContentOfResponseAndWait(const std::string& body) {
    ASSERT_TRUE(producer_handle_);

    uint32_t bytes_written = body.size();
    EXPECT_EQ(producer_handle_->WriteData(body.data(), &bytes_written,
                                          MOJO_WRITE_DATA_FLAG_ALL_OR_NONE),
              MOJO_RESULT_OK);
    task_environment()->RunUntilIdle();
  }

  void CompleteResponseAndWait(net::Error net_error,
                               uint32_t expected_total_body_size) {
    ASSERT_TRUE(producer_handle_);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);

    producer_handle_.reset();

    network::URLLoaderCompletionStatus completion_status(net_error);
    completion_status.decoded_body_length = expected_total_body_size;
    request->client->OnComplete(completion_status);
    task_environment()->RunUntilIdle();

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

  base::WeakPtr<PrefetchContainer> GetPrefetchToServe(const GURL& url) {
    base::RunLoop run_loop;
    base::WeakPtr<PrefetchContainer> return_prefetch;

    prefetch_service_->GetPrefetchToServe(
        url, base::BindOnce(
                 [](base::WeakPtr<PrefetchContainer>* return_prefetch,
                    base::RunLoop* run_loop,
                    base::WeakPtr<PrefetchContainer> prefetch_to_serve) {
                   *return_prefetch = prefetch_to_serve;
                   run_loop->Quit();
                 },
                 &return_prefetch, &run_loop));
    run_loop.Run();
    return return_prefetch;
  }

  ScopedPrefetchServiceContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const test::PreloadingAttemptUkmEntryBuilder* attempt_entry_builder() {
    return attempt_entry_builder_.get();
  }

  ukm::SourceId ForceLogsUploadAndGetUkmId() {
    MockNavigationHandle mock_handle;
    mock_handle.set_is_in_primary_main_frame(true);
    mock_handle.set_is_same_document(false);
    mock_handle.set_has_committed(true);
    // Makes sure the accurate bit is always false.
    mock_handle.set_url(GURL("http://Not.Accurate.Trigger.Url/"));
    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents());
    // Sets the accurate bit, and records `TimeToNextNavigation`.
    static_cast<PreloadingDataImpl*>(preloading_data)
        ->DidStartNavigation(&mock_handle);
    // Records the UKMs.
    static_cast<PreloadingDataImpl*>(preloading_data)
        ->DidFinishNavigation(&mock_handle);
    return mock_handle.GetNextPageUkmSourceId();
  }

  void ExpectCorrectUkmLogs(PreloadingEligibility eligibility,
                            PreloadingHoldbackStatus holdback,
                            PreloadingTriggeringOutcome outcome,
                            PreloadingFailureReason failure) {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 1u);

    absl::optional<base::TimeDelta> ready_time = absl::nullopt;
    if (outcome == PreloadingTriggeringOutcome::kReady ||
        outcome == PreloadingTriggeringOutcome::kSuccess) {
      ready_time = base::ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }

    const auto expected_attempts = {attempt_entry_builder()->BuildEntry(
        source_id, PreloadingType::kPrefetch, eligibility, holdback, outcome,
        failure,
        /*accurate=*/false, ready_time)};

    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
    // We do not test the `PreloadingPrediction` as it is added in
    // `PreloadingDecider`.
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

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;

  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};

TEST_F(PrefetchServiceTest, CreateServiceWhenFeatureEnabled) {
  // Enable feature, which means that we should be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrefetchUseContentRefactor},
      {network::features::kPrefetchNoVarySearch});

  EXPECT_TRUE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, DontCreateServiceWhenFeatureDisabled) {
  // Disable feature, which means that we shouldn't be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {features::kPrefetchUseContentRefactor,
           network::features::kPrefetchNoVarySearch});

  EXPECT_FALSE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));
  ASSERT_TRUE(serveable_prefetch_container->GetHead());
  EXPECT_TRUE(serveable_prefetch_container->GetHead()->was_in_prefetch_cache);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
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
      .WillOnce(testing::Return(PreloadingEligibility::kPreloadingDisabled));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  // We expect one entry because the PreloadingAttempt is created when the
  // container is created, but since `IsSomePreloadingEnabled()` is false
  // we did not reach to the eligibility check.
  ExpectCorrectUkmLogs(PreloadingEligibility::kUnspecified,
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  // `IsDomainInPrefetchAllowList` returns false so we did not reach the
  // eligibility check.
  ExpectCorrectUkmLogs(PreloadingEligibility::kUnspecified,
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceAllowAllDomainsTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains", "true"}}}},
        {network::features::kPrefetchNoVarySearch});
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceAllowAllDomainsForExtendedPreloadingTest
    : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains_for_extended_preloading", "true"}}}},
        {network::features::kPrefetchNoVarySearch});
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kUnspecified,
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NonProxiedPrefetchDoesNotRequireAllowList) {
  base::HistogramTester histogram_tester;

  // Assume we have a delegate which will not grant access to the proxy for this
  // domain. Nonetheless a non-proxied prefetch should work.
  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();
  ON_CALL(*mock_prefetch_service_delegate, IsExtendedPreloadingEnabled)
      .WillByDefault(testing::Return(false));
  ON_CALL(*mock_prefetch_service_delegate,
          IsDomainInPrefetchAllowList(testing::_))
      .WillByDefault(testing::Return(false));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/false,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotEligibleHostnameNonUnique) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  PrefetchService::SetHostNonUniqueFilterForTesting(
      [](base::StringPiece) { return true; });

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(ToPreloadingEligibility(
                           PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique),
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotEligibleDataSaverEnabled) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0);

  // When data saver is enabled, then |PrefetchService| doesn't start the
  // prefetch at all.
  EXPECT_CALL(*mock_prefetch_service_delegate, IsSomePreloadingEnabled)
      .Times(1)
      .WillOnce(testing::Return(PreloadingEligibility::kDataSaverEnabled));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kDataSaverEnabled,
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotEligibleNonHttps) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("http://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(
          PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(PrefetchStatus::kPrefetchProxyNotAvailable),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(PrefetchStatus::kPrefetchIneligibleRetryAfter),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, EligibleNonHttpsNonProxiedPotentiallyTrustworthy) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://localhost"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://localhost"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotEligibleServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://example.com"))));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(
          PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, EligibleServiceWorkerNotRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://other.com"))));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotEligibleUserHasCookies) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(ToPreloadingEligibility(
                           PrefetchStatus::kPrefetchNotEligibleUserHasCookies),
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, EligibleUserHasCookiesForDifferentUrl) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://other.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, EligibleSameOriginPrefetchCanHaveExistingCookies) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/false,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(ToPreloadingEligibility(
                           PrefetchStatus::kPrefetchNotEligibleExistingProxy),
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);

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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/false,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);

  PrefetchService::SetNetworkContextForProxyLookupForTesting(nullptr);
}

TEST_F(PrefetchServiceTest, FailedNon2XXResponseCode) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNon2XX));
}

TEST_F(PrefetchServiceTest, FailedNetError) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNetError));
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

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrefetchStatus::kPrefetchFailedNon2XX));
}

TEST_F(PrefetchServiceTest, SuccessNonHTML) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NotServeableNavigationInDifferentRenderFrameHost) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceLimitedPrefetchesTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"max_srp_prefetches", "2"}}}},
        {network::features::kPrefetchNoVarySearch});
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
  MakePrefetchOnMainFrame(
      GURL("https://example1.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();
  MakePrefetchOnMainFrame(
      GURL("https://example2.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();
  MakePrefetchOnMainFrame(
      GURL("https://example3.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example1.com"));
  ASSERT_TRUE(serveable_prefetch_container1);
  EXPECT_TRUE(serveable_prefetch_container1->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container1->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container1->IsPrefetchServable(
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
      GetPrefetchToServe(GURL("https://example2.com"));
  ASSERT_TRUE(serveable_prefetch_container2);
  EXPECT_TRUE(serveable_prefetch_container2->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container2->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(serveable_prefetch_container2->IsPrefetchServable(
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
      GetPrefetchToServe(GURL("https://example3.com"));
  EXPECT_FALSE(serveable_prefetch_container3);
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 3u);

    // The third entry never reaches the holdback status check.
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/false,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/false,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kUnspecified,
             PreloadingTriggeringOutcome::kUnspecified,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/false)};
    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

class PrefetchServiceWithHTMLOnlyTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"html_only", "true"}}}},
        {network::features::kPrefetchNoVarySearch});
  }
};

TEST_F(PrefetchServiceWithHTMLOnlyTest, FailedNonHTMLWithHTMLOnly) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kFailure,
                       ToPreloadingFailureReason(
                           PrefetchStatus::kPrefetchFailedMIMENotSupported));
}

class PrefetchServiceAlwaysMakeDecoyRequestTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "1"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {network::features::kPrefetchNoVarySearch});
  }
};

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest, DecoyRequest) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

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
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);
  // A decoy is considered a failure.
  ExpectCorrectUkmLogs(
      PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
      PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrefetchStatus::kPrefetchIsPrivacyDecoy));
}

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest,
       NoDecoyRequestDisableDecoysBasedOnUserSettings) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockPrefetchServiceDelegate> mock_prefetch_service_delegate =
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>();

  EXPECT_CALL(*mock_prefetch_service_delegate, DisableDecoysBasedOnUserSettings)
      .Times(1)
      .WillOnce(testing::Return(true));

  MakePrefetchService(std::move(mock_prefetch_service_delegate));

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(ToPreloadingEligibility(
                           PrefetchStatus::kPrefetchNotEligibleUserHasCookies),
                       PreloadingHoldbackStatus::kUnspecified,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceHoldbackTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrefetchUseContentRefactor, {{"prefetch_holdback", "true"}});
  }
};

TEST_F(PrefetchServiceHoldbackTest, PrefetchHeldback) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
  // Holdback is checked and set after eligibility.
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchHeldback));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kHoldback,
                       PreloadingTriggeringOutcome::kUnspecified,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceIncognitoTest : public PrefetchServiceTest {
 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<TestBrowserContext>();
    browser_context->set_is_off_the_record(true);
    return browser_context;
  }
};

TEST_F(PrefetchServiceIncognitoTest, OffTheRecordIneligible) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      static_cast<int>(
          PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(
          PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
}

TEST_F(PrefetchServiceTest, NonDefaultStoragePartition) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  test_content_browser_client_->UseOffTheRecordContextForStoragePartition(true);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
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
      static_cast<int>(
          PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(
      ToPreloadingEligibility(
          PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition),
      PreloadingHoldbackStatus::kUnspecified,
      PreloadingTriggeringOutcome::kUnspecified,
      PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceStreamingURLLoaderTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "-1"},
         {"use_streaming_url_loader", "true"}});
  }
};

// TODO(crbug.com/1396460): Test flaky on lacros trybots.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_StreamingURLLoaderSuccessCase \
  DISABLED_StreamingURLLoaderSuccessCase
#else
#define MAYBE_StreamingURLLoaderSuccessCase StreamingURLLoaderSuccessCase
#endif
TEST_F(PrefetchServiceStreamingURLLoaderTest,
       MAYBE_StreamingURLLoaderSuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  // Send the head of the navigation. The prefetch should be servable after this
  // point. The body of the response will be streaming to the serving URL loader
  // as its received.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));

  // Navigate to the URL before the prefetch response is complete.
  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  // Check the metrics while the prefetch is still in progress.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.NetError",
                                    0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
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
            static_cast<int>(PrefetchStatus::kPrefetchNotFinishedInTime));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotFinishedInTime);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));
  EXPECT_TRUE(serveable_prefetch_container->GetHead());
  EXPECT_TRUE(serveable_prefetch_container->GetHead()->was_in_prefetch_cache);

  // Send the body and completion status of the request, then recheck all of the
  // metrics.
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
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

  referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  serving_page_metrics = GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchSuccessful));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_TRUE(serving_page_metrics->prefetch_header_latency);
  EXPECT_EQ(serving_page_metrics->prefetch_header_latency.value(),
            base::Milliseconds(kHeaderLatency));

  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));
  ASSERT_TRUE(serveable_prefetch_container->GetHead());
  EXPECT_TRUE(serveable_prefetch_container->GetHead()->was_in_prefetch_cache);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceNoVarySearchTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kPrefetchUseContentRefactor,
         network::features::kPrefetchNoVarySearch},
        {});
  }
};

// TODO(crbug.com/1396460): Test flaky on lacros trybots.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NoVarySearchSuccessCase DISABLED_NoVarySearchSuccessCase
#else
#define MAYBE_NoVarySearchSuccessCase NoVarySearchSuccessCase
#endif
TEST_F(PrefetchServiceNoVarySearchTest, MAYBE_NoVarySearchSuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com/?a=1"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      /*enable_no_vary_search_header*/ true);
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com/?a=1"),
                           /*use_prefetch_proxy=*/true);
  MakeResponseAndWait(
      net::HTTP_OK, net::OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", R"(params=("a"))"}},
      kHTMLBody);

  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_prefetch_container);
  EXPECT_EQ(serveable_prefetch_container->GetURL(),
            GURL("https://example.com/?a=1"));
  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));

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

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

// TODO(crbug.com/1396460): Test flaky on lacros trybots.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PrefetchFailedForRedirect DISABLED_PrefetchFailedForRedirect
#else
#define MAYBE_PrefetchFailedForRedirect PrefetchFailedForRedirect
#endif
TEST_F(PrefetchServiceTest, MAYBE_PrefetchFailedForRedirect) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(net::RedirectInfo(),
                         network::mojom::URLResponseHead::New());
  MakeResponseAndWait(
      net::HTTP_OK, net::OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true, {{"X-Testing", "Hello World"}}, kHTMLBody,
      std::move(redirects), network::TestURLLoaderFactory::kResponseDefault);

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
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchFailedRedirectsDisabled));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kFailure,
                       ToPreloadingFailureReason(
                           PrefetchStatus::kPrefetchFailedRedirectsDisabled));
}

class PrefetchServiceNeverBlockUntilHeadTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"block_until_head_eager_prefetch", "false"},
           {"block_until_head_moderate_prefetch", "false"},
           {"block_until_head_conservative_prefetch", "false"}}}},
        {network::features::kPrefetchNoVarySearch});
  }
};

// TODO(crbug.com/1396460): Test flaky on lacros trybots.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_HeadNotReceived DISABLED_HeadNotReceived
#else
#define MAYBE_HeadNotReceived HeadNotReceived
#endif
TEST_F(PrefetchServiceNeverBlockUntilHeadTest, MAYBE_HeadNotReceived) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  // Navigate to the URL before the head of the prefetch response is received.
  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  // Since PrefetchService cannot block until headers for this prefetch, it
  // should immediately return null.
  base::WeakPtr<PrefetchContainer> serveable_prefetch_container =
      GetPrefetchToServe(GURL("https://example.com"));
  EXPECT_FALSE(serveable_prefetch_container);

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
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  absl::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_TRUE(serving_page_metrics->prefetch_status);
  EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
            static_cast<int>(PrefetchStatus::kPrefetchNotFinishedInTime));
  EXPECT_TRUE(serving_page_metrics->required_private_prefetch_proxy);
  EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  EXPECT_FALSE(serving_page_metrics->prefetch_header_latency);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kRunning,
                       PreloadingFailureReason::kUnspecified);
}

class PrefetchServiceAlwaysBlockUntilHeadTest
    : public PrefetchServiceTest,
      public ::testing::WithParamInterface<blink::mojom::SpeculationEagerness> {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"block_until_head_eager_prefetch", "true"},
           {"block_until_head_moderate_prefetch", "true"},
           {"block_until_head_conservative_prefetch", "true"}}}},
        {network::features::kPrefetchNoVarySearch});
  }
};

// TODO(crbug.com/1396460): Test flaky on lacros trybots.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_BlockUntilHeadReceived DISABLED_BlockUntilHeadReceived
#else
#define MAYBE_BlockUntilHeadReceived BlockUntilHeadReceived
#endif
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest, MAYBE_BlockUntilHeadReceived) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true, GetParam()));
  base::RunLoop().RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           /*use_prefetch_proxy=*/true);

  // Navigate to the URL before the head of the prefetch response is received
  Navigate(GURL("https://example.com"), main_rfh()->GetGlobalId());

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::RunLoop get_prefetch_run_loop;
  base::WeakPtr<PrefetchContainer> serveable_prefetch_container;
  prefetch_service_->GetPrefetchToServe(
      GURL("https://example.com"),
      base::BindOnce(
          [](base::WeakPtr<PrefetchContainer>* serveable_prefetch_container,
             base::RunLoop* get_prefetch_run_loop,
             base::WeakPtr<PrefetchContainer> prefetch_to_serve) {
            *serveable_prefetch_container = prefetch_to_serve;
            get_prefetch_run_loop->Quit();
          },
          &serveable_prefetch_container, &get_prefetch_run_loop));
  EXPECT_FALSE(serveable_prefetch_container);

  // Sends the head of the prefetch response. This should trigger the above
  // callback.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  get_prefetch_run_loop.Run();
  ASSERT_TRUE(serveable_prefetch_container);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
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

  EXPECT_TRUE(serveable_prefetch_container->HasPrefetchStatus());
  EXPECT_EQ(serveable_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_TRUE(
      serveable_prefetch_container->IsPrefetchServable(base::TimeDelta::Max()));
  ASSERT_TRUE(serveable_prefetch_container->GetHead());
  EXPECT_TRUE(serveable_prefetch_container->GetHead()->was_in_prefetch_cache);

  ExpectCorrectUkmLogs(PreloadingEligibility::kEligible,
                       PreloadingHoldbackStatus::kAllowed,
                       PreloadingTriggeringOutcome::kReady,
                       PreloadingFailureReason::kUnspecified);
}

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAlwaysBlockUntilHeadTest,
    testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                    blink::mojom::SpeculationEagerness::kConservative));

}  // namespace
}  // namespace content
