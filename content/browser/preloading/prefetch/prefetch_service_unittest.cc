// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include <string_view>
#include <vector>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/browser/preloading/prefetch/mock_prefetch_service_delegate.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#define DISABLED_CHROMEOS(x) DISABLED_##x
#else
#define DISABLED_CHROMEOS(x) x
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_CASTOS)
#define DISABLED_CHROMEOS_AND_CASTOS(x) DISABLED_##x
#else
#define DISABLED_CHROMEOS_AND_CASTOS(x) x
#endif

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
  explicit TestNetworkContext(std::optional<net::ProxyInfo> proxy_info)
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
  std::optional<net::ProxyInfo> proxy_info_;
};

net::RequestPriority ExpectedPriorityForEagerness(
    blink::mojom::SpeculationEagerness eagerness) {
  switch (eagerness) {
    case blink::mojom::SpeculationEagerness::kConservative:
      return net::RequestPriority::MEDIUM;
    case blink::mojom::SpeculationEagerness::kModerate:
      return net::RequestPriority::LOW;
    default:
      return net::RequestPriority::IDLE;
  }
}

class PrefetchFakeServiceWorkerContext : public FakeServiceWorkerContext {
 public:
  explicit PrefetchFakeServiceWorkerContext(
      BrowserTaskEnvironment& task_environment)
      : task_environment_(task_environment) {}

  PrefetchFakeServiceWorkerContext(const PrefetchFakeServiceWorkerContext&) =
      delete;
  PrefetchFakeServiceWorkerContext& operator=(
      const PrefetchFakeServiceWorkerContext&) = delete;

  ~PrefetchFakeServiceWorkerContext() override = default;

  void CheckHasServiceWorker(const GURL& url,
                             const blink::StorageKey& key,
                             CheckHasServiceWorkerCallback callback) override {
    if (long_service_worker_check_duration_.is_positive()) {
      task_environment()->FastForwardBy(long_service_worker_check_duration_);
    }
    if (!MaybeHasRegistrationForStorageKey(key)) {
      std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
      return;
    }
    auto service_worker_info = base::ranges::find_if(
        service_worker_scopes_,
        [url](const std::pair<GURL, ServiceWorkerCapability>&
                  service_worker_info) {
          return base::StartsWith(url.spec(), service_worker_info.first.spec());
        });
    if (service_worker_info != service_worker_scopes_.end()) {
      std::move(callback).Run(service_worker_info->second);
      return;
    }
    std::move(callback).Run(ServiceWorkerCapability::NO_SERVICE_WORKER);
  }

  void AddServiceWorkerScope(const GURL& scope,
                             ServiceWorkerCapability capability) {
    ASSERT_NE(capability, ServiceWorkerCapability::NO_SERVICE_WORKER);
    service_worker_scopes_[scope] = capability;
  }

  void SetServiceWorkerCheckDuration(base::TimeDelta duration) {
    long_service_worker_check_duration_ = duration;
  }

 private:
  BrowserTaskEnvironment* task_environment() {
    return &task_environment_.get();
  }

  base::TimeDelta long_service_worker_check_duration_;
  std::map<GURL, ServiceWorkerCapability> service_worker_scopes_;
  const base::raw_ref<BrowserTaskEnvironment> task_environment_;
};

struct NavigationResult {
  std::unique_ptr<testing::NiceMock<MockNavigationHandle>> navigation_handle;
  base::test::TestFuture<PrefetchContainer::Reader> reader_future;
};

