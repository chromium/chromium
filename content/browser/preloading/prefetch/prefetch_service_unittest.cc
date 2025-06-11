// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/mock_prefetch_service_delegate.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_scheduler.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/preload_pipeline_info.h"
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
#include "content/test/test_content_browser_client.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
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
#include "third_party/blink/public/common/navigation/preloading_headers.h"
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

// Represents the duration between prefetch is added and its URLRequest is
// started (`URLResponseHead.LoadTimingInfo.request_start`).
constexpr int kAddedToURLRequestStartLatency = 123;
// Represents the duration between the URLRequest is started
// (`URLResponseHead.LoadTimingInfo.request_start`) and the header is received
// (`URLResponseHead.LoadTimingInfo.receive_headers_end`).
constexpr int kHeaderLatency = 456;

// TODO(taiyo): Convert const to constexper.
const char kHTMLMimeType[] = "text/html";

const char kHTMLBody[] = R"(
      <!DOCTYPE HTML>
      <html>
        <head></head>
        <body></body>
      </html>)";

const char kHTMLBodyServerError[] = R"(
    <!DOCTYPE HTML>
<html lang="en">
<head>
  <title>500 Internal Server Error</title>
</head>
<body>
</body>
</html>
)";

// Param for parametrized tests for rearchitecturing/refactoring of
// `PrefetchService`.
//
// Do not remove and keep it even if there is no param to make it easy to add
// another param in the future.
struct PrefetchServiceRearchParam {
 public:
  using Arg = int;

  static std::vector<PrefetchServiceRearchParam::Arg> Params();
  static PrefetchServiceRearchParam CreateFromIndex(int index);

  bool prefetch_scheduler;
  bool prefetch_scheduler_progress_sync_best_effort;
};

// static
std::vector<int> PrefetchServiceRearchParam::Params() {
  return {0, 1, 2};
}

// static
PrefetchServiceRearchParam PrefetchServiceRearchParam::CreateFromIndex(
    int index) {
  std::vector<PrefetchServiceRearchParam> params = {
      PrefetchServiceRearchParam{
          .prefetch_scheduler = false,
          .prefetch_scheduler_progress_sync_best_effort = false,
      },
      PrefetchServiceRearchParam{
          .prefetch_scheduler = true,
          .prefetch_scheduler_progress_sync_best_effort = false,
      },
      PrefetchServiceRearchParam{
          .prefetch_scheduler = true,
          .prefetch_scheduler_progress_sync_best_effort = true,
      },
  };
  return params[index];
}

class WithPrefetchServiceRearchParam {
 public:
  explicit WithPrefetchServiceRearchParam(int index)
      : param_(PrefetchServiceRearchParam::CreateFromIndex(index)) {}
  virtual ~WithPrefetchServiceRearchParam() = default;

  void InitRearchFeatures();

  const PrefetchServiceRearchParam& rearch_param() { return param_; }

 private:
  PrefetchServiceRearchParam param_;
  base::test::ScopedFeatureList feature_list_prefetch_scheduler_;
};

void WithPrefetchServiceRearchParam::InitRearchFeatures() {
  if (param_.prefetch_scheduler) {
    feature_list_prefetch_scheduler_.InitWithFeaturesAndParameters(
        {{
            features::kPrefetchScheduler,
            {
                {"kPrefetchSchedulerProgressSyncBestEffort",
                 param_.prefetch_scheduler_progress_sync_best_effort ? "true"
                                                                     : "false"},
            },
        }},
        {});
  }
}

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
    auto service_worker_info = std::ranges::find_if(
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

// A `content::PrefetchRequestStatusListener` that tracks when the callbacks
// have been called.
class ProbePrefetchRequestStatusListener
    : public content::PrefetchRequestStatusListener {
 public:
  ~ProbePrefetchRequestStatusListener() override = default;

  void OnPrefetchStartFailedGeneric() override {
    prefetch_start_failed_called_ = true;
  }

  void OnPrefetchStartFailedDuplicate() override {
    prefetch_start_failed_duplicate_called_ = true;
  }

  void OnPrefetchResponseCompleted() override {
    prefetch_response_completed_called_ = true;
  }

  void OnPrefetchResponseError() override {
    prefetch_response_error_called_ = true;
  }

  void OnPrefetchResponseServerError(int response_code) override {
    prefetch_response_server_error_called_ = true;
    server_response_code_ = response_code;
  }

  bool GetPrefetchStartFailedCalled() { return prefetch_start_failed_called_; }

  bool GetPrefetchResponseCompletedCalled() {
    return prefetch_response_completed_called_;
  }

  bool GetPrefetchResponseErrorCalled() {
    return prefetch_response_error_called_;
  }

  bool GetPrefetchResponseServerErrorCalled() {
    return prefetch_response_server_error_called_;
  }

  base::WeakPtr<ProbePrefetchRequestStatusListener> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool prefetch_start_failed_called_ = false;
  bool prefetch_start_failed_duplicate_called_ = false;
  bool prefetch_response_completed_called_ = false;
  bool prefetch_response_error_called_ = false;
  bool prefetch_response_server_error_called_ = false;

  std::optional<int> server_response_code_;

  base::WeakPtrFactory<ProbePrefetchRequestStatusListener> weak_factory_{this};
};

// A |content::PrefetchRequestStatusListener| that forwards the callback
// resolution to a `ProbePrefetchRequestStatusListener` for testability.
//
// The `ProbePrefetchRequestStatusListener` is needed because the
// `TestablePrefetchRequestStatusListener` is eventually owned by the browser
// context, whereas the `ProbePrefetchRequestStatusListener` is owned by the
// test.
class TestablePrefetchRequestStatusListener
    : public content::PrefetchRequestStatusListener {
 public:
  explicit TestablePrefetchRequestStatusListener(
      base::WeakPtr<ProbePrefetchRequestStatusListener> probe_listener)
      : probe_listener_(probe_listener) {}

  ~TestablePrefetchRequestStatusListener() override = default;

  void OnPrefetchStartFailedGeneric() override {
    probe_listener_->OnPrefetchStartFailedGeneric();
  }

  void OnPrefetchStartFailedDuplicate() override {
    probe_listener_->OnPrefetchStartFailedDuplicate();
  }

  void OnPrefetchResponseCompleted() override {
    probe_listener_->OnPrefetchResponseCompleted();
  }

  void OnPrefetchResponseError() override {
    probe_listener_->OnPrefetchResponseError();
  }

  void OnPrefetchResponseServerError(int response_code) override {
    probe_listener_->OnPrefetchResponseServerError(response_code);
  }

 private:
  base::WeakPtr<ProbePrefetchRequestStatusListener> probe_listener_ = nullptr;
};

class PrefetchServiceTestBase : public PrefetchingMetricsTestBase {
 public:
  const int kServiceWorkerCheckDuration = 1000;
  PrefetchServiceTestBase()
      : test_url_loader_factory_(/*observe_loader_requests=*/true),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    service_worker_context_ =
        std::make_unique<PrefetchFakeServiceWorkerContext>(*task_environment());
  }

  void SetUp() override {
    PrefetchingMetricsTestBase::SetUp();

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
  }

  void TearDown() override {
    if (PrefetchDocumentManager::GetForCurrentDocument(main_rfh()))
      PrefetchDocumentManager::DeleteForCurrentDocument(main_rfh());
    PrefetchDocumentManager::SetPrefetchServiceForTesting(nullptr);
    mock_navigation_handle_.reset();
    PrefetchService::SetURLLoaderFactoryForTesting(nullptr);
    PrefetchService::SetHostNonUniqueFilterForTesting(nullptr);
    PrefetchService::SetServiceWorkerContextForTesting(nullptr);
    PrefetchService::SetURLLoaderFactoryForTesting(nullptr);
    test_content_browser_client_.reset();
    request_handler_keep_alive_.clear();
    service_worker_context_.reset();
    PrefetchingMetricsTestBase::TearDown();
  }

  virtual void InitScopedFeatureList() = 0;

  void InitBaseParams() {
    scoped_feature_list_base_params_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {});
  }

  void MakePrefetchService(std::unique_ptr<MockPrefetchServiceDelegate>
                               mock_prefetch_service_delegate) {
    test_content_browser_client_ =
        std::make_unique<ScopedPrefetchServiceContentBrowserClient>(
            std::move(mock_prefetch_service_delegate));

    std::unique_ptr<PrefetchService> prefetch_service =
        std::make_unique<PrefetchService>(browser_context());
    BrowserContextImpl::From(browser_context())
        ->SetPrefetchServiceForTesting(std::move(prefetch_service));
    PrefetchDocumentManager::SetPrefetchServiceForTesting(
        BrowserContextImpl::From(browser_context())->GetPrefetchService());
  }

  // Creates a prefetch request for |url| on the current main frame.
  void MakePrefetchOnMainFrame(
      const GURL& prefetch_url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer = blink::mojom::Referrer(),
      network::mojom::NoVarySearchPtr&& no_vary_search_hint =
          network::mojom::NoVarySearchPtr(),
      PreloadingType planned_max_preloading_type = PreloadingType::kPrefetch) {
    CHECK(prefetch_type.IsRendererInitiated());
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
    prefetch_document_manager->PrefetchUrl(
        prefetch_url, prefetch_type,
        GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type()),
        referrer, SpeculationRulesTags(), no_vary_search_hint,
        PreloadPipelineInfo::Create(planned_max_preloading_type));
  }

  [[nodiscard]] std::unique_ptr<PrefetchHandle> MakePrefetchFromEmbedder(
      const GURL& prefetch_url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer = blink::mojom::Referrer(),
      const std::optional<url::Origin> referring_origin = std::nullopt) {
    CHECK(!prefetch_type.IsRendererInitiated());

    auto prefetch_container = std::make_unique<PrefetchContainer>(
        *web_contents(), prefetch_url, prefetch_type,
        test::kPreloadingEmbedderHistgramSuffixForTesting, referrer,
        std::move(referring_origin),
        /*no_vary_search_hint=*/std::nullopt,
        PreloadPipelineInfo::Create(
            /*planned_max_preloading_type=*/PreloadingType::kPrefetch),
        /*attempt=*/nullptr);
    return BrowserContextImpl::From(browser_context())
        ->GetPrefetchService()
        ->AddPrefetchContainerWithHandle(std::move(prefetch_container));
  }

  std::unique_ptr<content::PrefetchHandle> MakePrefetchFromBrowserContext(
      const GURL& url,
      std::optional<net::HttpNoVarySearchData> no_vary_search_data,
      const net::HttpRequestHeaders& additional_headers,
      std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
      base::TimeDelta ttl_in_sec = base::Seconds(/* 10 minutes */ 60 * 10)) {
    return browser_context()->StartBrowserPrefetchRequest(
        url, test::kPreloadingEmbedderHistgramSuffixForTesting, true,
        no_vary_search_data, additional_headers,
        std::move(request_status_listener), ttl_in_sec,
        /*should_append_variations_header=*/true);
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
    std::optional<std::string> sec_purpose_header_value = std::nullopt;
    net::HttpRequestHeaders additional_headers;
  };

  void VerifyCommonRequestState(const GURL& url) {
    VerifyCommonRequestState(url, {});
  }

  void VerifyCommonRequestStateForEmbedders(
      const GURL& url,
      const VerifyCommonRequestStateOptions& options) {
    VerifyCommonRequestState(
        url, {.expected_priority =
                  base::FeatureList::IsEnabled(
                      features::kPrefetchNetworkPriorityForEmbedders)
                      ? net::RequestPriority::MEDIUM
                      : net::RequestPriority::IDLE});
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
        BrowserContextImpl::From(browser_context())
            ->GetPrefetchService()
            ->MatchUrl(prefetch_key);
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
    Navigate(url, main_rfh()->GetProcess()->GetDeprecatedID(),
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
            auto request_handler =
                prefetch_to_serve.CreateRequestHandler().first;
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
    PrefetchService* prefetch_service =
        BrowserContextImpl::From(browser_context())->GetPrefetchService();
      auto key = PrefetchContainer::Key(initiator_document_token, url);
      PrefetchMatchResolver::FindPrefetch(
          std::move(key), PrefetchServiceWorkerState::kDisallowed,
          /*is_nav_prerender=*/false, *prefetch_service,
          GetServingPageMetricsContainerForMostRecentNavigation(),
          std::move(callback));
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
  // - Starts matching of prefetch `PrefetchMatchResolver::FindPrefetch()`.
  //
  // Note that these are very small part of navigation. For example,
  // post-matching process `OnGotPrefetchToServe()` and redirects are not
  // handled.
  std::unique_ptr<NavigationResult> SimulatePartOfNavigation(
      const GURL& url,
      bool is_renderer_initiated,
      bool is_nav_prerender) {
    return is_renderer_initiated
               ? SimulatePartOfNavigation(
                     url, is_nav_prerender,
                     main_rfh()->GetProcess()->GetDeprecatedID(),
                     main_rfh()->GetFrameToken(), MainDocumentToken())
               : SimulatePartOfNavigation(url, is_nav_prerender,
                                          ChildProcessHost::kInvalidUniqueID,
                                          std::nullopt, std::nullopt);
  }

  std::unique_ptr<NavigationResult> SimulatePartOfNavigation(
      const GURL& url,
      bool is_nav_prerender,
      int initiator_process_id,
      const std::optional<blink::LocalFrameToken>& initiator_local_frame_token,
      const std::optional<blink::DocumentToken>& initiator_document_token) {
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
    PrefetchService* prefetch_service =
        BrowserContextImpl::From(browser_context())->GetPrefetchService();
    auto key = PrefetchContainer::Key(initiator_document_token, url);
    PrefetchMatchResolver::FindPrefetch(
        std::move(key), PrefetchServiceWorkerState::kDisallowed,
        is_nav_prerender, *prefetch_service,
        std::move(serving_page_metrics_container), std::move(callback));

    return res;
  }

  ScopedPrefetchServiceContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
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
    auto it = std::ranges::find(pending_requests, url,
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

    EXPECT_THAT(
        request->request.headers.GetHeader(blink::kPurposeHeaderName),
        testing::Optional(std::string(blink::kSecPurposePrefetchHeaderValue)));

    std::string sec_purpose_header_value;
    if (options.sec_purpose_header_value) {
      sec_purpose_header_value = options.sec_purpose_header_value.value();
    } else {
      sec_purpose_header_value =
          options.use_prefetch_proxy
              ? blink::kSecPurposePrefetchAnonymousClientIpHeaderValue
              : blink::kSecPurposePrefetchHeaderValue;
    }
    EXPECT_THAT(
        request->request.headers.GetHeader(blink::kSecPurposeHeaderName),
        testing::Optional(sec_purpose_header_value));

    EXPECT_THAT(request->request.headers.GetHeader("Accept"),
                testing::Optional(FrameAcceptHeaderValue(
                    /*allow_sxg_responses=*/true, browser_context())));

    EXPECT_THAT(request->request.headers.GetHeader("Upgrade-Insecure-Requests"),
                testing::Optional(std::string("1")));

    ASSERT_TRUE(request->request.trusted_params.has_value());
    VerifyIsolationInfo(request->request.trusted_params->isolation_info);

    EXPECT_EQ(request->request.priority, options.expected_priority);

    net::HttpRequestHeaders::Iterator header_it(options.additional_headers);
    while (header_it.GetNext()) {
      EXPECT_THAT(request->request.headers.GetHeader(header_it.name()),
                  testing::Optional(header_it.value()));
    }
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

  base::ScopedMockElapsedTimersForTest scoped_test_timer_;

  std::unique_ptr<PrefetchFakeServiceWorkerContext> service_worker_context_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  base::test::ScopedFeatureList scoped_feature_list_base_params_;
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;

  std::unique_ptr<testing::NiceMock<MockNavigationHandle>>
      mock_navigation_handle_;

  std::unique_ptr<ScopedPrefetchServiceContentBrowserClient>
      test_content_browser_client_;

  std::map<GURL, mojo::ScopedDataPipeProducerHandle> producer_handle_for_gurl_;

  std::vector<PrefetchRequestHandler> request_handler_keep_alive_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState};
};

class PrefetchServiceTest
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceTest() : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
  }
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

TEST_P(PrefetchServiceTest, SuccessCase) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  const PrefetchType prefetch_type = PrefetchType(
      PreloadingTriggerType::kSpeculationRule,
      /*use_prefetch_proxy=*/true, blink::mojom::SpeculationEagerness::kEager);
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
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

  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          GetMetricsSuffixTriggerTypeAndEagerness(
              prefetch_type, /*embedder_histogram_suffix=*/std::nullopt)),
      false, 1);
}

TEST_P(PrefetchServiceTest, SuccessCase_Browser) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  net::HttpRequestHeaders request_additional_headers = {};
  request_additional_headers.SetHeader("foo", "bar");
  request_additional_headers.SetHeader("foo1", "bar1");

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("https://example.com?b=1"),
                                     std::nullopt, request_additional_headers,
                                     std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(
      GURL("https://example.com?b=1"),
      {.use_prefetch_proxy = false,
       .additional_headers = request_additional_headers});

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

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

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_TRUE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  NavigateInitiatedByBrowser(GURL("https://example.com?b=1"));

  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com?b=1"), std::nullopt);
  ExpectServingReaderSuccess(serveable_reader);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL("https://example.com/?b=1"));

  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          GetMetricsSuffixTriggerTypeAndEagerness(
              PrefetchType(PreloadingTriggerType::kEmbedder,
                           /*use_prefetch_proxy=*/false),
              test::kPreloadingEmbedderHistgramSuffixForTesting)),
      false, 1);
}

TEST_P(PrefetchServiceTest, SuccessCase_Browser_NoVarySearch) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  net::HttpRequestHeaders request_additional_headers = {};
  request_additional_headers.SetHeader("foo", "bar");
  request_additional_headers.SetHeader("foo1", "bar1");

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  net::HttpNoVarySearchData nvs_data =
      net::HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, false);
  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("https://example.com?a=1"), nvs_data,
                                     request_additional_headers,
                                     std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(
      GURL("https://example.com?a=1"),
      {.use_prefetch_proxy = false,
       .additional_headers = request_additional_headers});

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  MakeResponseAndWait(
      net::HTTP_OK, net::OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", R"(params=("a"))"}},
      kHTMLBody);

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

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_TRUE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  NavigateInitiatedByBrowser(GURL("https://example.com"));

  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com"), std::nullopt);
  ExpectServingReaderSuccess(serveable_reader);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL("https://example.com/?a=1"));
}