class PrefetchServiceTestBase : public RenderViewHostTestHarness {
 public:
  const int kServiceWorkerCheckDuration = 1000;
  PrefetchServiceTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_url_loader_factory_(/*observe_loader_requests=*/true),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    service_worker_context_ =
        std::make_unique<PrefetchFakeServiceWorkerContext>(*task_environment());
  }

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
        [](std::string_view) { return false; });
    PrefetchService::SetServiceWorkerContextForTesting(
        service_worker_context_.get());

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
    request_handler_keep_alive_.clear();
    service_worker_context_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  virtual void InitScopedFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {});
  }

  void InitScopedFeatureListForNewWaitLoop(bool should_enable) {
    if (should_enable) {
      scoped_feature_list_for_new_wait_loop_.InitWithFeatures(
          {features::kPrefetchNewWaitLoop}, {});
    } else {
      scoped_feature_list_for_new_wait_loop_.InitWithFeatures(
          {}, {features::kPrefetchNewWaitLoop});
    }
  }

  void MakePrefetchService(std::unique_ptr<MockPrefetchServiceDelegate>
                               mock_prefetch_service_delegate) {
    test_content_browser_client_ =
        std::make_unique<ScopedPrefetchServiceContentBrowserClient>(
            std::move(mock_prefetch_service_delegate));

    prefetch_service_ = std::make_unique<PrefetchService>(browser_context());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        prefetch_service_.get());
  }

  // Creates a prefetch request for |url| on the current main frame.
  void MakePrefetchOnMainFrame(
      const GURL& prefetch_url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer = blink::mojom::Referrer(),
      network::mojom::NoVarySearchPtr&& no_vary_search_hint =
          network::mojom::NoVarySearchPtr()) {
    CHECK(prefetch_type.IsRendererInitiated());
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());

    prefetch_document_manager->PrefetchUrl(
        prefetch_url, prefetch_type,
        GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type()),
        /*planned_max_preloading_type=*/PreloadingType::kPrefetch, referrer,
        no_vary_search_hint, nullptr);
  }

  void MakePrefetchFromEmbedder(
      const GURL& prefetch_url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer = blink::mojom::Referrer(),
      const std::optional<url::Origin> referring_origin = std::nullopt) {
    CHECK(!prefetch_type.IsRendererInitiated());

    auto prefetch_container = std::make_unique<PrefetchContainer>(
        *web_contents(), prefetch_url, prefetch_type, referrer,
        std::move(referring_origin), /*no_vary_search_expected=*/std::nullopt,
        /*attempt=*/nullptr);
    prefetch_service_->AddPrefetchContainer(std::move(prefetch_container));
  }

  int RequestCount() { return test_url_loader_factory_.NumPending(); }

  void ClearCompletedRequests() {
    std::vector<network::TestURLLoaderFactory::PendingRequest>* requests =
        test_url_loader_factory_.pending_requests();

    std::erase_if(
        *requests,
        [](const network::TestURLLoaderFactory::PendingRequest& request) {
          return !request.client.is_connected();
        });
  }

  struct VerifyCommonRequestStateOptions {
    bool use_prefetch_proxy = false;
    net::RequestPriority expected_priority = net::RequestPriority::IDLE;
  };

  void VerifyCommonRequestState(const GURL& url) {
    VerifyCommonRequestState(url, {});
  }

  void VerifyCommonRequestState(
      const GURL& url,
      const VerifyCommonRequestStateOptions& options) {
    SCOPED_TRACE(url.spec());
    EXPECT_EQ(RequestCount(), 1);

    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);

    VerifyCommonRequestState(url, options, request);
  }

  void VerifyCommonRequestStateByUrl(const GURL& url) {
    VerifyCommonRequestStateByUrl(url, {});
  }

  void VerifyCommonRequestStateByUrl(
      const GURL& url,
      const VerifyCommonRequestStateOptions& options) {
    SCOPED_TRACE(url.spec());
    auto* pending_request = GetPendingRequestByUrl(url);
    ASSERT_TRUE(pending_request);

    VerifyCommonRequestState(url, options, pending_request);
  }

  // Verify a prefetch attempt is pending (eligible but not started yet, to
  // ensure prefetches are sequential);
  void VerifyPrefetchAttemptIsPending(const GURL& url) {
    PrefetchContainer::Key prefetch_key(MainDocumentToken(), url);
    base::WeakPtr<PrefetchContainer> prefetch_container =
        prefetch_service_->MatchUrl(prefetch_key);
    ASSERT_TRUE(prefetch_container);
    ASSERT_FALSE(prefetch_container->GetResourceRequest());
    ASSERT_EQ(prefetch_container->GetLoadState(),
              PrefetchContainer::LoadState::kEligible);
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

    head->proxy_chain =
        use_prefetch_proxy
            ? net::ProxyChain::FromSchemeHostAndPort(
                  net::ProxyServer::Scheme::SCHEME_HTTPS,
                  PrefetchProxyHost(
                      GURL(MockPrefetchServiceDelegate::kPrefetchProxyAddress))
                      .spec(),
                  std::nullopt)
            : net::ProxyChain::Direct();

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

  void MakeSingleRedirectAndWait(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr redirect_head) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);

    request->client->OnReceiveRedirect(redirect_info, redirect_head.Clone());
    task_environment()->RunUntilIdle();
  }

  void VerifyFollowRedirectParams(size_t expected_follow_redirect_params_size) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->test_url_loader);

    const auto& follow_redirect_params =
        request->test_url_loader->follow_redirect_params();
    EXPECT_EQ(follow_redirect_params.size(),
              expected_follow_redirect_params_size);

    for (const auto& follow_redirect_param : follow_redirect_params) {
      EXPECT_EQ(follow_redirect_param.removed_headers.size(), 0U);
      EXPECT_TRUE(follow_redirect_param.modified_headers.IsEmpty());
      EXPECT_TRUE(follow_redirect_param.modified_cors_exempt_headers.IsEmpty());
      EXPECT_FALSE(follow_redirect_param.new_url);
    }
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

    auto head = CreateURLResponseHeadForPrefetch(http_status, mime_type,
                                                 use_prefetch_proxy, headers,
                                                 request->request.url);
    network::URLLoaderCompletionStatus status(net_error);
    test_url_loader_factory_.AddResponse(request->request.url, std::move(head),
                                         body, status);
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
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_FALSE(producer_handle_for_gurl_.count(request->request.url));
    ASSERT_TRUE(request);
    SendHeadOfResponseAndWait(http_status, mime_type, use_prefetch_proxy,
                              headers, expected_total_body_size, request);
    ASSERT_TRUE(producer_handle_for_gurl_.count(request->request.url));
  }

  void SendHeadOfResponseForUrlAndWait(
      const GURL& request_url,
      net::HttpStatusCode http_status,
      const std::string mime_type,
      bool use_prefetch_proxy,
      std::vector<std::pair<std::string, std::string>> headers,
      uint32_t expected_total_body_size) {
    auto* request = GetPendingRequestByUrl(request_url);

    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);
    ASSERT_FALSE(producer_handle_for_gurl_.count(request->request.url));
    SendHeadOfResponseAndWait(http_status, mime_type, use_prefetch_proxy,
                              headers, expected_total_body_size, request);
    ASSERT_TRUE(producer_handle_for_gurl_.count(request->request.url));
  }

  void SendBodyContentOfResponseAndWait(const std::string& body) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    SendBodyContentOfResponseAndWait(body, request);
  }

  void SendBodyContentOfResponseForUrlAndWait(const GURL& url,
                                              const std::string& body) {
    auto* request = GetPendingRequestByUrl(url);
    ASSERT_TRUE(request);
    SendBodyContentOfResponseAndWait(body, request);
  }

  void CompleteResponseAndWait(net::Error net_error,
                               uint32_t expected_total_body_size) {
    network::TestURLLoaderFactory::PendingRequest* request =
        test_url_loader_factory_.GetPendingRequest(0);
    ASSERT_TRUE(request);
    CompleteResponseAndWait(net_error, expected_total_body_size, request);
  }

  void CompleteResponseForUrlAndWait(const GURL& url,
                                     net::Error net_error,
                                     uint32_t expected_total_body_size) {
    auto* request = GetPendingRequestByUrl(url);
    ASSERT_TRUE(request);
    CompleteResponseAndWait(net_error, expected_total_body_size, request);
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateForTesting(url, value, base::Time::Now()));

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

  void Navigate(
      const GURL& url,
      int initiator_process_id,
      const std::optional<blink::LocalFrameToken>& initiator_local_frame_token,
      const std::optional<blink::DocumentToken>& initiator_document_token) {
    mock_navigation_handle_ =
        std::make_unique<testing::NiceMock<MockNavigationHandle>>(
            web_contents());
    mock_navigation_handle_->set_url(url);
    mock_navigation_handle_->set_initiator_process_id(initiator_process_id);
    mock_navigation_handle_->set_initiator_frame_token(
        base::OptionalToPtr(initiator_local_frame_token));

    // Simulate how `NavigationRequest` calls
    // `PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle()`.
    if (initiator_document_token &&
        PrefetchDocumentManager::FromDocumentToken(initiator_process_id,
                                                   *initiator_document_token)) {
      PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle(
          *mock_navigation_handle_);
    }
  }

  void NavigateInitiatedByRenderer(const GURL& url) {
    Navigate(url, main_rfh()->GetProcess()->GetID(),
             main_rfh()->GetFrameToken(), MainDocumentToken());
  }

  void NavigateInitiatedByBrowser(const GURL& url) {
    Navigate(url, ChildProcessHost::kInvalidUniqueID, std::nullopt,
             std::nullopt);
  }

  std::optional<PrefetchServingPageMetrics>
  GetMetricsForMostRecentNavigation() {
    if (!mock_navigation_handle_)
      return std::nullopt;

    return PrefetchServingPageMetrics::GetForNavigationHandle(
        *mock_navigation_handle_);
  }

  PrefetchMatchResolver* GetPrefetchMatchResolverForMostRecentNavigation() {
    if (!mock_navigation_handle_) {
      return nullptr;
    }

    PrefetchMatchResolver::CreateForNavigationHandle(*mock_navigation_handle_);
    return PrefetchMatchResolver::GetForNavigationHandle(
        *mock_navigation_handle_);
  }

  base::WeakPtr<PrefetchServingPageMetricsContainer>
  GetServingPageMetricsContainerForMostRecentNavigation() {
    if (!mock_navigation_handle_) {
      return nullptr;
    }

    auto* serving_page_metrics_container =
        PrefetchServingPageMetricsContainer::GetForNavigationHandle(
            *mock_navigation_handle_);
    if (!serving_page_metrics_container) {
      return nullptr;
    }
    return serving_page_metrics_container->GetWeakPtr();
  }

  blink::DocumentToken MainDocumentToken() {
    return static_cast<RenderFrameHostImpl*>(main_rfh())->GetDocumentToken();
  }

  void GetPrefetchToServe(
      base::test::TestFuture<PrefetchContainer::Reader>& future,
      const GURL& url,
      std::optional<blink::DocumentToken> initiator_document_token) {
    auto callback = base::BindOnce(
        [](base::test::TestFuture<PrefetchContainer::Reader>* future,
           std::vector<PrefetchRequestHandler>* request_handler_keep_alive,
           PrefetchContainer::Reader prefetch_to_serve) {
          if (prefetch_to_serve) {
            // When GetPrefetchToServe() is successful, also call
            // `CreateRequestHandler()` to simulate
            // `PrefetchURLLoaderInterceptor::OnGetPrefetchComplete()` behavior,
            // which is the primary non-test `GetPrefetchToServe()` code path.
            auto request_handler = prefetch_to_serve.CreateRequestHandler();
            CHECK(request_handler);
            // Keep-alive the PrefetchRequestHandler, because destructing
            // `request_handler` here can signal that the serving is finished,
            // and e.g. close body mojo pipe.
            request_handler_keep_alive->push_back(std::move(request_handler));
          }
          future->SetValue(std::move(prefetch_to_serve));
        },
        base::Unretained(&future),
        base::Unretained(&request_handler_keep_alive_));
    if (UseNewWaitLoop()) {
      auto key = PrefetchContainer::Key(initiator_document_token, url);
      PrefetchMatchResolver2::FindPrefetch(
          std::move(key), *prefetch_service_.get(),
          GetServingPageMetricsContainerForMostRecentNavigation(),
          std::move(callback));
    } else {
      PrefetchMatchResolver* prefetch_match_resolver =
          GetPrefetchMatchResolverForMostRecentNavigation();
      prefetch_match_resolver->SetOnPrefetchToServeReadyCallback(
          std::move(callback));
      prefetch_service_->GetPrefetchToServe(
          PrefetchContainer::Key(initiator_document_token, url),
          GetServingPageMetricsContainerForMostRecentNavigation(),
          *prefetch_match_resolver);
    }
  }

  PrefetchContainer::Reader GetPrefetchToServe(
      const GURL& url,
      std::optional<blink::DocumentToken> initiator_document_token) {
    base::test::TestFuture<PrefetchContainer::Reader> future;
    GetPrefetchToServe(future, url, std::move(initiator_document_token));
    return future.Take();
  }

  PrefetchContainer::Reader GetPrefetchToServe(const GURL& url) {
    return GetPrefetchToServe(url, MainDocumentToken());
  }

  // Simulates part of navigation.
  //
  // - Creates MockNavigationHandle.
  // - Starts matching of prefetch `PrefetchMatchResolver2::FindPrefetch()`.
  //
  // Note that these are very small part of navigation. For example,
  // post-matching process `OnGotPrefetchToServe()` and redirects are not
  // handled.
  std::unique_ptr<NavigationResult> SimulatePartOfNavigation(
      const GURL& url,
      bool is_renderer_initiated) {
    return is_renderer_initiated
               ? SimulatePartOfNavigation(
                     url, main_rfh()->GetProcess()->GetID(),
                     main_rfh()->GetFrameToken(), MainDocumentToken())
               : SimulatePartOfNavigation(url,
                                          ChildProcessHost::kInvalidUniqueID,
                                          std::nullopt, std::nullopt);
  }

  std::unique_ptr<NavigationResult> SimulatePartOfNavigation(
      const GURL& url,
      int initiator_process_id,
      const std::optional<blink::LocalFrameToken>& initiator_local_frame_token,
      const std::optional<blink::DocumentToken>& initiator_document_token) {
    CHECK(UseNewWaitLoop());

    // Use std::unique_ptr as the below uses raw pointers and references.
    auto res = std::make_unique<NavigationResult>();

    res->navigation_handle =
        std::make_unique<testing::NiceMock<MockNavigationHandle>>(
            web_contents());
    res->navigation_handle->set_url(url);
    res->navigation_handle->set_initiator_process_id(initiator_process_id);
    res->navigation_handle->set_initiator_frame_token(
        base::OptionalToPtr(initiator_local_frame_token));

    // Simulate how `NavigationRequest` calls
    // `PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle()`.
    if (initiator_document_token &&
        PrefetchDocumentManager::FromDocumentToken(initiator_process_id,
                                                   *initiator_document_token)) {
      PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle(
          *res->navigation_handle);
    }

    auto callback = base::BindOnce(
        [](base::test::TestFuture<PrefetchContainer::Reader>& reader_future,
           PrefetchContainer::Reader reader) {
          reader_future.SetValue(std::move(reader));
        },
        std::ref(res->reader_future));
    auto serving_page_metrics_container =
        [&res]() -> base::WeakPtr<PrefetchServingPageMetricsContainer> {
      auto* serving_page_metrics_container =
          PrefetchServingPageMetricsContainer::GetForNavigationHandle(
              *res->navigation_handle);
      if (!serving_page_metrics_container) {
        return nullptr;
      }

      return serving_page_metrics_container->GetWeakPtr();
    }();
    auto key = PrefetchContainer::Key(initiator_document_token, url);
    PrefetchMatchResolver2::FindPrefetch(
        std::move(key), *prefetch_service_.get(),
        std::move(serving_page_metrics_container), std::move(callback));

    return res;
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

  // ##### Helpers for prefetching-related metrics #####
  // Tests prefetching-side (i.e. not serving-side) metrics including
  // "PrefetchProxy.Prefetch.*" UMAs, `PrefetchReferringPageMetrics` and
  // Preloading_Attempt UKMs.

  // Prefetch didn't receive any net errors nor non-redirect responses.
  // Use more specific methods below to check UKMs, if applicable.
  void ExpectPrefetchNoNetErrorOrResponseReceived(
      const base::HistogramTester& histogram_tester,
      bool is_eligible) {
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.RespCode", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.NetError", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

    std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
        PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
    EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_eligible_count,
              is_eligible ? 1 : 0);
    EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);
  }

  // Prefetch was not started because it was not eligible.
  void ExpectPrefetchNotEligible(const base::HistogramTester& histogram_tester,
                                 PreloadingEligibility expected_eligibility) {
    ExpectPrefetchNoNetErrorOrResponseReceived(histogram_tester,
                                               /*is_eligible=*/false);
    ExpectCorrectUkmLogs(
        {.eligibility = expected_eligibility,
         .holdback = PreloadingHoldbackStatus::kUnspecified,
         .outcome = PreloadingTriggeringOutcome::kUnspecified});
  }

  // Prefetch was started but failed before the final response nor any network
  // error is received.
  void ExpectPrefetchFailedBeforeResponseReceived(
      const base::HistogramTester& histogram_tester,
      PrefetchStatus expected_prefetch_status,
      bool is_accurate = false) {
    ExpectPrefetchNoNetErrorOrResponseReceived(histogram_tester,
                                               /*is_eligible=*/true);
    ExpectCorrectUkmLogs(
        {.outcome = PreloadingTriggeringOutcome::kFailure,
         .failure = ToPreloadingFailureReason(expected_prefetch_status),
         .is_accurate = is_accurate});
  }

  // Prefetch was started but failed due to a network error, before the final
  // response is received.
  void ExpectPrefetchFailedNetError(
      const base::HistogramTester& histogram_tester,
      int expected_net_error_code,
      blink::mojom::SpeculationEagerness eagerness =
          blink::mojom::SpeculationEagerness::kEager,
      bool is_accurate_triggering = false) {
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);

    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.RespCode", 0);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.NetError",
        std::abs(expected_net_error_code), 1);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
    histogram_tester.ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

    std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
        PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
    EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

    ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kFailure,
                          .failure = ToPreloadingFailureReason(
                              PrefetchStatus::kPrefetchFailedNetError),
                          .is_accurate = is_accurate_triggering,
                          .eagerness = eagerness});
  }

  // Prefetch was started but failed on or after the final response is
  // received.
  void ExpectPrefetchFailedAfterResponseReceived(
      const base::HistogramTester& histogram_tester,
      net::HttpStatusCode expected_response_code,
      int expected_body_length,
      PrefetchStatus expected_prefetch_status) {
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);

    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.RespCode", expected_response_code, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.BodyLength", expected_body_length, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
        1);

    std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
        PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
    EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

    ExpectCorrectUkmLogs(
        {.outcome = PreloadingTriggeringOutcome::kFailure,
         .failure = ToPreloadingFailureReason(expected_prefetch_status)});
  }

  void ExpectPrefetchSuccess(const base::HistogramTester& histogram_tester,
                             int expected_body_length,
                             blink::mojom::SpeculationEagerness eagerness =
                                 blink::mojom::SpeculationEagerness::kEager,
                             bool is_accurate = false) {
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);

    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.BodyLength", expected_body_length, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
    histogram_tester.ExpectUniqueSample(
        "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration,
        1);

    std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
        PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
    EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

    ExpectCorrectUkmLogs({.is_accurate = is_accurate, .eagerness = eagerness});
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

  struct ExpectCorrectUkmLogsArgs {
    PreloadingEligibility eligibility = PreloadingEligibility::kEligible;
    PreloadingHoldbackStatus holdback = PreloadingHoldbackStatus::kAllowed;
    PreloadingTriggeringOutcome outcome = PreloadingTriggeringOutcome::kReady;
    PreloadingFailureReason failure = PreloadingFailureReason::kUnspecified;
    bool is_accurate = false;
    bool expect_ready_time = false;
    blink::mojom::SpeculationEagerness eagerness =
        blink::mojom::SpeculationEagerness::kEager;
  };
  void ExpectCorrectUkmLogs(ExpectCorrectUkmLogsArgs args) {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 1u);

    std::optional<base::TimeDelta> ready_time = std::nullopt;
    if (args.outcome == PreloadingTriggeringOutcome::kReady ||
        args.outcome == PreloadingTriggeringOutcome::kSuccess ||
        args.expect_ready_time) {
      ready_time = base::ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }

    const auto expected_attempts = {attempt_entry_builder()->BuildEntry(
        source_id, PreloadingType::kPrefetch, args.eligibility, args.holdback,
        args.outcome, args.failure, args.is_accurate, ready_time,
        args.eagerness)};

    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
    // We do not test the `PreloadingPrediction` as it is added in
    // `PreloadingDecider`.
  }

  // ##### Helpers for serving-related metrics #####
  static void ExpectServingMetrics(
      const base::Location& location,
      std::optional<PrefetchServingPageMetrics> serving_page_metrics,
      PrefetchStatus expected_prefetch_status,
      std::optional<base::TimeDelta> prefetch_header_latency,
      bool required_private_prefetch_proxy) {
    SCOPED_TRACE(::testing::Message() << "callsite: " << location.ToString());

    ASSERT_TRUE(serving_page_metrics);
    ASSERT_TRUE(serving_page_metrics->prefetch_status);
    EXPECT_EQ(serving_page_metrics->prefetch_status.value(),
              static_cast<int>(expected_prefetch_status));
    EXPECT_EQ(serving_page_metrics->prefetch_header_latency,
              prefetch_header_latency);
    EXPECT_EQ(serving_page_metrics->required_private_prefetch_proxy,
              required_private_prefetch_proxy);
    EXPECT_TRUE(serving_page_metrics->same_tab_as_prefetching_tab);
  }

  void ExpectServingMetrics(PrefetchStatus expected_prefetch_status,
                            bool prefetch_header_latency = false,
                            bool required_private_prefetch_proxy = true) {
    // std::optional<base::TimeDelta> prefetch_header_latency_value =
    // prefetch_header_latency ? base::Milliseconds(kHeaderLatency) :
    // std::nullopt;
    std::optional<base::TimeDelta> prefetch_header_latency_value;
    if (prefetch_header_latency) {
      prefetch_header_latency_value = base::Milliseconds(kHeaderLatency);
    }
    ExpectServingMetrics(FROM_HERE, GetMetricsForMostRecentNavigation(),
                         expected_prefetch_status,
                         std::move(prefetch_header_latency_value),
                         required_private_prefetch_proxy);
  }

  void ExpectServingMetricsSuccess(
      bool required_private_prefetch_proxy = true) {
    ExpectServingMetrics(PrefetchStatus::kPrefetchSuccessful,
                         /*prefetch_header_latency=*/true,
                         required_private_prefetch_proxy);
  }

  struct ExpectServingMetricsArgs {
    PrefetchStatus prefetch_status;
    std::optional<base::TimeDelta> prefetch_header_latency;
    bool required_private_prefetch_proxy;
  };
  static void ExpectServingMetrics(
      const base::Location& location,
      const std::unique_ptr<NavigationResult>& nav_res,
      ExpectServingMetricsArgs args) {
    ExpectServingMetrics(location,
                         PrefetchServingPageMetrics::GetForNavigationHandle(
                             *nav_res->navigation_handle),
                         args.prefetch_status, args.prefetch_header_latency,
                         args.required_private_prefetch_proxy);
  }

  static void ExpectServingReaderSuccess(
      const PrefetchContainer::Reader& serveable_reader) {
    ExpectServingReaderSuccess(FROM_HERE, serveable_reader);
  }

  static void ExpectServingReaderSuccess(
      const base::Location& location,
      const PrefetchContainer::Reader& serveable_reader) {
    SCOPED_TRACE(::testing::Message() << "callsite: " << location.ToString());

    ASSERT_TRUE(serveable_reader);
    EXPECT_TRUE(serveable_reader.HasPrefetchStatus());
    EXPECT_EQ(serveable_reader.GetPrefetchStatus(),
              PrefetchStatus::kPrefetchSuccessful);
    EXPECT_EQ(serveable_reader.GetServableState(base::TimeDelta::Max()),
              PrefetchContainer::ServableState::kServable);
    ASSERT_TRUE(serveable_reader.GetPrefetchContainer()->GetNonRedirectHead());
    EXPECT_TRUE(serveable_reader.GetPrefetchContainer()
                    ->GetNonRedirectHead()
                    ->was_in_prefetch_cache);
  }

 protected:
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequestByUrl(
      const GURL& url) {
    auto& pending_requests = *test_url_loader_factory_.pending_requests();
    auto it = base::ranges::find(pending_requests, url,
                                 [](const auto& pending_request) {
                                   return pending_request.request.url;
                                 });
    if (it == pending_requests.end()) {
      return nullptr;
    }
    network::TestURLLoaderFactory::PendingRequest* request = &*it;
    return request;
  }

  void VerifyCommonRequestState(
      const GURL& url,
      const VerifyCommonRequestStateOptions& options,
      const network::TestURLLoaderFactory::PendingRequest* request) {
    ASSERT_TRUE(request);

    EXPECT_EQ(request->request.url, url);
    EXPECT_EQ(request->request.method, "GET");
    EXPECT_TRUE(request->request.enable_load_timing);
    EXPECT_EQ(request->request.load_flags, net::LOAD_PREFETCH);
    EXPECT_EQ(request->request.credentials_mode,
              network::mojom::CredentialsMode::kInclude);

    EXPECT_THAT(request->request.headers.GetHeader("Purpose"),
                testing::Optional(std::string("prefetch")));

    EXPECT_THAT(request->request.headers.GetHeader("Sec-Purpose"),
                testing::Optional(std::string(
                    options.use_prefetch_proxy ? "prefetch;anonymous-client-ip"
                                               : "prefetch")));

    EXPECT_THAT(request->request.headers.GetHeader("Accept"),
                testing::Optional(FrameAcceptHeaderValue(
                    /*allow_sxg_responses=*/true, browser_context())));

    EXPECT_THAT(request->request.headers.GetHeader("Upgrade-Insecure-Requests"),
                testing::Optional(std::string("1")));

    ASSERT_TRUE(request->request.trusted_params.has_value());
    VerifyIsolationInfo(request->request.trusted_params->isolation_info);

    EXPECT_EQ(request->request.priority, options.expected_priority);
  }

  void SendHeadOfResponseAndWait(
      net::HttpStatusCode http_status,
      const std::string mime_type,
      bool use_prefetch_proxy,
      std::vector<std::pair<std::string, std::string>> headers,
      uint32_t expected_total_body_size,
      network::TestURLLoaderFactory::PendingRequest* request) {
    ASSERT_FALSE(producer_handle_for_gurl_.count(request->request.url));
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);

    auto head = CreateURLResponseHeadForPrefetch(http_status, mime_type,
                                                 use_prefetch_proxy, headers,
                                                 request->request.url);

    mojo::ScopedDataPipeConsumerHandle body;
    EXPECT_EQ(mojo::CreateDataPipe(
                  expected_total_body_size,
                  producer_handle_for_gurl_[request->request.url], body),
              MOJO_RESULT_OK);

    request->client->OnReceiveResponse(std::move(head), std::move(body),
                                       std::nullopt);
    task_environment()->RunUntilIdle();
  }

  void SendBodyContentOfResponseAndWait(
      const std::string& body,
      network::TestURLLoaderFactory::PendingRequest* request) {
    ASSERT_TRUE(producer_handle_for_gurl_.count(request->request.url));
    ASSERT_TRUE(producer_handle_for_gurl_[request->request.url]);

    EXPECT_EQ(producer_handle_for_gurl_[request->request.url]->WriteAllData(
                  base::as_byte_span(body)),
              MOJO_RESULT_OK);
    // Ok to ignore `actually_written_bytes` because of `...ALL_OR_NONE`.
    task_environment()->RunUntilIdle();
  }

  void CompleteResponseAndWait(
      net::Error net_error,
      uint32_t expected_total_body_size,
      network::TestURLLoaderFactory::PendingRequest* request) {
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->client);
    if (producer_handle_for_gurl_.count(request->request.url)) {
      producer_handle_for_gurl_[request->request.url].reset();
      producer_handle_for_gurl_.erase(request->request.url);
    }

    network::URLLoaderCompletionStatus completion_status(net_error);
    completion_status.decoded_body_length = expected_total_body_size;
    request->client->OnComplete(completion_status);
    task_environment()->RunUntilIdle();

    test_url_loader_factory_.ClearResponses();
  }

  std::unique_ptr<PrefetchFakeServiceWorkerContext> service_worker_context_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  base::test::ScopedFeatureList scoped_feature_list_for_new_wait_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;
  std::unique_ptr<PrefetchService> prefetch_service_;

  std::unique_ptr<testing::NiceMock<MockNavigationHandle>>
      mock_navigation_handle_;

  std::unique_ptr<ScopedPrefetchServiceContentBrowserClient>
      test_content_browser_client_;

  std::map<GURL, mojo::ScopedDataPipeProducerHandle> producer_handle_for_gurl_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;

  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;

  std::vector<PrefetchRequestHandler> request_handler_keep_alive_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState};
};

class PrefetchServiceTest : public PrefetchServiceTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
    PrefetchServiceTestBase::InitScopedFeatureList();
  }
};

INSTANTIATE_TEST_SUITE_P(, PrefetchServiceTest, testing::Bool());

TEST_P(PrefetchServiceTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 1);

  if (UseNewWaitLoop()) {
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("PrefetchProxy.AfterClick."
                           "PrefetchMatchingBlockedNavigationWithPrefetch.%s",
                           GetPrefetchEagernessHistogramSuffix(
                               blink::mojom::SpeculationEagerness::kEager)
                               .c_str()),
        false, 1);
  } else {
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
            GetPrefetchEagernessHistogramSuffix(
                blink::mojom::SpeculationEagerness::kEager)
                .c_str()),
        false, 1);
  }
}

TEST_P(PrefetchServiceTest, PrefetchDoesNotMatchIfDocumentTokenDoesNotMatch) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // No servable PrefetchContainer is returned for different DocumentToken.
  blink::DocumentToken different_document_token;
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com"),
                                  different_document_token));
}

TEST_P(PrefetchServiceTest, SuccessCase_Embedder) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);

  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  MakePrefetchFromEmbedder(GURL("https://example.com"),
                           PrefetchType(PreloadingTriggerType::kEmbedder,
                                        /*use_prefetch_proxy=*/true));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Verify that the prefetch request was successful.
  // TODO(crbug.com/40269462): Revise current helper functions (ExpectPrefetch*)
  // for browser-initiated prefetch.
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

  NavigateInitiatedByBrowser(GURL("https://example.com"));

  ExpectServingReaderSuccess(
      GetPrefetchToServe(GURL("https://example.com"), std::nullopt));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 1);
}

TEST_P(PrefetchServiceTest,
       PrefetchDoesNotMatchIfDocumentTokenDoesNotMatch_Embedder) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);

  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  MakePrefetchFromEmbedder(GURL("https://example.com"),
                           PrefetchType(PreloadingTriggerType::kEmbedder,
                                        /*use_prefetch_proxy=*/true));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Verify that the prefetch request was successful.
  // TODO(crbug.com/40269462): Revise current helper functions (ExpectPrefetch*)
  // for browser-initiated prefetch.
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

  NavigateInitiatedByBrowser(GURL("https://example.com"));

  // No servable PrefetchContainer is returned for different DocumentToken.
  EXPECT_FALSE(
      GetPrefetchToServe(GURL("https://example.com"), MainDocumentToken()));
}

TEST_P(PrefetchServiceTest, NoPrefetchingPreloadingDisabled) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kPreloadingDisabled);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligiblePreloadingDisabled);
}

TEST_P(PrefetchServiceTest, NoPrefetchingDomainNotInAllowList) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  // `IsDomainInPrefetchAllowList` returns false so we did not reach the
  // eligibility check.
  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kUnspecified);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  std::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_FALSE(serving_page_metrics->prefetch_status);
}

class PrefetchServiceAllowAllDomainsTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains", "true"}}}},
        {});
  }
};

INSTANTIATE_TEST_SUITE_P(, PrefetchServiceAllowAllDomainsTest, testing::Bool());

TEST_P(PrefetchServiceAllowAllDomainsTest, AllowAllDomains) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

class PrefetchServiceAllowAllDomainsForExtendedPreloadingTest
    : public PrefetchServiceTestBase {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains_for_extended_preloading", "true"}}}},
        {});
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kUnspecified);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  std::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  ASSERT_TRUE(serving_page_metrics);
  EXPECT_FALSE(serving_page_metrics->prefetch_status);
}

TEST_P(PrefetchServiceTest, NonProxiedPrefetchDoesNotRequireAllowList) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = false});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);
}

TEST_P(PrefetchServiceTest, NotEligibleHostnameNonUnique) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  PrefetchService::SetHostNonUniqueFilterForTesting(
      [](std::string_view) { return true; });

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kHostIsNonUnique);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleHostIsNonUnique);
}

TEST_P(PrefetchServiceTest, NotEligibleDataSaverEnabled) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kDataSaverEnabled);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleDataSaverEnabled);
}

TEST_P(PrefetchServiceTest, NotEligibleNonHttps) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("http://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kSchemeIsNotHttps);

  NavigateInitiatedByRenderer(GURL("http://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("http://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps);
}

TEST_P(PrefetchServiceTest, NotEligiblePrefetchProxyNotAvailable) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kPrefetchProxyNotAvailable);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(
      PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable);
}

TEST_P(PrefetchServiceTest,
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);
}

TEST_P(PrefetchServiceTest, NotEligibleOriginWithinRetryAfterWindow) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kRetryAfter);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleRetryAfter);
}

TEST_P(PrefetchServiceTest, EligibleNonHttpsNonProxiedPotentiallyTrustworthy) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://localhost"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://localhost"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://localhost"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://localhost")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);
}

TEST_P(PrefetchServiceTest, NotEligibleServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://example.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kUserHasServiceWorker);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker);
}

TEST_P(PrefetchServiceTest,
       NotEligibleServiceWorkerRegisteredServiceWorkerCheckUKM) {
  // ukm::TestAutoSetUkmRecorder ukm_recorder;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://example.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
  service_worker_context_->SetServiceWorkerCheckDuration(
      base::Microseconds(kServiceWorkerCheckDuration));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  ForceLogsUploadAndGetUkmId();
  // Now check the UKM records.
  using UkmEntry = ukm::builders::Preloading_Attempt;
  auto actual_attempts = test_ukm_recorder()->GetEntries(
      UkmEntry::kEntryName,
      {
          UkmEntry::kPrefetchServiceWorkerRegisteredCheckName,
          UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName,
      });
  EXPECT_EQ(actual_attempts.size(), 1u);
  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredCheckName));
  EXPECT_EQ(actual_attempts[0]
                .metrics[UkmEntry::kPrefetchServiceWorkerRegisteredCheckName],
            static_cast<int64_t>(
                PreloadingAttemptImpl::ServiceWorkerRegisteredCheck::kPath));
  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName));
  EXPECT_EQ(
      actual_attempts[0].metrics
          [UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName],
      ukm::GetExponentialBucketMin(
          kServiceWorkerCheckDuration,
          PreloadingAttemptImpl::
              kServiceWorkerRegisteredCheckDurationBucketSpacing));
}

TEST_P(PrefetchServiceTest, EligibleServiceWorkerNotRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://other.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://other.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

TEST_P(PrefetchServiceTest,
       EligibleServiceWorkerNotRegisteredServiceWorkerCheckUKM) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://other.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://other.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));

  ForceLogsUploadAndGetUkmId();
  // Now check the UKM records.
  using UkmEntry = ukm::builders::Preloading_Attempt;
  auto actual_attempts = test_ukm_recorder()->GetEntries(
      UkmEntry::kEntryName,
      {
          UkmEntry::kPrefetchServiceWorkerRegisteredCheckName,
          UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName,
      });
  EXPECT_EQ(actual_attempts.size(), 1u);

  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredCheckName));
  EXPECT_EQ(
      actual_attempts[0]
          .metrics[UkmEntry::kPrefetchServiceWorkerRegisteredCheckName],
      static_cast<int64_t>(
          PreloadingAttemptImpl::ServiceWorkerRegisteredCheck::kOriginOnly));
  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName));
  EXPECT_EQ(
      actual_attempts[0].metrics
          [UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName],
      ukm::GetExponentialBucketMin(
          0, PreloadingAttemptImpl::
                 kServiceWorkerRegisteredCheckDurationBucketSpacing));
}

TEST_P(PrefetchServiceTest, EligibleServiceWorkerRegistered) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://example.com"),
      ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

TEST_P(PrefetchServiceTest,
       EligibleServiceWorkerRegisteredServiceWorkerCheckUKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://example.com"),
      ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER);
  service_worker_context_->SetServiceWorkerCheckDuration(
      base::Microseconds(kServiceWorkerCheckDuration));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 1);

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  ForceLogsUploadAndGetUkmId();
  // Now check the UKM records.
  using UkmEntry = ukm::builders::Preloading_Attempt;
  auto actual_attempts = test_ukm_recorder()->GetEntries(
      UkmEntry::kEntryName,
      {
          UkmEntry::kPrefetchServiceWorkerRegisteredCheckName,
          UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName,
      });
  EXPECT_EQ(actual_attempts.size(), 1u);

  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredCheckName));
  EXPECT_EQ(actual_attempts[0]
                .metrics[UkmEntry::kPrefetchServiceWorkerRegisteredCheckName],
            static_cast<int64_t>(
                PreloadingAttemptImpl::ServiceWorkerRegisteredCheck::kPath));
  ASSERT_TRUE(actual_attempts[0].metrics.count(
      UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName));
  EXPECT_EQ(
      actual_attempts[0].metrics
          [UkmEntry::kPrefetchServiceWorkerRegisteredForURLCheckDurationName],
      ukm::GetExponentialBucketMin(
          kServiceWorkerCheckDuration,
          PreloadingAttemptImpl::
              kServiceWorkerRegisteredCheckDurationBucketSpacing));
}