TEST_P(PrefetchServiceTest, FailureCase_Browser_ServerErrorResponseCode) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("https://example.com?b=1"),
                                     std::nullopt, {},
                                     std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(GURL("https://example.com?b=1"),
                                       {.use_prefetch_proxy = false});

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  MakeResponseAndWait(net::HTTP_INTERNAL_SERVER_ERROR, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false, {}, kHTMLBodyServerError);

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.RespCode",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError", net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength",
      std::size(kHTMLBodyServerError), 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", kTotalTimeDuration, 1);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_TRUE(probe_listener->GetPrefetchResponseServerErrorCalled());

  NavigateInitiatedByBrowser(GURL("https://example.com?b=1"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com?b=1")));
}

TEST_P(PrefetchServiceTest, FailureCase_Browser_NetError) {
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("https://example.com?c=1"),
                                     std::nullopt, {},
                                     std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(GURL("https://example.com?c=1"),
                                       {.use_prefetch_proxy = false});
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_TRUE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_FAILED,
                               blink::mojom::SpeculationEagerness::kEager,
                               /*is_accurate_triggering=*/false,
                               /*browser_initiated_prefetch=*/true);

  NavigateInitiatedByBrowser(GURL("https://example.com?c=1"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com?c=1")));
}

TEST_P(PrefetchServiceTest, FailureCase_Browser_NotEligibleNonHttps) {
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  EXPECT_FALSE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("http://example.com"), std::nullopt,
                                     {}, std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(probe_listener->GetPrefetchStartFailedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseCompletedCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseErrorCalled());
  EXPECT_FALSE(probe_listener->GetPrefetchResponseServerErrorCalled());

  EXPECT_EQ(RequestCount(), 0);

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps, 1);
  ExpectPrefetchNotEligible(
      histogram_tester, PreloadingEligibility::kSchemeIsNotHttps,
      /*is_accurate=*/false, /*browser_initiated_prefetch=*/true);

  NavigateInitiatedByBrowser(GURL("http://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("http://example.com")));
}

TEST_P(PrefetchServiceTest, BrowserContextPrefetchRespectsTTL) {
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  net::HttpRequestHeaders request_additional_headers = {};
  request_additional_headers.SetHeader("foo", "bar");
  request_additional_headers.SetHeader("foo1", "bar1");

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(GURL("https://example.com?b=1"),
                                     std::nullopt, request_additional_headers,
                                     std::move(request_status_listener),
                                     base::Minutes(5));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(
      GURL("https://example.com?b=1"),
      {.use_prefetch_proxy = false,
       .additional_headers = request_additional_headers});

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);
  NavigateInitiatedByBrowser(GURL("https://example.com?b=1"));

  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com?b=1"), std::nullopt);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL("https://example.com?b=1"));

  task_environment()->FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(serveable_reader);
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
  const PrefetchType prefetch_type = PrefetchType(
      PreloadingTriggerType::kEmbedder, /*use_prefetch_proxy=*/false);
  auto handle =
      MakePrefetchFromEmbedder(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(GURL("https://example.com"),
                                       {.use_prefetch_proxy = false});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
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
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          GetMetricsSuffixTriggerTypeAndEagerness(
              prefetch_type,
              test::kPreloadingEmbedderHistgramSuffixForTesting)),
      false, 1);
}

TEST_P(PrefetchServiceTest,
       PrefetchDoesNotMatchIfDocumentTokenDoesNotMatch_Embedder) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);

  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  auto handle =
      MakePrefetchFromEmbedder(GURL("https://example.com"),
                               PrefetchType(PreloadingTriggerType::kEmbedder,
                                            /*use_prefetch_proxy=*/false));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(GURL("https://example.com"),
                                       {.use_prefetch_proxy = false});
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligiblePreloadingDisabled, 1);
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
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceAllowAllDomainsTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    // Override `kPrefetchUseContentRefactor`.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAllowAllDomainsTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

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
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceAllowAllDomainsForExtendedPreloadingTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    // Override `kPrefetchUseContentRefactor`.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"allow_all_domains_for_extended_preloading", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAllowAllDomainsForExtendedPreloadingTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

TEST_P(PrefetchServiceAllowAllDomainsForExtendedPreloadingTest,
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

TEST_P(PrefetchServiceAllowAllDomainsForExtendedPreloadingTest,
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleHostIsNonUnique, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleDataSaverEnabled, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleRetryAfter, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker, 1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleUserHasCookies, 1);
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchNotUsedCookiesChanged, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy,
      1);
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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleExistingProxy, 1);
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
  Navigate(GURL("https://example.com"),
           main_rfh()->GetProcess()->GetDeprecatedID(), other_token,
           different_document_token);

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com"),
                                  different_document_token));
  std::optional<PrefetchServingPageMetrics> serving_page_metrics =
      GetMetricsForMostRecentNavigation();
  EXPECT_FALSE(serving_page_metrics);
}

class PrefetchServiceWithHTMLOnlyTest
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceWithHTMLOnlyTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    // Override `kPrefetchUseContentRefactor`.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "0"},
           {"prefetch_container_lifetime_s", "-1"},
           {"html_only", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceWithHTMLOnlyTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

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
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceAlwaysMakeDecoyRequestTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    // Override `kPrefetchUseContentRefactor`.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {{"ineligible_decoy_request_probability", "1"},
           {"prefetch_container_lifetime_s", "-1"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAlwaysMakeDecoyRequestTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

TEST_P(PrefetchServiceAlwaysMakeDecoyRequestTest, DecoyRequest) {
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

TEST_P(PrefetchServiceAlwaysMakeDecoyRequestTest,
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

TEST_P(PrefetchServiceAlwaysMakeDecoyRequestTest,
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

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysMakeDecoyRequestTest,
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
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceIncognitoTest() : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
  }

 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<TestBrowserContext>();
    browser_context->set_is_off_the_record(true);
    return browser_context;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceIncognitoTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

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

  histogram_tester.ExpectUniqueSample(
      "Preloading.Prefetch.PrefetchStatus",
      PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition, 1);
  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kNonDefaultStoragePartition);

  NavigateInitiatedByRenderer(GURL("https://example.com"));
  EXPECT_FALSE(GetPrefetchToServe(GURL("https://example.com")));
  ExpectServingMetrics(
      PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition,
      /*prefetch_header_latency=*/false,
      /*required_private_prefetch_proxy=*/false);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceTest, DISABLED_CHROMEOS(StreamingURLLoaderSuccessCase)) {
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

TEST_P(PrefetchServiceTest, NoVarySearchSuccessCase_Embedder) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  auto handle =
      MakePrefetchFromEmbedder(GURL("https://example.com?a=1"),
                               PrefetchType(PreloadingTriggerType::kEmbedder,
                                            /*use_prefetch_proxy=*/false));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestStateForEmbedders(GURL("https://example.com?a=1"),
                                       {.use_prefetch_proxy = false});
  MakeResponseAndWait(
      net::HTTP_OK, net::OK, kHTMLMimeType,
      /*use_prefetch_proxy=*/false,
      {{"X-Testing", "Hello World"}, {"No-Vary-Search", R"(params=("a"))"}},
      kHTMLBody);

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

  PrefetchContainer::Reader serveable_reader =
      GetPrefetchToServe(GURL("https://example.com"), std::nullopt);
  ExpectServingReaderSuccess(serveable_reader);
  EXPECT_EQ(serveable_reader.GetPrefetchContainer()->GetURL(),
            GURL("https://example.com/?a=1"));
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceTest,
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

class PrefetchServiceAlwaysBlockUntilHeadTest
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<
          std::tuple<PrefetchServiceRearchParam::Arg,
                     blink::mojom::SpeculationEagerness>> {
 public:
  PrefetchServiceAlwaysBlockUntilHeadTest()
      : WithPrefetchServiceRearchParam(std::get<0>(GetParam())) {}

  const int kPrefetchTimeout = 10000;
  const int kBlockUntilHeadTimeout = 1000;
  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    // Override `kPrefetchUseContentRefactor`.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchUseContentRefactor,
          {
              {"ineligible_decoy_request_probability", "0"},
              {"prefetch_container_lifetime_s", "-1"},
              {"prefetch_timeout_ms", "10000"},
              // Initialize timeouts > 0ms for testing purposes.
              {"block_until_head_timeout_eager_prefetch", "1000"},
              {"block_until_head_timeout_moderate_prefetch", "1000"},
              {"block_until_head_timeout_conservative_prefetch", "1000"},
          }}},
        {});
  }

  blink::mojom::SpeculationEagerness GetEagernessParam() {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAlwaysBlockUntilHeadTest,
    testing::Combine(
        testing::ValuesIn(PrefetchServiceRearchParam::Params()),
        testing::Values(blink::mojom::SpeculationEagerness::kModerate,
                        blink::mojom::SpeculationEagerness::kConservative)));

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(BlockUntilHeadReceived)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam(),
                        /*is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      base::Milliseconds(500), 1);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(
      GURL("https://example.com/index.html?a=5"), prefetch_type,
      /* referrer */ blink::mojom::Referrer(), std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam(),
                        /* is_accurate=*/true);
  ExpectServingReaderSuccess(serveable_reader);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      base::Milliseconds(600), 1);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(
      GURL("https://example.com/index.html?a=5"), prefetch_type,
      /* referrer */ blink::mojom::Referrer(),
      /* no_vary_search_hint */ std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam());
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(700), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(
      GURL("https://example.com/index.html?a=5"), prefetch_type,
      /* referrer */ blink::mojom::Referrer(),
      /* no_vary_search_hint */ std::move(no_vary_search_hint));
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com/index.html?a=5"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

  // Navigate to the URL before the head of the prefetch response is received
  NavigateInitiatedByRenderer(GURL("https://example.com/index.html"));

  // Request the prefetch from the PrefetchService. The given callback shouldn't
  // be called until after the head is received.
  base::test::TestFuture<PrefetchContainer::Reader> future;
  GetPrefetchToServe(future, GURL("https://example.com/index.html"),
                     MainDocumentToken());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency));

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam());
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedCookiesChangedWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
                        .eagerness = GetEagernessParam()});

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(800), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedTimeoutWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  PrefetchType prefetch_type(PreloadingTriggerType::kSpeculationRule,
                             /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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

  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_TIMED_OUT,
                               GetEagernessParam(),
                               /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError);

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  base::TimeDelta block_until_head_timeout =
      PrefetchBlockUntilHeadTimeout(prefetch_type, /*is_nav_prerender=*/false);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      block_until_head_timeout, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedTimeoutWhileBlockUntilHeadForOlderNavigation)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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

  EXPECT_TRUE(first_future.IsReady());
  EXPECT_TRUE(second_future.IsReady());
  PrefetchContainer::Reader serveable_reader = second_future.Take();
  EXPECT_FALSE(serveable_reader);
  ExpectPrefetchFailedNetError(histogram_tester, net::ERR_TIMED_OUT,
                               GetEagernessParam(),
                               /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError,
                       /*prefetch_header_latency=*/false,
                       /*required_private_prefetch_proxy=*/false);

  // The metrics are recorded for the first and the second navigation, as the
  // PrefetchContainers were initially considered as a candidate at the time of
  // navigation start but decided not to be used later (after
  // `kBlockUntilHeadTimeout` msec) due to timeout.
  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(kBlockUntilHeadTimeout), 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 2);

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
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(kBlockUntilHeadTimeout), 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 2);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(FailedNetErrorWhileBlockUntilHead)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
                               GetEagernessParam(),
                               /*is_accurate_triggering=*/true);
  ExpectServingMetrics(PrefetchStatus::kPrefetchFailedNetError,
                       /*prefetch_header_latency=*/false,
                       /*required_private_prefetch_proxy=*/false);

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(300), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40064525): For NVSBlockUntilHeadReceivedOneMatchOneTimeout,
// FailedCookiesChangedAfterPrefetchStartedTimedoutNVSHintPrefetch,
// FailedCookiesChangedAfterPrefetchStartedNVSHintPrefetch and
// NVSBlockUntilHeadReceivedMultipleMatchesByNVSHint, consider only keeping one
// of them and removing the remaining, as they almost test the same logic.
// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(
    PrefetchServiceAlwaysBlockUntilHeadTest,
    DISABLED_CHROMEOS_AND_CASTOS(NVSBlockUntilHeadReceivedOneMatchOneTimeout)) {
  // The scenario is:
  // * Prefetch https://example.com/index.html?a=5 with NVS hint to
  //   ignore "a" and send request.
  // * Queue a prefetch for https://example.com/index.html?b=3 with NVS hint to
  //   match but send no request.
  // * Navigate to https://example.com/index.html.
  // * Receive a response for the first prefetch containing NVS header
  //   equivalent to the NVS hint.
  // * Expect https://example.com/index.html?a=5 to be served.
  const std::string kTestUrl = "https://example.com/index.html";
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(2));

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam());

  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    MakePrefetchOnMainFrame(
        GURL(kTestUrl + "?a=5"), prefetch_type,
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();

    VerifyCommonRequestState(GURL(kTestUrl + "?a=5"),
                             {.expected_priority = ExpectedPriorityForEagerness(
                                  GetEagernessParam())});
  }
  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"b"}));
    MakePrefetchOnMainFrame(
        GURL(kTestUrl + "?b=3"), prefetch_type,
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

  task_environment()->FastForwardBy(
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency));
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
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency), 1);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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
      GURL(kTestUrl),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam()));
  VerifyCommonRequestState(
      GURL(kTestUrl),
      {.expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});
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
                     /*use_prefetch_proxy=*/false, GetEagernessParam()),
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();
    VerifyCommonRequestStateByUrl(
        GURL(kTestUrl + "?a=1"),
        {.expected_priority =
             ExpectedPriorityForEagerness(GetEagernessParam())});
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
}