TEST_P(PrefetchServiceTest, EligibleServiceWorkerNotRegisteredAtThisPath) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://example.com/sw"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com/non_sw/index.html"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com/non_sw/index.html"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com/non_sw/index.html"));
  ExpectServingReaderSuccess(
      GetPrefetchToServe(GURL("https://example.com/non_sw/index.html")));
  ExpectServingMetricsSuccess();
}

TEST_P(PrefetchServiceTest, NotEligibleUserHasCookies) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kUserHasCookies);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleUserHasCookies);
}

TEST_P(PrefetchServiceTest, EligibleUserHasCookiesForDifferentUrl) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://other.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

TEST_P(PrefetchServiceTest, EligibleSameOriginPrefetchCanHaveExistingCookies) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(FailedCookiesChangedAfterPrefetchStarted)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Adding a cookie after the prefetch has started will cause it to fail when
  // being served.
  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));
  task_environment()->RunUntilIdle();

  NavigateInitiatedByRenderer(GURL("https://example.com"));

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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  ExpectServingMetrics(PrefetchStatus::kPrefetchNotUsedCookiesChanged,
                       /*prefetch_header_latency=*/true);

  // ReadyTime will be included in the UKM, because the prefetch was ready, and
  // then failed.
  ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kFailure,
                        .failure = ToPreloadingFailureReason(
                            PrefetchStatus::kPrefetchNotUsedCookiesChanged),
                        .is_accurate = true,
                        .expect_ready_time = true});
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(SameOriginPrefetchIgnoresProxyRequirement)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  // Make a same-origin prefetch that requires the proxy. The proxy requirement
  // is only enforced for cross-origin requests.
  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  // serving_page_metrics->required_private_prefetch_proxy will be true if the
  // prefetch is marked as requiring the proxy when cross origin, even if only
  // prefetch request was same-origin.
  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(NotEligibleSameSiteCrossOriginPrefetchRequiresProxy)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  // Make a same-site cross-origin prefetch that requires the proxy. These types
  // of prefetches are blocked.
  MakePrefetchOnMainFrame(
      GURL("https://other.example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(
      histogram_tester,
      PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy);

  NavigateInitiatedByRenderer(GURL("https://other.example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://other.example.com")));
  ExpectServingMetrics(
      PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy);
}

TEST_P(PrefetchServiceTest, NotEligibleExistingConnectProxy) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kExistingProxy);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleExistingProxy);

  PrefetchService::SetNetworkContextForProxyLookupForTesting(nullptr);
}

TEST_P(PrefetchServiceTest, EligibleExistingConnectProxyButSameOriginPrefetch) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  PrefetchService::SetNetworkContextForProxyLookupForTesting(nullptr);
}

TEST_P(PrefetchServiceTest, FailedNon2XXResponseCode) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_NOT_FOUND, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchFailedAfterResponseReceived(
      histogram_tester, net::HTTP_NOT_FOUND, std::size(kHTMLBody),
      PrefetchStatus::kPrefetchFailedNon2XX);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNon2XX,
                       /*prefetch_header_latency=*/true);
}

TEST_P(PrefetchServiceTest, FailedNetError) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_FAILED);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError);
}

TEST_P(PrefetchServiceTest, HandleRetryAfterResponse) {
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  // Simulate the origin responding with a "retry-after" header.
  MakeResponseAndWait(net::HTTP_SERVICE_UNAVAILABLE, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"Retry-After", "1234"}, {"X-Testing", "Hello World"}},
                      "");

  ExpectPrefetchFailedAfterResponseReceived(
      histogram_tester, net::HTTP_SERVICE_UNAVAILABLE, 0,
      PrefetchStatus::kPrefetchFailedNon2XX);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNon2XX,
                       /*prefetch_header_latency=*/true);
}

TEST_P(PrefetchServiceTest, SuccessNonHTML) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  std::string body = "fake PDF";
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/pdf",
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, body);

  ExpectPrefetchSuccess(histogram_tester, body.size());

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();
}

// Regression test for crbug.com/1491889. Completes a prefetch, and then changes
// the cookies for the prefetched URL. It then creates two NavigationRequests
// (to the same URL) and calls GetPrefetchToServe for each request. This can
// happen in practice when a user clicks on a link to a URL twice).
TEST_P(PrefetchServiceTest,
       MultipleNavigationRequestsCallGetPrefetchAfterCookieChange) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Adding a cookie after the prefetch has started will cause it to fail when
  // being served.
  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));
  task_environment()->RunUntilIdle();

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> future_1;
  GetPrefetchToServe(future_1, GURL("https://example.com"),
                     MainDocumentToken());
  EXPECT_TRUE(future_1.IsReady());
  // No prefetch should be returned (the example.com prefetch had its cookies
  // changed).
  EXPECT_FALSE(future_1.Get().GetPrefetchContainer());

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> future_2;
  GetPrefetchToServe(future_2, GURL("https://example.com"),
                     MainDocumentToken());
  EXPECT_TRUE(future_2.IsReady());
  EXPECT_FALSE(future_2.Get().GetPrefetchContainer());
}

TEST_P(PrefetchServiceTest, NotServeableNavigationInDifferentRenderFrameHost) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Since the navigation is occurring in a LocalFrameToken other than where the
  // prefetch was requested from, we cannot use it.
  blink::LocalFrameToken other_token(base::UnguessableToken::Create());
  ASSERT_NE(other_token, main_rfh()->GetFrameToken());
  blink::DocumentToken different_document_token;
  ASSERT_NE(different_document_token, MainDocumentToken());
  Navigate(GURL("https://example.com"), main_rfh()->GetProcess()->GetID(),
           other_token, different_document_token);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com"),
                                  different_document_token));
  std::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  EXPECT_FALSE(serving_page_metrics);
}

class PrefetchServiceWithHTMLOnlyTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"html_only", "true"}}}},
        {});
  }
};

INSTANTIATE_TEST_SUITE_P(, PrefetchServiceWithHTMLOnlyTest, testing::Bool());

TEST_P(PrefetchServiceWithHTMLOnlyTest, FailedNonHTMLWithHTMLOnly) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  std::string body = "fake PDF";
  MakeResponseAndWait(net::HTTP_OK, net::OK, "application/pdf",
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, body);

  ExpectPrefetchFailedAfterResponseReceived(
      histogram_tester, net::HTTP_OK, body.size(),
      PrefetchStatus::kPrefetchFailedMIMENotSupported);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedMIMENotSupported,
                       /*prefetch_header_latency=*/true);
}

class PrefetchServiceAlwaysMakeDecoyRequestTest
    : public PrefetchServiceTestBase {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "1"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {});
  }
};

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest, DecoyRequest) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // A decoy is considered a failure.
  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchIsPrivacyDecoy);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIsPrivacyDecoy,
                       /*prefetch_header_latency=*/true);
}

TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest,
       NavigateBeforeDecoyResponseReceived) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

  ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kUnspecified});
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
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kUserHasCookies);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIneligibleUserHasCookies);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_F(PrefetchServiceAlwaysMakeDecoyRequestTest,
       DISABLED_CHROMEOS(RedirectDecoyRequest)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://redirect.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://redirect.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));

  // The redirect is ineligible, but will be followed since the prefetch is now
  // a decoy.
  VerifyFollowRedirectParams(1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchIsPrivacyDecoy);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchIsPrivacyDecoy,
                       /*prefetch_header_latency=*/true);
}

class PrefetchServiceIncognitoTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
    PrefetchServiceTestBase::InitScopedFeatureList();
  }

 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<TestBrowserContext>();
    browser_context->set_is_off_the_record(true);
    return browser_context;
  }
};

INSTANTIATE_TEST_SUITE_P(, PrefetchServiceIncognitoTest, testing::Bool());

TEST_P(PrefetchServiceIncognitoTest, OffTheRecordEligible) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com/"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com/"));
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false, {}, kHTMLBody);
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));
}

TEST_P(PrefetchServiceTest, NonDefaultStoragePartition) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  test_content_browser_client_->UseOffTheRecordContextForStoragePartition(true);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  EXPECT_EQ(RequestCount(), 0);

  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kNonDefaultStoragePartition);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(
      PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition,
      /*prefetch_header_latency=*/false,
      /*required_private_prefetch_proxy=*/false);
}

class PrefetchServiceStreamingURLLoaderTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"use_streaming_url_loader", "true"}}}},
        {});
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         PrefetchServiceStreamingURLLoaderTest,
                         testing::Bool());

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceStreamingURLLoaderTest,
       DISABLED_CHROMEOS(StreamingURLLoaderSuccessCase)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  // Send the head of the navigation. The prefetch should be servable after this
  // point. The body of the response will be streaming to the serving URL loader
  // as its received.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));

  // Navigate to the URL before the prefetch response is complete.
  NavigateInitiatedByRenderer(GURL("https://example.com"));

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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com"));
  ASSERT_TRUE(serveable_reader);
  EXPECT_TRUE(serveable_reader.HasPrefetchStatus());
  EXPECT_EQ(serveable_reader.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotFinishedInTime);
  EXPECT_EQ(serveable_reader.GetServableState(base::TimeDelta::Max()),
            PrefetchContainer::ServableState::kServable);
  EXPECT_TRUE(serveable_reader.GetPrefetchContainer()->GetNonRedirectHead());
  EXPECT_TRUE(serveable_reader.GetPrefetchContainer()
                  ->GetNonRedirectHead()
                  ->was_in_prefetch_cache);

  ExpectServingMetrics(PrefetchStatus::kPrefetchNotFinishedInTime);

  // Send the body and completion status of the request, then recheck all of the
  // metrics.
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        blink::mojom::SpeculationEagerness::kEager,
                        /*is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess();
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest, DISABLED_CHROMEOS(NoVarySearchSuccessCase)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com/?a=1"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      /*referrer=*/blink::mojom::Referrer());
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com/?a=1"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(
      net::HTTP_OK, net::OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", R"(params=("a"))"}},
      kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com"));
  ExpectServingReaderSuccess(serveable_reader);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL("https://example.com/?a=1"));
  ExpectServingMetricsSuccess();
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest, DISABLED_CHROMEOS(PrefetchEligibleRedirect)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));
  VerifyFollowRedirectParams(1);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kIsolatedToIsolated, 1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest, DISABLED_CHROMEOS(IneligibleRedirectCookies)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  ASSERT_TRUE(SetCookie(GURL("https://redirect.com"), "testing"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));

  // Since the redirect URL has cookies, it is ineligible for prefetching and
  // causes the prefetch to fail. Also since checking if the URL has cookies
  // requires mojo, the eligibility check will not complete immediately.
  VerifyFollowRedirectParams(0);

  // Falls back to normal navigation.
  EXPECT_FALSE(future.Take());

  histogram_tester.ExpectUniqueSample("PrefetchProxy.Redirect.Result",
                                      PrefetchRedirectResult::kFailedIneligible,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kIsolatedToIsolated, 1);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchFailedIneligibleRedirect,
      /*is_accurate=*/true);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedIneligibleRedirect);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(IneligibleRedirectServiceWorker)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  service_worker_context_->AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey::CreateFromStringForTesting("https://redirect.com"));
  service_worker_context_->AddServiceWorkerScope(
      GURL("https://redirect.com"),
      ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));

  // Since the redirect URL has cookies, it is ineligible for prefetching and
  // causes the prefetch to fail. Also the eligibility check should fail
  // immediately.
  VerifyFollowRedirectParams(0);

  histogram_tester.ExpectUniqueSample("PrefetchProxy.Redirect.Result",
                                      PrefetchRedirectResult::kFailedIneligible,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kIsolatedToIsolated, 1);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchFailedIneligibleRedirect);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedIneligibleRedirect);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest, DISABLED_CHROMEOS(InvalidRedirect)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  // The redirect is considered invalid because it has a non-3XX HTTP code.
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(redirect_info, CreateURLResponseHeadForPrefetch(
                                               net::HTTP_OK, kHTMLMimeType,
                                               /*use_prefetch_proxy=*/true, {},
                                               GURL("https://redirect.com")));
  VerifyFollowRedirectParams(0);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kFailedInvalidResponseCode, 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Redirect.NetworkContextStateTransition", 0);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchFailedInvalidRedirect);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedInvalidRedirect);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(PrefetchSameOriginEligibleRedirect)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));

  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://example.com/redirect");
  MakeSingleRedirectAndWait(redirect_info,
                            CreateURLResponseHeadForPrefetch(
                                net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
                                /*use_prefetch_proxy=*/true, {},
                                GURL("https://example.com/redirect")));
  VerifyFollowRedirectParams(1);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kDefaultToDefault, 1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
// TODO(crbug.com/40265797): This test is testing the current
// functionality, and should be removed while fixing this bug.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(IneligibleSameSiteCrossOriginRequiresProxyRedirect)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));

  task_environment()->RunUntilIdle();

  // The request to the same-origin prefetch URL should ignore the proxy
  // requirement, since it only applies to cross-origin prefetches.
  VerifyCommonRequestState(GURL("https://example.com"));
  VerifyFollowRedirectParams(0);

  // Redirect to a same-site cross-origin URL. The proxy requirement should
  // apply to this URL, and result in the redirect being marked as ineligible,
  // because we cannot make same-site cross-origin requests that require the
  // proxy.
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://other.example.com/redirect");
  MakeSingleRedirectAndWait(redirect_info,
                            CreateURLResponseHeadForPrefetch(
                                net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
                                /*use_prefetch_proxy=*/true, {},
                                GURL("https://example.com/redirect")));
  VerifyFollowRedirectParams(0);

  histogram_tester.ExpectUniqueSample("PrefetchProxy.Redirect.Result",
                                      PrefetchRedirectResult::kFailedIneligible,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kDefaultToDefault, 1);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchFailedIneligibleRedirect);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedIneligibleRedirect);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(RedirectDefaultToIsolatedNetworkContextTransition)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));
  task_environment()->RunUntilIdle();

  // Since the redirect is cross-site compared to the referrer. A new request
  // will be started in an isolated network context, and the redirect will not
  // be followed directly.
  VerifyFollowRedirectParams(0);
  ClearCompletedRequests();
  VerifyCommonRequestState(GURL("https://redirect.com"));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kDefaultToIsolated, 1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(
           RedirectDefaultToIsolatedNetworkContextTransitionWithProxy)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  // The same-origin request should not use the proxy.
  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = false});
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));
  task_environment()->RunUntilIdle();

  // Since the redirect is cross-site compared to the referrer. A new request
  // will be started in an isolated network context, and the redirect will not
  // be followed directly. The new request should use the proxy.
  VerifyFollowRedirectParams(0);
  ClearCompletedRequests();
  VerifyCommonRequestState(GURL("https://redirect.com"),
                           {.use_prefetch_proxy = true});

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kDefaultToIsolated, 1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetricsSuccess();

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(RedirectIsolatedToDefaultNetworkContextTransition)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://other.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://other.com"),
                           {.use_prefetch_proxy = false});
  VerifyFollowRedirectParams(0);

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://example.com/redirect");
  MakeSingleRedirectAndWait(redirect_info,
                            CreateURLResponseHeadForPrefetch(
                                net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
                                /*use_prefetch_proxy=*/true, {},
                                GURL("https://example.com/redirect")));
  task_environment()->RunUntilIdle();

  // Since the redirect is same-site compared to the referrer. A new request
  // will be started in the default network context, and the redirect will not
  // be followed directly.
  VerifyFollowRedirectParams(0);
  ClearCompletedRequests();
  VerifyCommonRequestState(GURL("https://example.com/redirect"),
                           {.use_prefetch_proxy = false});

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kIsolatedToDefault, 1);

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  NavigateInitiatedByRenderer(GURL("https://other.com"));
  ExpectServingReaderSuccess(GetPrefetchToServe(GURL("https://other.com")));
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

class PrefetchServiceAllowRedirectsAndAlwaysBlockUntilHeadTest
    : public PrefetchServiceTestBase {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"block_until_head_eager_prefetch", "true"},
           {"block_until_head_moderate_prefetch", "true"},
           {"block_until_head_conservative_prefetch", "true"}}}},
        {features::kPrefetchNewWaitLoop});
  }
};

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_F(PrefetchServiceAllowRedirectsAndAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(RedirectNetworkContextTransitionBlockUntilHead)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/referrer"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"));
  VerifyFollowRedirectParams(0);

  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));
  task_environment()->RunUntilIdle();

  // Since the redirect is cross-site compared to the referrer. A new request
  // will be started in an isolated network context, and the redirect will not
  // be followed directly.
  VerifyFollowRedirectParams(0);
  ClearCompletedRequests();
  VerifyCommonRequestState(GURL("https://redirect.com"));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kSuccessRedirectFollowed, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.NetworkContextStateTransition",
      PrefetchRedirectNetworkContextTransition::kDefaultToIsolated, 1);

  // Once the final response to the prefetch is received, then callback given to
  // |GetPrefetchToServe| should be run.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_TRUE(serveable_reader);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        blink::mojom::SpeculationEagerness::kEager,
                        /*is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 2, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(RedirectInsufficientReferrerPolicy)) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://referrer.com"));
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  blink::mojom::Referrer referrer;
  referrer.url = GURL("https://referrer.com");
  referrer.policy = network::mojom::ReferrerPolicy::kDefault;
  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      /*referrer=*/referrer);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  // Redirect to a different site. This will check the referrer policy, but
  // since it is not sufficiently strict, the redirect should fail.
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy = net::ReferrerPolicy::NEVER_CLEAR;
  redirect_info.new_url = GURL("https://redirect.com");
  MakeSingleRedirectAndWait(
      redirect_info,
      CreateURLResponseHeadForPrefetch(
          net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
          /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com")));
  VerifyFollowRedirectParams(0);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Redirect.Result",
      PrefetchRedirectResult::kFailedInsufficientReferrerPolicy, 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Redirect.NetworkContextStateTransition", 0);

  ExpectPrefetchFailedBeforeResponseReceived(
      histogram_tester, PrefetchStatus::kPrefetchFailedInvalidRedirect);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedInvalidRedirect);

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
}

class PrefetchServiceNeverBlockUntilHeadTest : public PrefetchServiceTestBase {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"block_until_head_eager_prefetch", "false"},
           {"block_until_head_moderate_prefetch", "false"},
           {"block_until_head_conservative_prefetch", "false"}}}},
        {});
  }
};

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_F(PrefetchServiceNeverBlockUntilHeadTest,
       DISABLED_CHROMEOS(HeadNotReceived)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  // Navigate to the URL before the head of the prefetch response is received.
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Since PrefetchService cannot block until headers for this prefetch, it
  // should immediately return null.
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));

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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  ExpectCorrectUkmLogs(
      {.outcome = PreloadingTriggeringOutcome::kRunning, .is_accurate = true});

  ExpectServingMetrics(PrefetchStatus::kPrefetchNotFinishedInTime);
}

class PrefetchServiceAlwaysBlockUntilHeadTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<blink::mojom::SpeculationEagerness> {
 public:
  const int kPrefetchTimeout = 10000;
  const int kBlockUntilHeadTimeout = 1000;
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {
              {"ineligible_decoy_request_probability", "0"},
              {"prefetch_container_lifetime_s", "-1"},
              {"prefetch_timeout_ms", "10000"},
              {"block_until_head_eager_prefetch", "true"},
              {"block_until_head_moderate_prefetch", "true"},
              {"block_until_head_conservative_prefetch", "true"},
              // Initialize timeouts > 0ms for testing purposes.
              {"block_until_head_timeout_eager_prefetch", "1000"},
              {"block_until_head_timeout_moderate_prefetch", "1000"},
              {"block_until_head_timeout_conservative_prefetch", "1000"},
          }}},
        {features::kPrefetchNewWaitLoop});
  }
};

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(BlockUntilHeadReceived)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(500));

  // Sends the head of the prefetch response. This should trigger the above
  // callback.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_TRUE(serveable_reader);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam(),
                        /*is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.Served.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(500), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(NVSBlockUntilHeadReceived)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  network::mojom::NoVarySearchPtr no_vary_search_hint =
      network::mojom::NoVarySearch::New();
  no_vary_search_hint->vary_on_key_order = true;
  no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams(
          std::vector<std::string>({"a"}));
  MakePrefetchOnMainFrame(GURL("https://example.com/index.html?a=5"),
                          PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                       /*use_prefetch_proxy=*/true, GetParam()),
                          /* referrer */ blink::mojom::Referrer(),
                          std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com/index.html"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com/index.html"),
                     MainDocumentToken());
  EXPECT_FALSE(future.IsReady());
  task_environment()->FastForwardBy(base::Milliseconds(600));

  // Sends the head of the prefetch response. This should trigger the above
  // callback.
  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"a\")"}},
      std::size(kHTMLBody));
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_TRUE(serveable_reader);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam(),
                        /* is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.Served.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(600), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(NVSBlockUntilHeadReceivedNoMatchNoNVSHeader)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  network::mojom::NoVarySearchPtr no_vary_search_hint =
      network::mojom::NoVarySearch::New();
  no_vary_search_hint->vary_on_key_order = true;
  no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams(
          std::vector<std::string>({"a"}));
  MakePrefetchOnMainFrame(
      GURL("https://example.com/index.html?a=5"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()),
      /* referrer */ blink::mojom::Referrer(),
      /* no_vary_search_hint */ std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com/index.html"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com/index.html"),
                     MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(700));

  // Sends the head of the prefetch response. This should trigger the above
  // callback with a nullptr argument.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_FALSE(serveable_reader);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam());
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(700), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(NVSBlockUntilHeadReceivedNoMatchByNVSHeader)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  network::mojom::NoVarySearchPtr no_vary_search_hint =
      network::mojom::NoVarySearch::New();
  no_vary_search_hint->vary_on_key_order = true;
  no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams(
          std::vector<std::string>({"a"}));
  MakePrefetchOnMainFrame(
      GURL("https://example.com/index.html?a=5"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()),
      /* referrer */ blink::mojom::Referrer(),
      /* no_vary_search_hint */ std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com/index.html"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com/index.html"),
                     MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(kHeaderLatency));

  // Sends the head of the prefetch response. This should trigger the above
  // callback with a nullptr argument.
  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"b\")"}},
      std::size(kHTMLBody));
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_FALSE(serveable_reader);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam());
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(kHeaderLatency), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedCookiesChangedWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(800));

  // Adding a cookie after while blocking until the head is received will cause
  // it to fail.
  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));
  task_environment()->RunUntilIdle();

  // Sends the head of the prefetch response. This should trigger the above
  // callback.
  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  PrefetchContainer::Reader serveable_reader = future.Take();
  EXPECT_FALSE(serveable_reader);

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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  ExpectServingMetrics(PrefetchStatus::kPrefetchNotUsedCookiesChanged);

  ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kFailure,
                        .failure = ToPreloadingFailureReason(
                            PrefetchStatus::kPrefetchNotUsedCookiesChanged),
                        .is_accurate = true,
                        .eagerness = GetParam()});

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(800), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedTimeoutWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  PrefetchType prefetch_type(PreloadingTriggerType::kSpeculationRule,
                             /*use_prefetch_proxy=*/true, GetParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  // If the prefetch times out while PrefetchService is blocking until head,
  // then it should unblock without setting serveable_reader.
  task_environment()->FastForwardBy(base::Milliseconds(kPrefetchTimeout));
  PrefetchContainer::Reader serveable_reader = future.Take();
  EXPECT_FALSE(serveable_reader);

  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_TIMED_OUT, GetParam(),
                               /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  base::TimeDelta block_until_head_timeout =
      PrefetchBlockUntilHeadTimeout(prefetch_type);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      block_until_head_timeout, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedTimeoutWhileBlockUntilHeadForOlderNavigation)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // The first navigation is started and then gone before the head of the
  // prefetch response is received. The given callback shouldn't be called (and
  // the metrics shouldn't be recorded) because the navigation is gone before
  // the request is unblocked.
  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> first_future;
  GetPrefetchToServe(first_future, GURL("https://example.com"),
                     MainDocumentToken());
  EXPECT_FALSE(first_future.IsReady());

  // The second navigation is started before the head of the prefetch response
  // is received.
  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> second_future;
  GetPrefetchToServe(second_future, GURL("https://example.com"),
                     MainDocumentToken());
  EXPECT_FALSE(second_future.IsReady());

  // The prefetch times out while PrefetchService is blocking until head.
  // This should unblock the request after `kBlockUntilHeadTimeout` msec without
  // setting serveable_reader.
  task_environment()->FastForwardBy(base::Milliseconds(kPrefetchTimeout));

  PrefetchContainer::Reader serveable_reader = second_future.Take();
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(serveable_reader);
  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_TIMED_OUT, GetParam(),
                               /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError,
                       /*prefetch_header_latency=*/false,
                       /*required_private_prefetch_proxy=*/false);

  // This metric is recorded for the second navigation, as the PrefetchContainer
  // was initially considered as a candidate at the time of navigation start but
  // decided not to be used later (after `kBlockUntilHeadTimeout` msec) due to
  // timeout.
  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(kBlockUntilHeadTimeout), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);

  // The third navigation is started after the PrefetchContainer became not
  // servable.
  NavigateInitiatedByRenderer(GURL("https://example.com"));
  base::test::TestFuture<PrefetchContainer::Reader> third_future;
  GetPrefetchToServe(third_future, GURL("https://example.com"),
                     MainDocumentToken());
  serveable_reader = third_future.Take();
  EXPECT_FALSE(serveable_reader);

  // The metric should not be recorded for the third navigation, because the
  // PrefetchContainer was not servable when the third navigation starts and
  // thus shouldn't be considered as a candidate in the first place.
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(kBlockUntilHeadTimeout), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedNetErrorWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(300));

  // If the prefetch encounters a net error while PrefetchService is blocking
  // until head, then it should unblock without setting
  // serveable_reader.
  CompleteResponseAndWait(net::ERR_ACCESS_DENIED, 0);
  PrefetchContainer::Reader serveable_reader = future.Take();
  EXPECT_FALSE(serveable_reader);

  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_ACCESS_DENIED,
                               GetParam(), /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError,
                       /*prefetch_header_latency=*/false,
                       /*required_private_prefetch_proxy=*/false);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(300), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40064525): For NVSBlockUntilHeadReceivedOneMatchOneTimeout,