// TODO(crbug.com/40249481): Test flaky on trybots.
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
      GURL(kTestUrl),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam()));
  VerifyCommonRequestState(
      GURL(kTestUrl),
      {.expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});
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
                     /*use_prefetch_proxy=*/false, GetEagernessParam()),
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

// TODO(crbug.com/40249481): Test flaky on trybots.
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

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false, GetEagernessParam());

  {
    network::mojom::NoVarySearchPtr no_vary_search_hint =
        network::mojom::NoVarySearch::New();
    no_vary_search_hint->vary_on_key_order = true;
    no_vary_search_hint->search_variance =
        network::mojom::SearchParamsVariance::NewNoVaryParams(
            std::vector<std::string>({"a"}));
    GURL not_matched_url = GURL(kTestUrl + "?a=5");
    MakePrefetchOnMainFrame(
        not_matched_url, prefetch_type,
        /* referrer */ blink::mojom::Referrer(),
        /* no_vary_search_hint */ std::move(no_vary_search_hint));
    task_environment()->RunUntilIdle();

    VerifyCommonRequestState(not_matched_url,
                             {.expected_priority = ExpectedPriorityForEagerness(
                                  GetEagernessParam())});
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
        matched_url, prefetch_type,
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
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(0), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(BlockUntilHeadTimedout)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());

  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();
  EXPECT_FALSE(serveable_reader);

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(1000), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(HeadReceivedBeforeTimeout)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();
  EXPECT_FALSE(serveable_reader);

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(1000), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 1);
}

// TODO(crbug.com/40249481): Test flaky on trybots.
TEST_P(PrefetchServiceAlwaysBlockUntilHeadTest,
       DISABLED_CHROMEOS(MultipleGetPrefetchToServe)) {
  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  const PrefetchType prefetch_type =
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true, GetEagernessParam());
  MakePrefetchOnMainFrame(GURL("https://example.com"), prefetch_type);
  task_environment()->RunUntilIdle();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {.use_prefetch_proxy = true,
       .expected_priority = ExpectedPriorityForEagerness(GetEagernessParam())});

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
  EXPECT_TRUE(first_future.IsReady());
  EXPECT_TRUE(second_future.IsReady());
  PrefetchContainer::Reader serveable_reader = second_future.Take();
  EXPECT_FALSE(serveable_reader);

  // If the prefetch is received after the block until head has timed out, it
  // will not be used.
  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/true,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Check the metrics now that the prefetch is complete.
  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        GetEagernessParam(),
                        /*is_accurate=*/true);
  ExpectServingMetricsSuccess();

  std::string histogram_suffix =
      GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type, std::nullopt);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.Served.%s",
          histogram_suffix),
      0);
  histogram_tester.ExpectUniqueTimeSample(
      base::StringPrintf(
          "Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.NotServed.%s",
          histogram_suffix),
      base::Milliseconds(1000), 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.%s",
          histogram_suffix),
      true, 2);
}

// Tests that the prefetch eviction for eligible but not started triggers (i.e.
// `PreloadingAttempt`'s `PreloadingHoldbackStatus` is `kUnspecified`) causes no
// crash. This is a regression test of crbug.com/404703517.
TEST_P(PrefetchServiceTest, PrefetchEvictionForEligibleButNotStartedPrefetch) {
  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/2));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();
  auto candidate_2 = candidate_1.Clone();
  candidate_2->url = url_2;

  // Send `candidate_1` and `candidate_2`.
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_2.Clone());
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();
  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(), url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(), url_2))[0];

  // `candidate_1` should be started, while `candidate_2` stays in a queue.
  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  // Try to evict.
  prefetch_service->EvictPrefetchesForBrowsingDataRemoval(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve)
          ->BuildStorageKeyFilter(),
      PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved);

  // - `candidate_1` should have `PreloadingEligibility::kEligible`,
  //   `PreloadingHoldbackStatus::kAllowed` and
  //   `PreloadingTriggeringOutcome::kFailure` as
  //   `kPrefetchEvictedAfterBrowsingDataRemoved`.
  // - `candidate_2` should have `PreloadingEligibility::kEligible` but
  //   `PreloadingHoldbackStatus::kUnspecified` (as the holdback status will be
  //   determined when the prefetch is actually started) and
  //   `PreloadingTriggeringOutcome::kUnspecified`.
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    ASSERT_EQ(actual_attempts.size(), 2u);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kAllowed,
             PreloadingTriggeringOutcome::kFailure,
             ToPreloadingFailureReason(
                 PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved),
             /*accurate=*/false,
             /*ready_time=*/std::nullopt,
             blink::mojom::SpeculationEagerness::kEager),
         attempt_entry_builder()->BuildEntry(
             source_id, PreloadingType::kPrefetch,
             PreloadingEligibility::kEligible,
             PreloadingHoldbackStatus::kUnspecified,
             PreloadingTriggeringOutcome::kUnspecified,
             PreloadingFailureReason::kUnspecified,
             /*accurate=*/false,
             /*ready_time=*/std::nullopt,
             blink::mojom::SpeculationEagerness::kEager)};
    ASSERT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

// Tests that the prefetch eviction during eligiblity check (i.e.
// `PreloadingAttempt`'s `PreloadingEligibility` is `kUnspecified`) causes no
// crash. This is a regression test of crbug.com/404703517.
TEST_P(PrefetchServiceTest, PrefetchEvictionDuringEligiblityCheck) {
  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/1));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  // Pause the elibility check.
  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  prefetch_service->SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  const auto url_1 = GURL("https://example.com/one");
  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();

  // Send `candidate_1`;
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(), url_1))[0];

  // `candidate_1` should be on a way of eligibility check.
  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kNotStarted);

  // Try to evict.
  prefetch_service->EvictPrefetchesForBrowsingDataRemoval(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve)
          ->BuildStorageKeyFilter(),
      PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved);
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    ASSERT_EQ(actual_attempts.size(), 1u);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
            source_id, PreloadingType::kPrefetch,
            PreloadingEligibility::kUnspecified,
            PreloadingHoldbackStatus::kUnspecified,
            PreloadingTriggeringOutcome::kUnspecified,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/std::nullopt,
            blink::mojom::SpeculationEagerness::kEager)};
    ASSERT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }

  prefetch_service->SetDelayEligibilityCheckForTesting(base::NullCallback());
}