// FailedCookiesChangedAfterPrefetchStartedTimedoutNVSHintPrefetch,
// FailedCookiesChangedAfterPrefetchStartedNVSHintPrefetch and
// NVSBlockUntilHeadReceivedMultipleMatchesByNVSHint, consider only keeping one
// of them and removing the remaining, as they almost test the same logic.
// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(
    PrefetchServiceAlwaysBlockUntilHeadTest,
    DISABLED_CHROMEOS_AND_CASTOS(NVSBlockUntilHeadReceivedOneMatchOneTimeout)) {
  // The scenario is:
  // * Prefetch https://example.com/index.html?a=5 with NVS hint/NVS header to
  //   ignore "a" and send head/body.
  // * Queue a prefetch for https://example.com/index.html?b=3 with NVS hint to
  //   match but send no head/body.
  // * Navigate to https://example.com/index.html.
  // * Expect https://example.com/index.html?a=5 to be served.
  const std::string kTestUrl = "https://example.com/index.html";
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(2));

  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    MakePrefetchOnMainFrame(
        GURL(kTestUrl + "?a=5"),
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();

    VerifyCommonRequestState(
        GURL(kTestUrl + "?a=5"),
        {.expected_priority = ExpectedPriorityForEagerness(GetParam())});
  }
  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"b"}));
    MakePrefetchOnMainFrame(
        GURL(kTestUrl + "?b=3"),
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();

    VerifyPrefetchAttemptIsPending(GURL(kTestUrl + "?b=3"));
  }
  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL(kTestUrl));

  // Request the prefetch from the PrefetchService. `future` should be blocked
  // until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL(kTestUrl), MainDocumentToken());

  task_environment()->FastForwardBy(base::Milliseconds(kHeaderLatency));
  EXPECT_FALSE(future.IsReady());

  // Sends the head of the prefetch response. This should unblock `future`.
  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"a\")"}},
      std::size(kHTMLBody));

  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_TRUE(serveable_reader);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL(kTestUrl + "?a=5"));

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  // Check the metrics now that the prefetch is complete.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 2);
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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  ExpectServingMetricsSuccess(/*required_private_prefetch_proxy=*/false);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  if (UseNewWaitLoop()) {
    histogram_tester.ExpectUniqueTimeSample(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.BlockUntilHeadDuration2.Served.%s",
            histogram_suffix.c_str()),
        base::Milliseconds(kHeaderLatency), 1);
    histogram_tester.ExpectUniqueTimeSample(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.BlockUntilHeadDuration2.NotServed.%s",
            histogram_suffix.c_str()),
        base::Milliseconds(kHeaderLatency), 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf("PrefetchProxy.AfterClick."
                           "PrefetchMatchingBlockedNavigationWithPrefetch.%s",
                           histogram_suffix.c_str()),
        true, 2);
  } else {
    histogram_tester.ExpectUniqueTimeSample(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
            histogram_suffix.c_str()),
        base::Milliseconds(kHeaderLatency), 0);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
            histogram_suffix.c_str()),
        true, 1);
  }
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS_AND_CASTOS(
           FailedCookiesChangedAfterPrefetchStartedTimedoutNVSHintPrefetch)) {
  // The scenario is:
  // * Prefetch https://example.com/index.html.
  // * Queue a prefetch for https://example.com/index.html?a=1 with NVS hint to
  //   match but send no head/body.
  // * Change the cookies.
  // * Navigate to https://example.com/index.html.
  // * Expect no prefetch to be served.
  const std::string kTestUrl = "https://example.com/index.html";
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(2));

  MakePrefetchOnMainFrame(
      GURL(kTestUrl), PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                   /*use_prefetch_proxy=*/false, GetParam()));
  VerifyCommonRequestState(
      GURL(kTestUrl),
      {.expected_priority = ExpectedPriorityForEagerness(GetParam())});
  task_environment()->RunUntilIdle();
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    MakePrefetchOnMainFrame(
        GURL(kTestUrl + "?a=1"),
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();
    VerifyCommonRequestStateByUrl(
        GURL(kTestUrl + "?a=1"),
        {.expected_priority = ExpectedPriorityForEagerness(GetParam())});
  }

  // Adding a cookie after the prefetch has started will cause it to fail when
  // being served.
  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));
  task_environment()->RunUntilIdle();

  NavigateInitiatedByRenderer(GURL(kTestUrl));

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

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  // Request the prefetch from the PrefetchService. Since both prefetch
  // candidates are not eligible serveable_reader will be falsy.
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com/index.html")));

  // Behavioral change.
  //
  // kenoss@ believes that `PrefetchServingPageMetrics` is broken (at least non
  // reliable state).
  //
  // In this test, `PrefetchContainer::UpdateServingPageMetrics()` is called
  // four times: (For simplicity of description, we assumet that
  // `kPrefetchNewWaitLoop`.)
  //
  // - Prefetch for https://example.com/index.html, path is
  //      PrefetchMatchResolver2::FindPrefetchInternal()
  //   -> PrefetchService::CollectMatchCandidates()
  //   -> PrefetchContainer::UpdateServingPageMetrics()
  //   value is 0.456 s.
  // - Prefetch for https://example.com/index.html?a=1, path is
  //      PrefetchMatchResolver2::FindPrefetchInternal()
  //   -> PrefetchService::CollectMatchCandidates()
  //   -> PrefetchContainer::UpdateServingPageMetrics()
  //   value is std::nullopt.
  // - Prefetch for https://example.com/index.html, path is
  //      PrefetchMatchResolver2::FindPrefetchInternal()
  //   -> PrefetchMatchResolver2::UnblockForCookiesChanged()
  //   -> PrefetchContainer::OnDetectedCookiesChange2()
  //   -> PrefetchContainer::OnDetectedCookiesChange()
  //   -> PrefetchContainer::UpdateServingPageMetrics()
  //   value is 0.456 s.
  // - Prefetch for https://example.com/index.html?a=1, path is
  //      PrefetchMatchResolver2::FindPrefetchInternal()
  //   -> PrefetchMatchResolver2::UnblockForCookiesChanged()
  //   -> PrefetchContainer::OnDetectedCookiesChange2()
  //   -> PrefetchContainer::OnDetectedCookiesChange()
  //   -> PrefetchContainer::UpdateServingPageMetrics()
  //   value is std::nullopt.
  //
  // If `kPrefetchNewWaitLoop` is enabled, the calling order of the third and
  // the fourth is unstable because
  // `PrefetchMatchResolver2::UnblockForCookiesChanged()` uses an iteration of
  // `std::map` and those prefetch refer the same
  // `PrefetchServingPageMetricsContanier`. The essential problem is 1. they
  // share the same `PrefetchServingPageMetricsContanier`. 2. There is no
  // coordination.
  //
  // I don't think this metrics is worth to preserve until the root cause is
  // fixed.
  //
  // TODO(crbug.com/360094997): Reconstruct `PrefetchServingPageMetrics`.
  if (!UseNewWaitLoop()) {
    ExpectServingMetrics(PrefetchStatus::kPrefetchNotUsedCookiesChanged,
                         /*prefetch_header_latency=*/true,
                         /*required_private_prefetch_proxy=*/false);
  }
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS_AND_CASTOS(
           FailedCookiesChangedAfterPrefetchStartedNVSHintPrefetch)) {
  // The scenario is:
  // * Start prefetching https://example.com/index.html but send no head/body.
  // * Queue a prefetch for https://example.com/index.html?a=1 with NVS hint to
  //   match but send no head/body.
  // * Change the cookies.
  // * Navigate to https://example.com/index.html.
  // * Send head/body for https://example.com/index.html.
  // * Verify that the navigation is not waiting anymore on
  //   https://example.com/index.html?a=1 head.
  // * Expect no prefetch to be served.
  const std::string kTestUrl = "https://example.com/index.html";
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(2));
  MakePrefetchOnMainFrame(
      GURL(kTestUrl), PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                   /*use_prefetch_proxy=*/false, GetParam()));
  VerifyCommonRequestState(
      GURL(kTestUrl),
      {.expected_priority = ExpectedPriorityForEagerness(GetParam())});
  task_environment()->RunUntilIdle();
  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    GURL url_2{kTestUrl + "?a=1"};
    MakePrefetchOnMainFrame(
        url_2,
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();
    VerifyPrefetchAttemptIsPending(url_2);
  }

  // Adding a cookie after the prefetch has started will cause it to fail when
  // being served.
  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));
  task_environment()->RunUntilIdle();

  NavigateInitiatedByRenderer(GURL(kTestUrl));

  // Request the prefetch from the PrefetchService.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL(kTestUrl), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  // Sends the head of the prefetch response. This should not trigger the above
  // callback.
  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"e\")"}},
      std::size(kHTMLBody));

  EXPECT_TRUE(future.IsReady());
  PrefetchContainer::Reader serveable_reader = future.Take();
  // Both prefetch candidates are not eligible.
  EXPECT_FALSE(serveable_reader);
  EXPECT_FALSE(
      GetPrefetchMatchResolverForMostRecentNavigation()->IsWaitingForPrefetch(
          GURL(kTestUrl + "?a=1")));

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode", net::HTTP_OK, 1);
  // We cancel the streaming of this prefetch because we know we cannot use it.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", std::size(kHTMLBody), 0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 2);
  // None of the prefetches were successful because of the cookie change.
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  // Serving page metrics prefetch_header_latency is logged at response
  // complete. Since we cancel streaming the response, this should not be set.
  ExpectServingMetrics(PrefetchStatus::kPrefetchNotUsedCookiesChanged,
                       /*prefetch_header_latency=*/false,
                       /*required_private_prefetch_proxy=*/false);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS_AND_CASTOS(
           NVSBlockUntilHeadReceivedMultipleMatchesByNVSHint)) {
  // The scenario is:
  // * Prefetch https://example.com/index.html?a=5 with NVS hint to ignore "a"
  //   but mismatched NVS header and send head/body.
  // * Queue a prefetch for https://example.com/index.html?b=3 with NVS hint/NVS
  //   header to ignore "b" and send head/body.
  // * Navigate to https://example.com/index.html.
  // * Make sure to receive ?a=5 prefetch before ?b=3.
  // * Expect https://example.com/index.html?b=3 not to be served, because it
  //   cannot improve the performance (even worse, it would block the real
  //   navigation).
  const std::string kTestUrl = "https://example.com/index.html";
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(2));

  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    GURL not_matched_url = GURL(kTestUrl + "?a=5");
    MakePrefetchOnMainFrame(
        not_matched_url,
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();

    VerifyCommonRequestState(
        not_matched_url,
        {.expected_priority = ExpectedPriorityForEagerness(GetParam())});
  }
  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"b"}));
    GURL matched_url = GURL(kTestUrl + "?b=3");
    MakePrefetchOnMainFrame(
        matched_url,
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/false, GetParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();
    VerifyPrefetchAttemptIsPending(matched_url);
  }
  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL(kTestUrl));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL(kTestUrl), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());
  task_environment()->RunUntilIdle();
  // Sends the head of the prefetch response. This should not trigger the above
  // callback.
  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"e\")"}},
      std::size(kHTMLBody));

  // The second should not be used for a real navigation. The rationale is that:
  // if a prefetch request has not started before a real navigation starts, then
  // it cannot help improve the performance, and in the worst case it would
  // block the real navigation.
  EXPECT_TRUE(future.IsReady());
  PrefetchContainer::Reader serveable_reader = future.Take();
  ASSERT_FALSE(serveable_reader);

  // Send the body and completion status of the request,
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  SendHeadOfResponseForUrlAndWait(
      GURL(kTestUrl + "?b=3"), net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"b\")"}},
      std::size(kHTMLBody));
  // Check the metrics now that the prefetch is complete.
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 2);

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 2);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 2);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(0), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAlwaysBlockUntilHeadTest,
    testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                    blink::mojom::SpeculationEagerness::kConservative));

class PrefetchServiceAlwaysBlockUntilHeadWithTimeoutTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<blink::mojom::SpeculationEagerness> {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {
              {"ineligible_decoy_request_probability", "0"},
              {"prefetch_container_lifetime_s", "-1"},
              {"prefetch_timeout_ms", "10000"},
              {"block_until_head_eager_prefetch", "true"},
              {"block_until_head_moderate_prefetch", "true"},
              {"block_until_head_conservative_prefetch", "true"},
              {"block_until_head_timeout_eager_prefetch", "1000"},
              {"block_until_head_timeout_moderate_prefetch", "1000"},
              {"block_until_head_timeout_conservative_prefetch", "1000"},
          }}},
        {features::kPrefetchNewWaitLoop});
  }
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAlwaysBlockUntilHeadWithTimeoutTest,
    testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                    blink::mojom::SpeculationEagerness::kConservative));

TEST_P(PrefetchServiceAlwaysBlockUntilHeadWithTimeoutTest,
       DISABLED_CHROMEOS(BlockUntilHeadTimedout)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback should be
  // triggered once the timeout is exceeded.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  PrefetchContainer::Reader serveable_reader = future.Take();
  EXPECT_FALSE(serveable_reader);

  // If the prefetch is received after the block until head has timed out, it
  // will not be used.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();
  EXPECT_FALSE(serveable_reader);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(1000), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

TEST_P(PrefetchServiceAlwaysBlockUntilHeadWithTimeoutTest,
       DISABLED_CHROMEOS(HeadReceivedBeforeTimeout)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));

  // Request the prefetch from the PrefetchService. The given callback should be
  // triggered once the timeout is exceeded.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com"), MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  PrefetchContainer::Reader serveable_reader = future.Take();
  EXPECT_FALSE(serveable_reader);

  // If the prefetch is received after the block until head has timed out, it
  // will not be used.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();
  EXPECT_FALSE(serveable_reader);

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(1000), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on lacros trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadWithTimeoutTest,
       DISABLED_CHROMEOS(MultipleGetPrefetchToServe)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetParam()));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com"));
  // Request the prefetch from the PrefetchService. The same prefetch will be
  // requested again, so this callback will not be called.
  base::test::TestFuture<PrefetchContainer::Reader> first_future;
  GetPrefetchToServe(first_future, GURL("https://example.com"),
                     MainDocumentToken());

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  // Request the prefetch from the PrefetchService a second time. This
  // callback should be triggered once the timeout is exceeded.
  base::test::TestFuture<PrefetchContainer::Reader> second_future;
  GetPrefetchToServe(second_future, GURL("https://example.com"),
                     MainDocumentToken());
  EXPECT_FALSE(second_future.IsReady());
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  PrefetchContainer::Reader serveable_reader = second_future.Take();
  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(serveable_reader);

  // If the prefetch is received after the block until head has timed out, it
  // will not be used.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody), GetParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetPrefetchEagernessHistogramSuffix(GetParam());
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.NotServed.%s",
          histogram_suffix.c_str()),
      base::Milliseconds(1000), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          histogram_suffix.c_str()),
      true, 1);

  EXPECT_FALSE(first_future.IsReady());
}