// Tests that the prefetch eviction for heldback triggers causes no crash. This
// is a regression test of crbug.com/404703517.
TEST_P(PrefetchServiceTest, PrefetchEvictionWhenHoldback) {
  content::test::PreloadingConfigOverride preloading_config_override;
  preloading_config_override.SetHoldback(
      PreloadingType::kPrefetch,
      content_preloading_predictor::kSpeculationRules, true);

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/1));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  const auto url_1 = GURL("https://example.com/one");
  auto candidate_1 = blink::mojom::SpeculationCandidate::New();
  candidate_1->url = url_1;
  candidate_1->action = blink::mojom::SpeculationAction::kPrefetch;
  candidate_1->eagerness = blink::mojom::SpeculationEagerness::kEager;
  candidate_1->referrer = blink::mojom::Referrer::New();

  // Send `candidate_1`;
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.push_back(candidate_1.Clone());
  auto* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(main_rfh());
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(), url_1))[0];
  task_environment()->RunUntilIdle();

  // `candidate_1` should be failed as heldback
  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kFailedHeldback);

  // Try to evict.
  prefetch_service->EvictPrefetchesForBrowsingDataRemoval(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve)
          ->BuildStorageKeyFilter(),
      PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved);
  {
    const auto source_id = ForceLogsUploadAndGetUkmId();
    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    ASSERT_EQ(actual_attempts.size(), 1u);
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> expected_attempts =
        {attempt_entry_builder()->BuildEntry(
            source_id, PreloadingType::kPrefetch,
            PreloadingEligibility::kEligible,
            PreloadingHoldbackStatus::kHoldback,
            PreloadingTriggeringOutcome::kUnspecified,
            PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/std::nullopt,
            blink::mojom::SpeculationEagerness::kEager)};
    ASSERT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
  }
}

class PrefetchServiceNewLimitsTest
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceNewLimitsTest() : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrefetchNewLimits,
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceNewLimitsTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

TEST_P(PrefetchServiceNewLimitsTest,
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

TEST_P(PrefetchServiceNewLimitsTest, NonEagerPrefetchEvictedAtLimit) {
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

TEST_P(PrefetchServiceNewLimitsTest, PrefetchWithNoCandidateIsNotStarted) {
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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();
  VerifyCommonRequestState(url_1);

  // Remove |url_2| from the list of candidates while a prefetch for |url_1| is
  // in progress.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates);

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

TEST_P(PrefetchServiceNewLimitsTest,
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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  // Prefetch for |url_1| should have started.
  VerifyCommonRequestState(url_1);

  // Remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates);
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

TEST_P(PrefetchServiceNewLimitsTest,
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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  // Complete prefetches for |url_1| and |url_2|.
  auto prefetch_1 = CompleteExistingPrefetch(url_1);
  ASSERT_TRUE(prefetch_1);
  auto prefetch_2 = CompleteExistingPrefetch(url_2);
  ASSERT_TRUE(prefetch_2);

  // Remove |candidate_1|.
  candidates.clear();
  candidates.push_back(candidate_2.Clone());
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();
  // |prefetch_1| should have been removed.
  EXPECT_FALSE(prefetch_1);
  EXPECT_TRUE(prefetch_2);
}

// Test to see if we can re-prefetch a url whose previous prefetch expired.
TEST_P(PrefetchServiceNewLimitsTest, PrefetchReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Override `kPrefetchUseContentRefactor`.
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
  prefetch_document_manager->ProcessCandidates(candidates);
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
  prefetch_document_manager->ProcessCandidates(candidates);
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

TEST_P(PrefetchServiceNewLimitsTest, NextPrefetchQueuedImmediatelyAfterReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Override `kPrefetchUseContentRefactor`.
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
  prefetch_document_manager->ProcessCandidates(candidates);
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

// Tests that the prefetch queue is not stuck when resetting the running
// prefetch during waiting its response. Regression test for
// crbug.com/400233773.
TEST_P(PrefetchServiceTest, PrefetchQueueNotStuckWhenResettingRunningPrefetch) {
  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  // Reset the first prefetch during waiting its response. Note that it will
  // eventually destruct the loader.
  handle_1.reset();
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(prefetch_container1);
  // https://crbug.com/400233773 is fixed.
  EXPECT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

TEST_P(PrefetchServiceNewLimitsTest, PrefetchFailsAndIsReset) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  // Override `kPrefetchUseContentRefactor`.
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
  prefetch_document_manager->ProcessCandidates(candidates);
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
  histogram_tester.ExpectUniqueSample("Preloading.Prefetch.PrefetchStatus",
                                      PrefetchStatus::kPrefetchFailedNetError,
                                      1);
}

TEST_P(PrefetchServiceNewLimitsTest, EagerPrefetchLimitIsDynamic) {
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
  prefetch_document_manager->ProcessCandidates(candidates);
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
  prefetch_document_manager->ProcessCandidates(candidates);
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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  // |url_1| should not be reprefetched because we are at the limit.
  EXPECT_EQ(RequestCount(), 0);

  // Remove |candidate_2|.
  candidates.clear();
  candidates.push_back(candidate_1.Clone());
  candidates.push_back(candidate_3.Clone());
  prefetch_document_manager->ProcessCandidates(candidates);
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

TEST_P(PrefetchServiceNewLimitsTest, RemoveCandidateForFailedPrefetch) {
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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  // Prefetch for |url| should have started.
  VerifyCommonRequestState(url);
  // Send error response for prefetch of |url|
  MakeResponseAndWait(net::HTTP_OK, net::ERR_FAILED, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Remove |candidate|.
  candidates.clear();
  prefetch_document_manager->ProcessCandidates(candidates);
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
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceClientHintsTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
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

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceClientHintsTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

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
  prefetch_document_manager->ProcessCandidates(candidates);
  task_environment()->RunUntilIdle();

  // If all goes well, the service has reported that the prefetch cannot be
  // served. This could be changed in the future to instead wait for the
  // prefetch rather than cancelling it, but what's key here is that it doesn't
  // hang.
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

// Tests that the `PrefetchStreamingURLLoader` disconnection during
// `PrefetchService`'s redirection handling (starts from
// `PrefetchStreamingURLLoader::OnReceiveRedirect`, and ends until
// `PrefetchService` calls `PrefetchStreamingURLLoader::HandleRedirect`) causes
// no crash, and the corresponding prefetch should not be served.
// A regression test for crbug.com/396133768.
TEST_P(
    PrefetchServiceTest,
    DISABLED_CHROMEOS(URLLoaderDisconnectedWhileHandlingRedirectEligibilty)) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();
  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  // Set a handler so that the eligibility check sequences invocked after
  // `PrefetchService::OnPrefetchRedirect` will be paused.
  base::test::TestFuture<base::OnceClosure>
      redirect_eligibility_check_callback_future;
  prefetch_service->SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             redirect_eligibility_check_callback_future,
         base::OnceClosure callback) {
        redirect_eligibility_check_callback_future->SetValue(
            std::move(callback));
      },
      base::Unretained(&redirect_eligibility_check_callback_future)));

  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->client);
  auto redirect_head = CreateURLResponseHeadForPrefetch(
      net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
      /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com"));

  request->client->OnReceiveRedirect(redirect_info, redirect_head.Clone());
  task_environment()->RunUntilIdle();

  // Now the redirect handling is paused right before
  // `PrefetchService::OnPrefetchRedirect.`

  // Disconnect URLLoader.
  base::test::TestFuture<void> disconnect_future;
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(),
                                 GURL("https://example.com")));
  ASSERT_EQ(1u, prefetches.size());
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;
  prefetch_container->GetStreamingURLLoader()->SetOnDeletionScheduledForTests(
      disconnect_future.GetCallback());
  request->client.reset();
  ASSERT_TRUE(disconnect_future.Wait());

  // Resume the eligibility check.
  redirect_eligibility_check_callback_future.Take().Run();
  task_environment()->RunUntilIdle();

  // Now `ServableState` should be `kNotServable` since we don't have a
  // non-redirect response but `PrefetchStreamingURLLoader` is gone.
  EXPECT_EQ(prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchContainer::ServableState::kNotServable);

  // Start a navigation. The prefetch should not be served.
  std::unique_ptr<NavigationResult> navigation_result =
      SimulatePartOfNavigation(GURL("https://example.com"),
                               /*is_renderer_initiated=*/true,
                               /*is_nav_prerender=*/true);
  ASSERT_TRUE(navigation_result->reader_future.IsReady());
  EXPECT_FALSE(navigation_result->reader_future.Take());

  prefetch_service->SetDelayEligibilityCheckForTesting(base::NullCallback());
}

// Tests that the `PrefetchStreamingURLLoader` disconnection during
// `PrefetchService`'s redirection handling causes no crash, and successfully
// unblocks the navigation that potentially matches the corresponding
// prefetch and thus was blocked in the match resolver (BlockUntilHead).
// A regression test for crbug.com/396133768.√
TEST_P(
    PrefetchServiceTest,
    DISABLED_CHROMEOS(
        URLLoaderDisconnectedWhileHandlingRedirectEligibilty_BlockUntilHead)) {
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/true,
                   blink::mojom::SpeculationEagerness::kEager));
  task_environment()->RunUntilIdle();
  VerifyCommonRequestState(GURL("https://example.com"),
                           {.use_prefetch_proxy = true});
  VerifyFollowRedirectParams(0);

  // Start a navigation and invoke FindPrefetch to start a wait loop.
  std::unique_ptr<NavigationResult> navigation_result =
      SimulatePartOfNavigation(GURL("https://example.com"),
                               /*is_renderer_initiated=*/true,
                               /*is_nav_prerender=*/true);
  task_environment()->RunUntilIdle();
  ASSERT_FALSE(navigation_result->reader_future.IsReady());

  // Set a handler so that the eligibility check sequences invocked after
  // `PrefetchService::OnPrefetchRedirect` will be paused.
  base::test::TestFuture<base::OnceClosure>
      redirect_eligibility_check_callback_future;
  prefetch_service->SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             redirect_eligibility_check_callback_future,
         base::OnceClosure callback) {
        redirect_eligibility_check_callback_future->SetValue(
            std::move(callback));
      },
      base::Unretained(&redirect_eligibility_check_callback_future)));

  // Start redirecting.
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_referrer_policy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  redirect_info.new_url = GURL("https://redirect.com");
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->client);
  auto redirect_head = CreateURLResponseHeadForPrefetch(
      net::HTTP_PERMANENT_REDIRECT, kHTMLMimeType,
      /*use_prefetch_proxy=*/true, {}, GURL("https://redirect.com"));

  request->client->OnReceiveRedirect(redirect_info, redirect_head.Clone());
  task_environment()->RunUntilIdle();

  // Now the redirect handling is paused right before
  // `PrefetchService::OnPrefetchRedirect.`

  // Disconnect URLLoader.
  base::test::TestFuture<void> disconnect_future;
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(MainDocumentToken(),
                                 GURL("https://example.com")));
  ASSERT_EQ(1u, prefetches.size());
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;
  prefetch_container->GetStreamingURLLoader()->SetOnDeletionScheduledForTests(
      disconnect_future.GetCallback());
  request->client.reset();
  ASSERT_TRUE(disconnect_future.Wait());
  // `PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo` will directly
  // invoke `PrefetchMatchResolver::OnHeadDetermine`. At this point the
  // container's `ServableState` should be `kShouldBlockUntilHeadReceived` (same
  // with a normal case of an ineligible redirect), so it should be unblocked
  // for unmatched.
  // TODO(crbug.com/396133768): Explicitly check more detailed Servable/Load
  // states.
  redirect_eligibility_check_callback_future.Take().Run();
  task_environment()->RunUntilIdle();
  // The prefetch should not be served.
  EXPECT_FALSE(navigation_result->reader_future.Take());

  prefetch_service->SetDelayEligibilityCheckForTesting(base::NullCallback());
}

// Test that multiple concurrent navigations are handled correctly.
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
  scoped_feature_list.InitWithFeatures({features::kPrefetchReusable}, {});

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
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
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
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_"
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
  scoped_feature_list.InitWithFeatures({features::kPrefetchReusable}, {});

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
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
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
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_"
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
  scoped_feature_list.InitWithFeatures({features::kPrefetchReusable}, {});

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
      GURL("https://example.com/?match=1"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com/?notEventuallyMatch=1"),
      /*is_renderer_initiated=*/true, /*is_nav_prerender=*/false);
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
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_"
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
// `PrefetchContainer::OnDetectedCookiesChange()` multiple times.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(
           MultipleConcurrentNavigationBlockUntilHeadThenCookiesChanged)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kPrefetchReusable}, {});

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
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
  std::unique_ptr<NavigationResult> nav_res2 = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/false);
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
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_"
      "Eager",
      true, 2);
}

// Scenario:
//
// - Prefetch ahead of prerender A started. The eligibility check is not done
//   yet.
// - Navigation X started, which is potentially matches and eventually matches
//   to A. Blocked by A. (Regard X as prerender, while we don't assume that in
//   this test actually.)
// - The eligibility check of A scceeds. Matching process proceeds and ends as
//   success.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(PrefetchAheadOfPrerenderSuccess)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrerender2FallbackPrefetchSpecRules}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  auto& prefetch_service = *PrefetchService::GetFromFrameTreeNodeId(
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId());
  prefetch_service.SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  MakePrefetchOnMainFrame(
      GURL("https://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), network::mojom::NoVarySearchPtr(),
      /*planned_max_preloading_type=*/PreloadingType::kPrerender);
  task_environment()->RunUntilIdle();

  std::unique_ptr<NavigationResult> nav_res = SimulatePartOfNavigation(
      GURL("https://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/true);
  task_environment()->RunUntilIdle();

  // The prefetch is a match candidate, but eligibility check is not done yet.
  // Matching process is in progress.
  ASSERT_FALSE(nav_res->reader_future.IsReady());

  // Proceed to the eligibility check.
  eligibility_check_callback_future.Take().Run();

  VerifyCommonRequestState(
      GURL("https://example.com"),
      {
          .use_prefetch_proxy = false,
          .sec_purpose_header_value =
              blink::kSecPurposePrefetchPrerenderHeaderValue,
      });

  SendHeadOfResponseAndWait(net::HTTP_OK, kHTMLMimeType,
                            /*use_prefetch_proxy=*/true,
                            {{"X-Testing", "Hello World"}},
                            std::size(kHTMLBody));
  SendBodyContentOfResponseAndWait(kHTMLBody);
  CompleteResponseAndWait(net::OK, std::size(kHTMLBody));

  ExpectPrefetchSuccess(histogram_tester, std::size(kHTMLBody),
                        blink::mojom::SpeculationEagerness::kEager,
                        /*is_accurate=*/true);

  ExpectServingReaderSuccess(FROM_HERE, nav_res->reader_future.Take());
  // TODO(crbug.com/356540465): See the bug. Make PrefetchServingMetrics
  // available for multiple concurrent navigations.
  ExpectServingMetrics(
      FROM_HERE, nav_res,
      {.prefetch_status = PrefetchStatus::kPrefetchSuccessful,
       .prefetch_header_latency = base::Milliseconds(kHeaderLatency),
       .required_private_prefetch_proxy = false});

  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.AfterClick.RedirectChainSize", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate."
      "SpeculationRule_"
      "Eager",
      true, 1);

  prefetch_service.SetDelayEligibilityCheckForTesting(base::NullCallback());
}

// Scenario:
//
// - Prefetch ahead of prerender A started. The eligibility check is not done
//   yet.
// - Navigation X started, which is potentially matches to A. Blocked by A.
//   (Regard X as prerender, while we don't assume that in this test actually.)
// - The eligibility check of A failed (due to non https). Matching process ends
//   with no prefetch.
TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(PrefetchAheadOfPrerenderIneligible)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPrerender2FallbackPrefetchSpecRules}, {});

  base::HistogramTester histogram_tester;

  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>());

  base::test::TestFuture<base::OnceClosure> eligibility_check_callback_future;
  auto& prefetch_service = *PrefetchService::GetFromFrameTreeNodeId(
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId());
  prefetch_service.SetDelayEligibilityCheckForTesting(base::BindRepeating(
      [](base::test::TestFuture<base::OnceClosure>*
             eligibility_check_callback_future,
         base::OnceClosure callback) {
        eligibility_check_callback_future->SetValue(std::move(callback));
      },
      base::Unretained(&eligibility_check_callback_future)));

  MakePrefetchOnMainFrame(
      GURL("http://example.com"),
      PrefetchType(PreloadingTriggerType::kSpeculationRule,
                   /*use_prefetch_proxy=*/false,
                   blink::mojom::SpeculationEagerness::kEager),
      blink::mojom::Referrer(), network::mojom::NoVarySearchPtr(),
      /*planned_max_preloading_type=*/PreloadingType::kPrerender);
  task_environment()->RunUntilIdle();

  std::unique_ptr<NavigationResult> nav_res = SimulatePartOfNavigation(
      GURL("http://example.com"), /*is_renderer_initiated=*/true,
      /*is_nav_prerender=*/true);
  task_environment()->RunUntilIdle();

  // The prefetch is a match candidate, but eligibility check is not done yet.
  // Matching process is in progress.
  ASSERT_FALSE(nav_res->reader_future.IsReady());

  // Proceed to the eligibility check.
  eligibility_check_callback_future.Take().Run();

  ASSERT_FALSE(nav_res->reader_future.Take());

  EXPECT_EQ(RequestCount(), 0);
  ExpectPrefetchNotEligible(histogram_tester,
                            PreloadingEligibility::kSchemeIsNotHttps,
                            /*is_accurate=*/true);

  // Note that serving metrics is not recorded for the prefetch because
  // `HasPrefetchStatus()` doesn't hold in
  // `PrefetchContainer::UpdateServingPageMetrics()`.

  prefetch_service.SetDelayEligibilityCheckForTesting(base::NullCallback());
}

TEST_P(PrefetchServiceTest,
       DISABLED_CHROMEOS(IsPrefetchDuplicateSameNoVarySearchHint)) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);
  base::HistogramTester histogram_tester;
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/std::nullopt));

  std::unique_ptr<ProbePrefetchRequestStatusListener> probe_listener =
      std::make_unique<ProbePrefetchRequestStatusListener>();

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener =
          std::make_unique<TestablePrefetchRequestStatusListener>(
              probe_listener->GetWeakPtr());

  std::vector<std::string> no_vary_params = {"ts"};
  net::HttpNoVarySearchData nvs_hint =
      net::HttpNoVarySearchData::CreateFromNoVaryParams(no_vary_params, false);
  GURL pf_one_url("https://example.com/search?q=ai&ts=1000");
  std::unique_ptr<content::PrefetchHandle> handle =
      MakePrefetchFromBrowserContext(pf_one_url, nvs_hint, {},
                                     std::move(request_status_listener));
  task_environment()->RunUntilIdle();

  // Test with the "no-vary" param value changed.
  GURL pf_two_url("https://example.com/search?q=ai&ts=1001");
  EXPECT_TRUE(browser_context()->IsPrefetchDuplicate(pf_two_url, nvs_hint));

  // Test with an additional query parameter.
  GURL pf_three_url("https://example.com/search?q=ai&ts=1000&qsubts=1000");
  EXPECT_FALSE(browser_context()->IsPrefetchDuplicate(pf_three_url, nvs_hint));

  // Test with the same params with different values.
  GURL pf_four_url("https://example.com/search?q=dogs&ts=1002");
  EXPECT_FALSE(browser_context()->IsPrefetchDuplicate(pf_four_url, nvs_hint));
}