class PrefetchServiceNewLimitsTest : public PrefetchServiceTest {
 public:
  void InitScopedFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"}}},
         {features::kPrefetchNewLimits,
          {{"max_eager_prefetches", "2"}, {"max_non_eager_prefetches", "2"}}}},
        {});
  }

  PrefetchContainer::Reader CompletePrefetch(
      GURL url,
      blink::mojom::SpeculationEagerness eagerness) {
    MakePrefetchOnMainFrame(
        url, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                          /*use_prefetch_proxy=*/false, eagerness));
    task_environment()->RunUntilIdle();
    return CompleteExistingPrefetch(
        url, {.expected_priority = ExpectedPriorityForEagerness(eagerness)});
  }

  // Unlike the above method, this expects the prefetch for |url| to have
  // already been triggered.
  PrefetchContainer::Reader CompleteExistingPrefetch(
      GURL url,
      const VerifyCommonRequestStateOptions& common_options = {}) {
    VerifyCommonRequestState(url, common_options);
    MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                        /*use_prefetch_proxy=*/false,
                        {{"X-Testing", "Hello World"}}, kHTMLBody);
    NavigateInitiatedByRenderer(url);
    return GetPrefetchToServe(url);
  }
};

TEST_F(PrefetchServiceNewLimitsTest,
       NonEagerPrefetchAllowedWhenEagerLimitIsReached) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");
  const GURL url_3 = GURL("https://example.com/three");
  const GURL url_4 = GURL("https://example.com/four");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/4));

  ASSERT_TRUE(
      CompletePrefetch(url_1, blink::mojom::SpeculationEagerness::kEager));
  ASSERT_TRUE(
      CompletePrefetch(url_2, blink::mojom::SpeculationEagerness::kEager));

  // Note: |url_3| is not prefetched as the limit for eager prefetches has been
  // reached.
  MakePrefetchOnMainFrame(
      url_3, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                          /*use_prefetch_proxy=*/false,
                          blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(RequestCount(), 0);
  NavigateInitiatedByRenderer(url_3);
  ASSERT_FALSE(GetPrefetchToServe(url_3));

  // We can still prefetch |url_4| as it is a conservative prefetch.
  auto non_eager_prefetch = CompletePrefetch(
      url_4, blink::mojom::SpeculationEagerness::kConservative);
  ASSERT_TRUE(non_eager_prefetch);
  EXPECT_EQ(non_eager_prefetch.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 4);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 4);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 3);
}

TEST_F(PrefetchServiceNewLimitsTest, NonEagerPrefetchEvictedAtLimit) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");
  const GURL url_3 = GURL("https://example.com/three");
  const GURL url_4 = GURL("https://example.com/four");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/4));

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_1)).Times(1);
  EXPECT_CALL(mock_destruction_callback, Run(url_2)).Times(1);
  PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh())
      ->SetPrefetchDestructionCallback(mock_destruction_callback.Get());

  auto prefetch_1 =
      CompletePrefetch(url_1, blink::mojom::SpeculationEagerness::kModerate);
  ASSERT_TRUE(prefetch_1);
  EXPECT_EQ(prefetch_1.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);

  auto prefetch_2 =
      CompletePrefetch(url_2, blink::mojom::SpeculationEagerness::kModerate);
  ASSERT_TRUE(prefetch_2);
  EXPECT_EQ(prefetch_2.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  ASSERT_TRUE(prefetch_1);

  auto prefetch_3 =
      CompletePrefetch(url_3, blink::mojom::SpeculationEagerness::kModerate);
  ASSERT_TRUE(prefetch_3);
  EXPECT_EQ(prefetch_3.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  // Prefetch for |url_1| should have been evicted to allow a prefetch of
  // |url_3|.
  ASSERT_FALSE(prefetch_1);
  ASSERT_TRUE(prefetch_2);

  auto prefetch_4 =
      CompletePrefetch(url_4, blink::mojom::SpeculationEagerness::kModerate);
  ASSERT_TRUE(prefetch_4);
  EXPECT_EQ(prefetch_4.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  // Prefetch for |url_2| should have been evicted to allow a prefetch of
  // |url_4|.
  ASSERT_FALSE(prefetch_2);
  ASSERT_TRUE(prefetch_3);

  // The first and second prefetches should have failure reason set to
  // 'kPrefetchEvicted'.
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 4u);

    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(
                 content::PrefetchStatus::kPrefetchEvictedForNewerPrefetch),
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kModerate),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(
                 content::PrefetchStatus::kPrefetchEvictedForNewerPrefetch),
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kModerate),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kModerate),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kModerate)};
    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

TEST_F(PrefetchServiceNewLimitsTest, PrefetchWithNoCandidateIsNotStarted) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");
  const GURL url_3 = GURL("https://example.com/three");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/3));

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;
  auto candidate_3 = candidate_1.Clone();
  candidate_3->url = url_3;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  ASSERT_TRUE(prefetch_document_manager);

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_2)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  // Send 3 candidates to PrefetchDocumentManager.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();
  VerifyCommonRequestState(url_1);

  // Remove |url_2| from the list of candidates while a prefetch for |url_1| is
  // in progress.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);

  // Finish prefetch of |url_1|.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  // PrefetchService skips |url_2| because its candidate was removed, and starts
  // prefetching |url_3| instead.
  VerifyCommonRequestState(url_3);
  // Finish prefetch of |url_2|.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  // There should be no pending prefetch requests.
  EXPECT_EQ(RequestCount(), 0);
}

TEST_F(PrefetchServiceNewLimitsTest,
       InProgressPrefetchWithNoCandidateIsCancelled) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/2));

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  ASSERT_TRUE(prefetch_document_manager);

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_1)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  // Send 2 candidates to PrefetchDocumentManager.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Prefetch for |url_1| should have started.
  VerifyCommonRequestState(url_1);

  // Remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // The prefetch for |url_1| should be cancelled, and prefetch for |url_2|
  // should have started.

  EXPECT_EQ(test_url_loader_factory_.pending_requests()->size(), 2u);
  // The client for the first request should be disconnected.
  EXPECT_FALSE(
      test_url_loader_factory_.GetPendingRequest(0)->client.is_connected());
  // Clears out first request.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  VerifyCommonRequestState(url_2);
  NavigateInitiatedByRenderer(url_1);
  PrefetchContainer::Reader serveable_reader = GetPrefetchToServe(url_1);
  EXPECT_FALSE(serveable_reader);
}

TEST_F(PrefetchServiceNewLimitsTest,
       CompletedPrefetchWithNoCandidateIsEvicted) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");
  const GURL url_3 = GURL("https://example.com/three");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/2));

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  ASSERT_TRUE(prefetch_document_manager);

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_1)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  // Send 2 candidates to PrefetchDocumentManager.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Complete prefetches for |url_1| and |url_2|.
  auto prefetch_1 = CompleteExistingPrefetch(url_1);
  ASSERT_TRUE(prefetch_1);
  auto prefetch_2 = CompleteExistingPrefetch(url_2);
  ASSERT_TRUE(prefetch_2);

  // Remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();
  // |prefetch_1| should have been removed.
  EXPECT_FALSE(prefetch_1);
  EXPECT_TRUE(prefetch_2);
}

// Test to see if we can re-prefetch a url whose previous prefetch expired.
TEST_F(PrefetchServiceNewLimitsTest, PrefetchReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "1"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/2));

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());

  const auto url = GURL("https://example.com/one");
  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->url = url;
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate->referrer = blink::mojom::Referrer::New();

  // Start and complete prefetch of |url|.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  auto prefetch = CompleteExistingPrefetch(url);
  ASSERT_TRUE(prefetch);
  EXPECT_EQ(prefetch.GetPrefetchStatus(), PrefetchStatus::kPrefetchSuccessful);

  // Fast forward by a second and expire |prefetch|.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(prefetch);

  // Try reprefetching |url|.
  // TODO(crbug.com/40064525): Ideally this prefetch would be requeued
  // automatically.
  candidates.clear();
  candidates.push_back(candidate.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  // Prefetch for |url| should have started again.
  VerifyCommonRequestState(url);

  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 2u);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(PrefetchStatus::kPrefetchIsStale),
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kEager),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kRunning,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/false,
             /*ready_time=*/std::nullopt,
             blink::mojom::SpeculationEagerness::kEager)};
    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

TEST_F(PrefetchServiceNewLimitsTest, NextPrefetchQueuedImmediatelyAfterReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "1"}}},
       {features::kPrefetchNewLimits, {{"max_eager_prefetches", "1"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/2));

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_1)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1->Clone();
  candidate_2->url = url_2;

  // Add |candidate_1| and |candidate_2|.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Complete |prefetch| of |url_1|.
  auto prefetch_1 = CompleteExistingPrefetch(url_1);
  ASSERT_TRUE(prefetch_1);
  EXPECT_EQ(prefetch_1.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);

  // Prefetch of |url_2| should not be queued because we are at the limit.
  EXPECT_EQ(RequestCount(), 0);

  // Fast forward by a second and expire |prefetch_1|.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(prefetch_1);

  // Prefetch of |url_2| should now be queued.
  VerifyCommonRequestState(url_2);
}

TEST_F(PrefetchServiceNewLimitsTest, PrefetchFailsAndIsReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchUseContentRefactor,
        {{"ineligible_decoy_request_probability", "0"},
         {"prefetch_container_lifetime_s", "1"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/1));

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());

  const auto url = GURL("https://example.com/one");
  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->url = url;
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate->referrer = blink::mojom::Referrer::New();

  // Start prefetch of |url|.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  base::RunLoop().RunUntilIdle();
  VerifyCommonRequestState(url);
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Fast forward by a second and expire existing PrefetchContainer.
  task_environment()->FastForwardBy(base::Seconds(1));
  // The failure reason recorded (failed due to net error) should not be
  // overwritten when the prefetch is timed out (marked as stale).
  ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kFailure,
                        .failure = ToPreloadingFailureReason(
                            PrefetchStatus::kPrefetchFailedNetError)});
}

TEST_F(PrefetchServiceNewLimitsTest, EagerPrefetchLimitIsDynamic) {
  const GURL url_1 = GURL("https://example.com/one");
  const GURL url_2 = GURL("https://example.com/two");
  const GURL url_3 = GURL("https://example.com/three");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/4));

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  ASSERT_TRUE(prefetch_document_manager);

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url_1)).Times(1);
  EXPECT_CALL(mock_destruction_callback, Run(url_2)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;
  auto candidate_3 = candidate_1.Clone();
  candidate_3->url = url_3;

  // Send |candidate_1| and |candidate_2|.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  auto prefetch_1 = CompleteExistingPrefetch(url_1);
  ASSERT_TRUE(prefetch_1);
  EXPECT_EQ(prefetch_1.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  auto prefetch_2 = CompleteExistingPrefetch(url_2);
  ASSERT_TRUE(prefetch_2);
  EXPECT_EQ(prefetch_2.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);

  // Remove |candidate_1| and add |candidate_3|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Prefetch for |url_3| should succeed, and |prefetch_1| should be evicted.
  auto prefetch_3 = CompleteExistingPrefetch(url_3);
  ASSERT_TRUE(prefetch_3);
  EXPECT_EQ(prefetch_3.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_FALSE(prefetch_1);
  EXPECT_TRUE(prefetch_2);

  // Re-add |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // |url_1| should not be reprefetched because we are at the limit.
  EXPECT_EQ(RequestCount(), 0);

  // Remove |candidate_2|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Prefetch for |url_1| should succeed, |prefetch_2| will be evicted
  // (because |candidate_2| was removed).
  prefetch_1 = CompleteExistingPrefetch(url_1);
  ASSERT_TRUE(prefetch_1);
  EXPECT_EQ(prefetch_1.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchSuccessful);
  EXPECT_FALSE(prefetch_2);
  EXPECT_TRUE(prefetch_3);

  // The first and second prefetches should have failure reason set to
  // 'kPrefetchEvicted', and the fourth prefetch should have failure reason
  // set to |kPrefetchFailedPerPageLimitExceeded|.
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 4u);

    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        // |url_1|, attempt #1 (evicted)
        {attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(
                 content::PrefetchStatus::
                     kPrefetchEvictedAfterCandidateRemoved),
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kEager),
         // |url_2| (evicted)
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(
                 content::PrefetchStatus::
                     kPrefetchEvictedAfterCandidateRemoved),
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kEager),
         // |url_3| (ready)
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kEager),
         // |url_1|, attempt #2 (ready)
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kReady,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/true,
             /*ready_time=*/
             base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
             blink::mojom::SpeculationEagerness::kEager)};
    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

TEST_F(PrefetchServiceNewLimitsTest, RemoveCandidateForFailedPrefetch) {
  const GURL url = GURL("https://example.com/one");

  NavigateAndCommit(GURL("https://example.com"));

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/1));

  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->url = url;
  candidate->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate->referrer = blink::mojom::Referrer::New();

  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  ASSERT_TRUE(prefetch_document_manager);

  base::MockRepeatingCallback<void(const GURL& url)> mock_destruction_callback;
  EXPECT_CALL(mock_destruction_callback, Run(url)).Times(1);
  prefetch_document_manager->SetPrefetchDestructionCallback(
      mock_destruction_callback.Get());

  // Send canadidate to PrefetchDocumentManager.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate.Clone());
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  // Prefetch for |url| should have started.
  VerifyCommonRequestState(url);
  // Send error response for prefetch of |url|
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Remove |candidate|.
  candidates.clear();
  prefetch_document_manager->ProcessCandidates(candidates,
                                               /*devtools_observer=*/nullptr);
  task_environment()->RunUntilIdle();

  ExpectCorrectUkmLogs({.outcome = PreloadingTriggeringOutcome::kFailure,
                        .failure = ToPreloadingFailureReason(
                            PrefetchStatus::kPrefetchFailedNetError)});
}