// These tests check the behavior of
// `PrefetchService::AddPrefetchContainerWithoutStartingPrefetch()` if an old
// prefetch is registered and yet another new prefetch for the same key is
// added.
class PrefetchServiceAddPrefetchContainerTest
    : public PrefetchServiceTestBase,
      public WithPrefetchServiceRearchParam,
      public ::testing::WithParamInterface<PrefetchServiceRearchParam::Arg> {
 public:
  PrefetchServiceAddPrefetchContainerTest()
      : WithPrefetchServiceRearchParam(GetParam()) {}

  void InitScopedFeatureList() override {
    InitBaseParams();
    InitRearchFeatures();
    scoped_feature_list_for_prerender2_fallback_.InitWithFeatures(
        {features::kPrerender2FallbackPrefetchSpecRules}, {});
  }

  PrefetchService& GetPrefetchService() {
    return *PrefetchService::GetFromFrameTreeNodeId(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId());
  }

  std::unique_ptr<PrefetchContainer> CreateSpeculationRulesPrefetchContainer(
      const blink::DocumentToken& document_token,
      const GURL& prefetch_url,
      PreloadingType planned_max_preloading_type) {
    auto prefetch_type =
        PrefetchType(PreloadingTriggerType::kSpeculationRule,
                     /*use_prefetch_proxy=*/true,
                     blink::mojom::SpeculationEagerness::kEager);

    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents());
    PreloadingURLMatchCallback matcher =
        PreloadingDataImpl::GetPrefetchServiceMatcher(
            GetPrefetchService(),
            PrefetchContainer::Key(document_token, prefetch_url));

    auto* attempt = static_cast<PreloadingAttemptImpl*>(
        preloading_data->AddPreloadingAttempt(
            GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type()),
            PreloadingType::kPrefetch, std::move(matcher),
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));

    attempt->SetSpeculationEagerness(prefetch_type.GetEagerness());

    return std::make_unique<PrefetchContainer>(
        static_cast<content::RenderFrameHostImpl&>(*main_rfh()), document_token,
        prefetch_url, std::move(prefetch_type), blink::mojom::Referrer(),
        std::make_optional(SpeculationRulesTags()),
        /*no_vary_search_hint=*/std::nullopt,
        /*prefetch_document_manager=*/nullptr,
        PreloadPipelineInfo::Create(planned_max_preloading_type),
        attempt->GetWeakPtr());
  }

  void AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::unique_ptr<PrefetchContainer> prefetch_container) {
    // GetPrefetchService().AddPrefetchContainerWithoutStartingPrefetch(std::move(prefetch_container));
    GetPrefetchService().AddPrefetchContainerWithoutStartingPrefetchForTesting(
        std::move(prefetch_container));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_for_prerender2_fallback_;
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    PrefetchServiceAddPrefetchContainerTest,
    testing::ValuesIn(PrefetchServiceRearchParam::Params()));

TEST_P(PrefetchServiceAddPrefetchContainerTest, ReplacesOldWithNewByDefault) {
  blink::DocumentToken document_token;

  std::unique_ptr<PrefetchContainer> prefetch_container1 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  prefetch_container1->SimulatePrefetchEligibleForTest();
  prefetch_container1->SimulatePrefetchStartedForTest();
  base::WeakPtr<PreloadingAttempt> attempt1 =
      prefetch_container1->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container1));

  std::unique_ptr<PrefetchContainer> prefetch_container2 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  base::WeakPtr<PreloadingAttempt> attempt2 =
      prefetch_container2->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container2));

  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
      GetPrefetchService().GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(document_token, GURL("https://example.com")));
  ASSERT_EQ(1u, prefetches.size());
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;

  ASSERT_EQ(attempt2.get(), prefetch_container->preloading_attempt().get());
}

TEST_P(PrefetchServiceAddPrefetchContainerTest,
       PreservesOldIfOldIsAheadOfPrerender) {
  blink::DocumentToken document_token;

  std::unique_ptr<PrefetchContainer> prefetch_container1 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrerender);
  base::WeakPtr<PreloadingAttempt> attempt1 =
      prefetch_container1->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container1));

  std::unique_ptr<PrefetchContainer> prefetch_container2 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  base::WeakPtr<PreloadingAttempt> attempt2 =
      prefetch_container2->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container2));

  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
      GetPrefetchService().GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(document_token, GURL("https://example.com")));
  ASSERT_EQ(1u, prefetches.size());
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;

  ASSERT_EQ(attempt1.get(), prefetch_container->preloading_attempt().get());
}

TEST_P(PrefetchServiceAddPrefetchContainerTest,
       ReplacesOldWithNewIfOldIsAheadOfPrerenderAndNotServable) {
  blink::DocumentToken document_token;

  std::unique_ptr<PrefetchContainer> prefetch_container1 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrerender);
  prefetch_container1->SimulatePrefetchFailedIneligibleForTest(
      PreloadingEligibility::kDataSaverEnabled);
  base::WeakPtr<PreloadingAttempt> attempt1 =
      prefetch_container1->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container1));

  std::unique_ptr<PrefetchContainer> prefetch_container2 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  base::WeakPtr<PreloadingAttempt> attempt2 =
      prefetch_container2->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container2));

  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
      GetPrefetchService().GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(document_token, GURL("https://example.com")));
  ASSERT_EQ(1u, prefetches.size());
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;

  ASSERT_EQ(attempt2.get(), prefetch_container->preloading_attempt().get());
}

TEST_P(PrefetchServiceAddPrefetchContainerTest,
       TakesOldWithAttributeMigrationIfNewIsAheadOfPrerender) {
  blink::DocumentToken document_token;

  std::unique_ptr<PrefetchContainer> prefetch_container1 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  base::WeakPtr<PreloadingAttempt> attempt1 =
      prefetch_container1->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container1));

  std::unique_ptr<PrefetchContainer> prefetch_container2 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrerender);
  base::WeakPtr<PreloadingAttempt> attempt2 =
      prefetch_container2->preloading_attempt();
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container2));

  {
    std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
        GetPrefetchService().GetAllForUrlWithoutRefAndQueryForTesting(
            PrefetchContainer::Key(document_token,
                                   GURL("https://example.com")));
    ASSERT_EQ(1u, prefetches.size());
    base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;

    ASSERT_EQ(attempt1.get(), prefetch_container->preloading_attempt().get());
    ASSERT_TRUE(prefetch_container->IsLikelyAheadOfPrerender());
  }

  // `prefetch_container1` now inherits a property `IsLikelyAheadOfPrerender()`.
  // So, it wins when yet another one is about to be added.

  std::unique_ptr<PrefetchContainer> prefetch_container3 =
      CreateSpeculationRulesPrefetchContainer(document_token,
                                              GURL("https://example.com"),
                                              PreloadingType::kPrefetch);
  AddPrefetchContainerWithoutStartingPrefetchForTesting(
      std::move(prefetch_container3));

  {
    std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>> prefetches =
        GetPrefetchService().GetAllForUrlWithoutRefAndQueryForTesting(
            PrefetchContainer::Key(document_token,
                                   GURL("https://example.com")));
    ASSERT_EQ(1u, prefetches.size());
    base::WeakPtr<PrefetchContainer> prefetch_container = prefetches[0].second;

    ASSERT_EQ(attempt1.get(), prefetch_container->preloading_attempt().get());
  }
}

// Tests behavior of concurrent prefetches.
//
// Scenario:
//
// - Three prefetch are triggered.
// - `PrefetchScheduler` starts the two of them.
// - The second one ended.
// - `PrefetchScheduler` starts the last one.
TEST_P(PrefetchServiceTest, PrefetchScheduler_RunsTwoConcurrentPrefetches) {
  if (!UsePrefetchScheduler()) {
    GTEST_SKIP() << "Assume PrefetchScheduler";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchSchedulerTesting,
        {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "2"},
         {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "2"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  const auto url_3 = GURL("https://example.com/three");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  auto handle_3 =
      MakePrefetchFromBrowserContext(url_3, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2,
      prefetch_container3;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];
  std::tie(std::ignore, prefetch_container3) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_3))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  handle_2.reset();
  EXPECT_FALSE(prefetch_container2);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

// Tests prioritizing behavior.
//
// Scenario:
//
// - Two prefetches are triggered.
// - A prefetch is triggered with high priority.
// - `PrefetchScheduler` starts the later one.
TEST_P(PrefetchServiceTest, PrefetchScheduler_Prioritize) {
  if (!(UsePrefetchScheduler() &&
        features::kPrefetchSchedulerProgressSyncBestEffort.Get())) {
    GTEST_SKIP() << "Assume PrefetchScheduler and "
                    "PrefetchSchedulerProgressSyncBestEffort";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchSchedulerTesting,
        {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "1"},
         {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "1"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  prefetch_service->GetPrefetchSchedulerForTesting()
      .SetCalculatePriorityForTesting(
          base::BindRepeating([](const PrefetchContainer& prefetch_container) {
            if (prefetch_container.GetURL().possibly_invalid_spec().ends_with(
                    "?prioritize=1")) {
              return PrefetchPriority::kHighTest;
            }

            return PrefetchPriority::kBase;
          }));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  const auto url_3 = GURL("https://example.com/two?prioritize=1");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  auto handle_3 =
      MakePrefetchFromBrowserContext(url_3, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2,
      prefetch_container3;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];
  std::tie(std::ignore, prefetch_container3) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_3))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  handle_1.reset();
  EXPECT_FALSE(prefetch_container1);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

// Tests prioritizing behavior.
//
// Scenario:
//
// - A prefetch is triggered.
// - A prefetch is triggered with high priority.
// - `PrefetchScheduler` starts the later one.
TEST_P(PrefetchServiceTest, PrefetchScheduler_Prioritize_Async) {
  if (!(UsePrefetchScheduler() &&
        !features::kPrefetchSchedulerProgressSyncBestEffort.Get())) {
    GTEST_SKIP() << "Assume PrefetchScheduler and not "
                    "PrefetchSchedulerProgressSyncBestEffort";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kPrefetchSchedulerTesting,
        {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "1"},
         {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "1"}}}},
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  prefetch_service->GetPrefetchSchedulerForTesting()
      .SetCalculatePriorityForTesting(
          base::BindRepeating([](const PrefetchContainer& prefetch_container) {
            if (prefetch_container.GetURL().possibly_invalid_spec().ends_with(
                    "?prioritize=1")) {
              return PrefetchPriority::kHighTest;
            }

            return PrefetchPriority::kBase;
          }));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two?prioritize=1");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

// Tests bursting behavior.
//
// Scenario:
//
// - Two prefetches are triggered.
// - `PrefetchScheduler` starts the first one.
// - Two prefetches are triggered with burst.
// - `PrefetchScheduler` starts the third one.
// - The third one ended.
// - `PrefetchScheduler` starts the fourth one.
TEST_P(PrefetchServiceTest, PrefetchScheduler_Burst) {
  if (!UsePrefetchScheduler()) {
    GTEST_SKIP() << "Assume PrefetchScheduler";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {
          {features::kPrefetchSchedulerTesting,
           {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "1"},
            {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "2"}}},
      },
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  prefetch_service->GetPrefetchSchedulerForTesting()
      .SetCalculatePriorityForTesting(
          base::BindRepeating([](const PrefetchContainer& prefetch_container) {
            if (prefetch_container.GetURL().possibly_invalid_spec().ends_with(
                    "?burst=1")) {
              return PrefetchPriority::kBurstTest;
            }

            return PrefetchPriority::kBase;
          }));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  const auto url_3 = GURL("https://example.com/three?burst=1");
  const auto url_4 = GURL("https://example.com/four?burst=1");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2,
      prefetch_container3, prefetch_container4;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  auto handle_3 =
      MakePrefetchFromBrowserContext(url_3, std::nullopt, {}, nullptr);
  auto handle_4 =
      MakePrefetchFromBrowserContext(url_4, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  std::tie(std::ignore, prefetch_container3) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_3))[0];
  std::tie(std::ignore, prefetch_container4) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_4))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  handle_3.reset();
  EXPECT_FALSE(prefetch_container3);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

// Tests bursting behavior.
//
// Scenario:
//
// - Two prefetches are triggered.
// - Two prefetches are triggered with burst.
// - `PrefetchScheduler` starts the first and third one.
// - The third one ended.
// - `PrefetchScheduler` starts the forth one.
// - The first one ended.
// - `PrefetchScheduler` doesn't start the second one as
//   `ActiveSetSizeLimitForBase` is 1.
TEST_P(PrefetchServiceTest, PrefetchScheduler_BurstTakesPriority) {
  if (!(UsePrefetchScheduler() &&
        features::kPrefetchSchedulerProgressSyncBestEffort.Get())) {
    GTEST_SKIP() << "Assume PrefetchScheduler and "
                    "PrefetchSchedulerProgressSyncBestEffort";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {
          {features::kPrefetchSchedulerTesting,
           {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "1"},
            {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "2"}}},
      },
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  prefetch_service->GetPrefetchSchedulerForTesting()
      .SetCalculatePriorityForTesting(
          base::BindRepeating([](const PrefetchContainer& prefetch_container) {
            if (prefetch_container.GetURL().possibly_invalid_spec().ends_with(
                    "?burst=1")) {
              return PrefetchPriority::kBurstTest;
            }

            return PrefetchPriority::kBase;
          }));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  const auto url_3 = GURL("https://example.com/three?burst=1");
  const auto url_4 = GURL("https://example.com/four?burst=1");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  auto handle_3 =
      MakePrefetchFromBrowserContext(url_3, std::nullopt, {}, nullptr);
  auto handle_4 =
      MakePrefetchFromBrowserContext(url_4, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2,
      prefetch_container3, prefetch_container4;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];
  std::tie(std::ignore, prefetch_container3) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_3))[0];
  std::tie(std::ignore, prefetch_container4) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_4))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);

  handle_3.reset();
  EXPECT_FALSE(prefetch_container3);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);

  handle_1.reset();
  EXPECT_FALSE(prefetch_container1);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

// Tests bursting behavior.
//
// Scenario:
//
// - Two prefetches are triggered.
// - Two prefetches are triggered with burst.
// - `PrefetchScheduler` starts the third and fourth one.
// - The third one ended.
// - `PrefetchScheduler` doesn't start the first/second one as
//   `ActiveSetSizeLimitForBase` is 1.
TEST_P(PrefetchServiceTest, PrefetchScheduler_BurstTakesPriority_Async) {
  if (!(UsePrefetchScheduler() &&
        !features::kPrefetchSchedulerProgressSyncBestEffort.Get())) {
    GTEST_SKIP() << "Assume PrefetchScheduler and not "
                    "PrefetchSchedulerProgressSyncBestEffort";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {
          {features::kPrefetchSchedulerTesting,
           {{"kPrefetchSchedulerTestingActiveSetSizeLimitForBase", "1"},
            {"kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", "2"}}},
      },
      {});

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context())->GetPrefetchService();

  prefetch_service->GetPrefetchSchedulerForTesting()
      .SetCalculatePriorityForTesting(
          base::BindRepeating([](const PrefetchContainer& prefetch_container) {
            if (prefetch_container.GetURL().possibly_invalid_spec().ends_with(
                    "?burst=1")) {
              return PrefetchPriority::kBurstTest;
            }

            return PrefetchPriority::kBase;
          }));

  const auto url_1 = GURL("https://example.com/one");
  const auto url_2 = GURL("https://example.com/two");
  const auto url_3 = GURL("https://example.com/three?burst=1");
  const auto url_4 = GURL("https://example.com/four?burst=1");
  auto handle_1 =
      MakePrefetchFromBrowserContext(url_1, std::nullopt, {}, nullptr);
  auto handle_2 =
      MakePrefetchFromBrowserContext(url_2, std::nullopt, {}, nullptr);
  auto handle_3 =
      MakePrefetchFromBrowserContext(url_3, std::nullopt, {}, nullptr);
  auto handle_4 =
      MakePrefetchFromBrowserContext(url_4, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  base::WeakPtr<PrefetchContainer> prefetch_container1, prefetch_container2,
      prefetch_container3, prefetch_container4;
  std::tie(std::ignore, prefetch_container1) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_1))[0];
  std::tie(std::ignore, prefetch_container2) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_2))[0];
  std::tie(std::ignore, prefetch_container3) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_3))[0];
  std::tie(std::ignore, prefetch_container4) =
      prefetch_service->GetAllForUrlWithoutRefAndQueryForTesting(
          PrefetchContainer::Key(std::nullopt, url_4))[0];

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container3->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);

  handle_3.reset();
  EXPECT_FALSE(prefetch_container3);
  // Resolve `PrefetchScheduler::ProgressAsync()`.
  task_environment()->RunUntilIdle();

  ASSERT_EQ(prefetch_container1->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container2->GetLoadState(),
            PrefetchContainer::LoadState::kEligible);
  ASSERT_EQ(prefetch_container4->GetLoadState(),
            PrefetchContainer::LoadState::kStarted);
}

TEST_P(PrefetchServiceTest,
       UMA_Prefetch_PrefetchContainer_AddedTo_Embedder_Success) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));

  const auto url = GURL("https://example.com/prefetched");
  auto handle = MakePrefetchFromBrowserContext(url, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  task_environment()->FastForwardBy(
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency));

  MakeResponseAndWait(net::HTTP_OK, net::OK, kHTMLMimeType,
                      /*use_prefetch_proxy=*/false,
                      {{"X-Testing", "Hello World"}}, kHTMLBody);

  // Call `PrefetchContainer::dtor()` to record UMAs.
  handle.reset();

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToInitialEligibility.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToPrefetchStarted.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToURLRequestStarted.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      kAddedToURLRequestStartLatency, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Prefetch.PrefetchContainer."
                    "AddedToHeaderDeterminedSuccessfully.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      kAddedToURLRequestStartLatency + kHeaderLatency, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Prefetch.PrefetchContainer."
                    "AddedToPrefetchCompletedSuccessfully.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      kAddedToURLRequestStartLatency + kHeaderLatency, 1);
}

TEST_P(PrefetchServiceTest,
       UMA_Prefetch_PrefetchContainer_AddedTo_Embedder_Fail) {
  base::HistogramTester histogram_tester;

  NavigateAndCommit(GURL("https://example.com"));
  MakePrefetchService(
      std::make_unique<testing::NiceMock<MockPrefetchServiceDelegate>>(
          /*num_on_prefetch_likely_calls=*/0));

  const auto url = GURL("https://example.com/prefetched");
  auto handle = MakePrefetchFromBrowserContext(url, std::nullopt, {}, nullptr);
  task_environment()->RunUntilIdle();

  task_environment()->FastForwardBy(
      base::Milliseconds(kAddedToURLRequestStartLatency + kHeaderLatency));

  // Call `PrefetchContainer::dtor()` to record UMAs.
  handle.reset();

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToInitialEligibility.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToPrefetchStarted.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0, 1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {"Prefetch.PrefetchContainer.AddedToURLRequestStarted.Embedder_",
           test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.PrefetchContainer."
                    "AddedToHeaderDeterminedSuccesfully.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({"Prefetch.PrefetchContainer."
                    "AddedToPrefetchCompletedSuccessfully.Embedder_",
                    test::kPreloadingEmbedderHistgramSuffixForTesting}),
      0);
}

}  // namespace
}  // namespace content