blink::UserAgentMetadata GetFakeUserAgentMetadata() {
  blink::UserAgentMetadata metadata;
  metadata.brand_version_list.emplace_back("fake", "42");
  metadata.brand_full_version_list.emplace_back("fake", "42.0.1701.99");
  metadata.full_version = "42.0.1701.99";
  return metadata;
}

class PrefetchServiceClientHintsTest
    : public PrefetchServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void InitScopedFeatureList() override {
    InitScopedFeatureListForNewWaitLoop(GetParam());
  }

 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = PrefetchServiceTestBase::CreateBrowserContext();
    TestBrowserContext::FromBrowserContext(browser_context.get())
        ->SetClientHintsControllerDelegate(&client_hints_controller_delegate_);
    return browser_context;
  }

  MockClientHintsControllerDelegate& client_hints_controller_delegate() {
    return client_hints_controller_delegate_;
  }

 private:
  MockClientHintsControllerDelegate client_hints_controller_delegate_{
      GetFakeUserAgentMetadata()};
};

INSTANTIATE_TEST_SUITE_P(, PrefetchServiceClientHintsTest, testing::Bool());

TEST_P(PrefetchServiceClientHintsTest, NoClientHintsWhenDisabled) {
  base::test::ScopedFeatureList disable_prefetch_ch;
  disable_prefetch_ch.InitAndDisableFeature(features::kPrefetchClientHints);

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://example.com/"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com/two"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://example.com/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_FALSE(pending->request.headers.HasHeader("Sec-CH-UA"));
}

TEST_P(PrefetchServiceClientHintsTest, LowEntropyClientHints) {
  base::test::ScopedFeatureList enable_prefetch_ch;
  enable_prefetch_ch.InitAndEnableFeature(features::kPrefetchClientHints);

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://example.com/"));

  MakePrefetchOnMainFrame(
      GURL("https://example.com/two"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://example.com/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_TRUE(pending->request.headers.HasHeader("Sec-CH-UA"));
}

TEST_P(PrefetchServiceClientHintsTest, HighEntropyClientHints) {
  base::test::ScopedFeatureList enable_prefetch_ch;
  enable_prefetch_ch.InitAndEnableFeature(features::kPrefetchClientHints);

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://example.com/"));
  client_hints_controller_delegate().SetMostRecentMainFrameViewportSize(
      gfx::Size(800, 600));
  client_hints_controller_delegate().PersistClientHints(
      url::Origin::Create(GURL("https://example.com/")),
      /*parent_rfh=*/nullptr,
      {network::mojom::WebClientHintsType::kViewportWidth});

  MakePrefetchOnMainFrame(
      GURL("https://example.com/two"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://example.com/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_TRUE(pending->request.headers.HasHeader("Sec-CH-UA"));
  std::optional<std::string> viewport_width =
      pending->request.headers.GetHeader("Sec-CH-Viewport-Width");
  EXPECT_TRUE(viewport_width.has_value());

  // Even though we hinted it above, if the actual RenderWidgetHostView reports
  // its size that gets used above. So accept any positive integer. (This
  // notably affects the results on Android.)
  int viewport_width_int;
  EXPECT_TRUE(base::StringToInt(*viewport_width, &viewport_width_int));
  EXPECT_GT(viewport_width_int, 0);
}

TEST_P(PrefetchServiceClientHintsTest, CrossSiteNone) {
  base::test::ScopedFeatureList enable_prefetch_ch;
  enable_prefetch_ch.InitAndEnableFeatureWithParameters(
      features::kPrefetchClientHints, {{"cross_site_behavior", "none"}});

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://a.test/"));
  client_hints_controller_delegate().SetMostRecentMainFrameViewportSize(
      gfx::Size(800, 600));
  client_hints_controller_delegate().PersistClientHints(
      url::Origin::Create(GURL("https://b.test/")),
      /*parent_rfh=*/nullptr,
      {network::mojom::WebClientHintsType::kViewportWidth});

  MakePrefetchOnMainFrame(
      GURL("https://b.test/"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://a.test/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_FALSE(pending->request.headers.HasHeader("Sec-CH-UA"));
  EXPECT_FALSE(pending->request.headers.HasHeader("Sec-CH-Viewport-Width"));
}

TEST_P(PrefetchServiceClientHintsTest, CrossSiteLowEntropy) {
  base::test::ScopedFeatureList enable_prefetch_ch;
  enable_prefetch_ch.InitAndEnableFeatureWithParameters(
      features::kPrefetchClientHints, {{"cross_site_behavior", "low_entropy"}});

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://a.test/"));
  client_hints_controller_delegate().SetMostRecentMainFrameViewportSize(
      gfx::Size(800, 600));
  client_hints_controller_delegate().PersistClientHints(
      url::Origin::Create(GURL("https://b.test/")),
      /*parent_rfh=*/nullptr,
      {network::mojom::WebClientHintsType::kViewportWidth});

  MakePrefetchOnMainFrame(
      GURL("https://b.test/"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://a.test/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_TRUE(pending->request.headers.HasHeader("Sec-CH-UA"));
  EXPECT_FALSE(pending->request.headers.HasHeader("Sec-CH-Viewport-Width"));
}

TEST_P(PrefetchServiceClientHintsTest, CrossSiteAll) {
  base::test::ScopedFeatureList enable_prefetch_ch;
  enable_prefetch_ch.InitAndEnableFeatureWithParameters(
      features::kPrefetchClientHints, {{"cross_site_behavior", "all"}});

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://a.test/"));
  client_hints_controller_delegate().SetMostRecentMainFrameViewportSize(
      gfx::Size(800, 600));
  client_hints_controller_delegate().PersistClientHints(
      url::Origin::Create(GURL("https://b.test/")),
      /*parent_rfh=*/nullptr,
      {network::mojom::WebClientHintsType::kViewportWidth});

  MakePrefetchOnMainFrame(
      GURL("https://b.test/"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://a.test/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  auto* pending = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending);
  EXPECT_TRUE(pending->request.headers.HasHeader("Sec-CH-UA"));
  EXPECT_TRUE(pending->request.headers.HasHeader("Sec-CH-Viewport-Width"));
}

TEST_P(PrefetchServiceTest, CancelWhileBlockedOnHead) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  NavigateAndCommit(GURL("https://example.com/"));

  GURL next_url("https://example.com/two");
  MakePrefetchOnMainFrame(
      next_url,
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(GURL("https://example.com/"),
                             network::mojom::ReferrerPolicy::kStrictOrigin));
  task_environment()->RunUntilIdle();

  // Start a navigation to the URL (one must be running for GetPrefetchToServe
  // to work, at present).
  NavigateInitiatedByRenderer(next_url);

  // Try to access the outcome of the prefetch, like the serving path does.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, next_url, MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  // Cancel the prefetch.
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  prefetch_document_manager->ProcessCandidates(candidates, nullptr);
  task_environment()->RunUntilIdle();

  // If all goes well, the service has reported that the prefetch cannot be
  // served. This could be changed in the future to instead wait for the
  // prefetch rather than cancelling it, but what's key here is that it doesn't
  // hang.
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

// With new wait loop, multiple concurrent navigations are handled correctly.
// TODO(crbug.com/353490734): Remove the above sentence.
//
// Scenario:
//
// - Prefetch A started.
// - A received non-redirect header.
// - Navigation X started, which matches to A. Unblocked synchronously as
//   success.
// - Navigation Y started, which matches to A. Unblocked synchronously as
//   success.
TEST_P(
    PrefetchServiceTest,
    DISABLED_CHROMEOS(MultipleConcurrentNavigationSuccessBeforeNavigations)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrefetchNewWaitLoop, features::kPrefetchReusable}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));

  std::unique_ptr<NavigationResult> nav_res1 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  task_environment()->RunUntilIdle();

  ExpectServingReaderSuccess(FROM_HERE, nav_res1->reader_future.Take());
  ExpectServingMetrics(
      FROM_HERE, nav_res1,
      {.prefetch_status = PrefetchStatus::kPrefetchSuccessful,
       .prefetch_header_latency = base::Milliseconds(kHeaderLatency),
       .required_private_prefetch_proxy = true});

  ExpectServingReaderSuccess(FROM_HERE, nav_res2->reader_future.Take());
  ExpectServingMetrics(
      FROM_HERE, nav_res2,
      {.prefetch_status = PrefetchStatus::kPrefetchSuccessful,
       .prefetch_header_latency = base::Milliseconds(kHeaderLatency),
       .required_private_prefetch_proxy = true});

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.PrefetchMatchingBlockedNavigationWithPrefetch."
      "Eager",
      false, 2);
}

// Scenario:
//
// - Prefetch A started.
// - Navigation X started, which matches to A. Blocked by A.
// - Navigation Y started, which matches to A. Blocked by A.
// - A received non-redirect header. Unblocks them as success.
TEST_P(
    PrefetchServiceTest,
    DISABLED_CHROMEOS(MultipleConcurrentNavigationBlockUntilHeadThenSuccess)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrefetchNewWaitLoop, features::kPrefetchReusable}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  std::unique_ptr<NavigationResult> nav_res1 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  task_environment()->RunUntilIdle();

  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        blink::mojom::SpeculationEagerness::kEager,
                        /*is_accurate=*/true);

  ExpectServingReaderSuccess(FROM_HERE, nav_res1->reader_future.Take());
  // TODO(crbug.com/356540465): See the bug. Make PrefetchServingMetrics
  // available for multiple concurrent navigations.
  ExpectServingMetrics(
      FROM_HERE, nav_res1,
      {.prefetch_status = PrefetchStatus::kPrefetchNotFinishedInTime,
       .prefetch_header_latency = std::nullopt,
       .required_private_prefetch_proxy = true});

  ExpectServingReaderSuccess(FROM_HERE, nav_res2->reader_future.Take());
  ExpectServingMetrics(
      FROM_HERE, nav_res2,
      {.prefetch_status = PrefetchStatus::kPrefetchSuccessful,
       .prefetch_header_latency = base::Milliseconds(kHeaderLatency),
       .required_private_prefetch_proxy = true});

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 2);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.PrefetchMatchingBlockedNavigationWithPrefetch."
      "Eager",
      true, 2);
}

// Scenario:
//
// - Prefetch A started with NVS hint.
// - Navigation X started, which is potentially matches and eventually matches
//   to A. Blocked by A.
// - Navigation Y started, which is potentially matches and not eventually
//   matches to A. Blocked by A.
// - A received non-redirect header. Unblocks them as success/fail.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(
           MultipleConcurrentNavigationBlockUntilHeadThenSuccessFail)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrefetchNewWaitLoop, features::kPrefetchReusable}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  network::mojom::NoVarySearchPtr no_vary_search_hint =
      network::mojom::NoVarySearch::New();
  no_vary_search_hint->vary_on_key_order = true;
  no_vary_search_hint->search_variance =
      network::mojom::SearchParamsVariance::NewNoVaryParams(
          std::vector<std::string>({"match", "notEventuallyMatch"}));

  MakePrefetchOnMainFrame(
      GURL("https://example.com/?match=0"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager),
      /* referrer */ blink::mojom::Referrer(), std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com/?match=0"),
                           {.use_prefetch_proxy = true});

  std::unique_ptr<NavigationResult> nav_res1 = SimulatePartOfNavigation(
      GURL("https://example.com/?match=1"), /*is_renderer_initiated=*/true);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com/?notEventuallyMatch=1"),
      /*is_renderer_initiated=*/true);
  task_environment()->RunUntilIdle();

  SendHeadOfResponseAndWait(
      net::HTTP_OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/true,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", "params=(\"match\")"}},
      std::size(kHTMLBody));
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        blink::mojom::SpeculationEagerness::kEager,
                        /*is_accurate=*/true);

  ExpectServingReaderSuccess(FROM_HERE, nav_res1->reader_future.Take());
  // TODO(crbug.com/356540465): See the bug. Make PrefetchServingMetrics
  // available for multiple concurrent navigations.
  ExpectServingMetrics(
      FROM_HERE, nav_res1,
      {.prefetch_status = PrefetchStatus::kPrefetchNotFinishedInTime,
       .prefetch_header_latency = std::nullopt,
       .required_private_prefetch_proxy = true});

  EXPECT_FALSE(nav_res2->reader_future.Take());

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.PrefetchMatchingBlockedNavigationWithPrefetch."
      "Eager",
      true, 2);
}

// Scenario:
//
// - Prefetch A started.
// - Navigation X started, which matches to A. Blocked by A.
// - Navigation Y started, which matches to A. Blocked by A.
// - Cookies of domain of A changed.
// - A received non-redirect header. Unblocks them as fail.
//
// This test checks that it is safe to call
// `PrefetchContainer::OnDetectedCookiesChange2()` multiple times.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(
           MultipleConcurrentNavigationBlockUntilHeadThenCookiesChanged)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrefetchNewWaitLoop, features::kPrefetchReusable}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});

  std::unique_ptr<NavigationResult> nav_res1 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true);
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  EXPECT_FALSE(nav_res1->reader_future.Take());

  EXPECT_FALSE(nav_res2->reader_future.Take());

  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.AfterClick.RedirectChainSize", 0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.PrefetchMatchingBlockedNavigationWithPrefetch."
      "Eager",
      true, 2);
}

}  // namespace
}  // namespace content
