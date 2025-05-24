// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace content {

using ::testing::_;

namespace {

// Make some broadly reasonable redirect info.
net::RedirectInfo SyntheticRedirect(const GURL& new_url) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = new_url;
  redirect_info.new_site_for_cookies = net::SiteForCookies::FromUrl(new_url);
  redirect_info.new_referrer = std::string();
  return redirect_info;
}

}  // namespace

std::ostream& operator<<(std::ostream& ostream, PrefetchReusableForTests v) {
  switch (v) {
    case PrefetchReusableForTests::kDisabled:
      return ostream << "AllowMultipleUses Disabled";
    case PrefetchReusableForTests::kEnabled:
      return ostream << "AllowMultipleUses Enabled";
  }
}

std::vector<PrefetchReusableForTests> PrefetchReusableValuesForTests() {
  return std::vector<PrefetchReusableForTests>{
      PrefetchReusableForTests::kDisabled, PrefetchReusableForTests::kEnabled};
}

void MakeServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container,
    network::mojom::URLResponseHeadPtr head,
    const std::string body) {
  prefetch_container->SimulatePrefetchStartedForTest();

  const GURL kTestUrl = GURL("https://test.com");

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = kTestUrl;
  request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  base::WeakPtr<PrefetchResponseReader> weak_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED();
      }),
      base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                     prefetch_container->GetWeakPtr()),
      weak_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);

  test_url_loader_factory.AddResponse(
      kTestUrl, std::move(head), body, status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);
  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  CHECK(weak_streaming_loader);
  CHECK(weak_response_reader);
  CHECK(weak_response_reader->Servable(base::TimeDelta::Max()));
}

network::TestURLLoaderFactory::PendingRequest
MakeManuallyServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container) {
  prefetch_container->SimulatePrefetchStartedForTest();

  const GURL kTestUrl = GURL("https://test.com");

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = kTestUrl;
  request->method = "GET";

  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head) {
        return std::optional<PrefetchErrorOnResponseReceived>();
      }),
      base::BindOnce(&PrefetchContainer::OnPrefetchComplete,
                     prefetch_container->GetWeakPtr()),
      base::BindRepeating([](const net::RedirectInfo& redirect_info,
                             network::mojom::URLResponseHeadPtr response_head) {
        NOTREACHED();
      }),
      base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                     prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  CHECK_EQ(test_url_loader_factory.pending_requests()->size(), 1u);
  return std::move(test_url_loader_factory.pending_requests()->at(0));
}

OnPrefetchRedirectCallback CreatePrefetchRedirectCallbackForTest(
    base::RunLoop* on_receive_redirect_loop,
    net::RedirectInfo* out_redirect_info,
    network::mojom::URLResponseHeadPtr* out_redirect_head) {
  return base::BindRepeating(
      [](base::RunLoop* on_receive_redirect_loop,
         net::RedirectInfo* out_redirect_info,
         network::mojom::URLResponseHeadPtr* out_redirect_head,
         const net::RedirectInfo& redirect_info,
         network::mojom::URLResponseHeadPtr redirect_head) {
        *out_redirect_info = redirect_info;
        *out_redirect_head = std::move(redirect_head);
        on_receive_redirect_loop->Quit();
      },
      on_receive_redirect_loop, out_redirect_info, out_redirect_head);
}

void MakeServableStreamingURLLoaderWithRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url) {
  prefetch_container->SimulatePrefetchStartedForTest();

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = original_url;
  request->method = "GET";

  base::RunLoop on_receive_redirect_loop;
  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  auto weak_first_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      *request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce(
          [](base::RunLoop* on_response_received_loop,
             network::mojom::URLResponseHead* head) {
            on_response_received_loop->Quit();
            return std::optional<PrefetchErrorOnResponseReceived>();
          },
          &on_response_received_loop),
      base::BindOnce(
          [](base::RunLoop* on_response_complete_loop,
             const network::URLLoaderCompletionStatus& completion_status) {
            on_response_complete_loop->Quit();
          },
          &on_response_complete_loop),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                     prefetch_container->GetWeakPtr()),
      weak_first_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);

  net::RedirectInfo original_redirect_info = SyntheticRedirect(redirect_url);

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, network::mojom::URLResponseHead::New(), "test body", status,
      std::move(redirects), network::TestURLLoaderFactory::kResponseDefault);
  on_receive_redirect_loop.Run();

  prefetch_container->AddRedirectHop(redirect_info);

  CHECK(weak_streaming_loader);
  weak_streaming_loader->HandleRedirect(
      PrefetchRedirectStatus::kFollow, redirect_info, std::move(redirect_head));

  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  CHECK(weak_streaming_loader);
  auto weak_second_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  weak_streaming_loader->SetResponseReader(weak_second_response_reader);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  CHECK(weak_streaming_loader);
  CHECK(weak_first_response_reader);
  CHECK(weak_second_response_reader);
  CHECK(weak_second_response_reader->Servable(base::TimeDelta::Max()));
}

void MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url) {
  prefetch_container->SimulatePrefetchStartedForTest();

  network::TestURLLoaderFactory test_url_loader_factory;
  std::unique_ptr<network::ResourceRequest> original_request =
      std::make_unique<network::ResourceRequest>();
  original_request->url = original_url;
  original_request->method = "GET";

  base::RunLoop on_receive_redirect_loop;

  net::RedirectInfo redirect_info;
  network::mojom::URLResponseHeadPtr redirect_head;

  // Simulate a PrefetchStreamingURLLoader that receives a redirect that
  // requires a change in a network context. When this happens, it will stop its
  // request, but can be used to serve the redirect. A new
  // PrefetchStreamingURLLoader will be started with a request to the redirect
  // URL.
  auto weak_first_streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      *original_request, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*timeout_duration=*/base::TimeDelta(),
      base::BindOnce([](network::mojom::URLResponseHead* head)
                         -> std::optional<PrefetchErrorOnResponseReceived> {
        NOTREACHED();
      }),
      base::BindOnce(
          [](const network::URLLoaderCompletionStatus& completion_status) {
            NOTREACHED();
          }),
      CreatePrefetchRedirectCallbackForTest(&on_receive_redirect_loop,
                                            &redirect_info, &redirect_head),
      base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                     prefetch_container->GetWeakPtr()),
      prefetch_container->GetResponseReaderForCurrentPrefetch());

  prefetch_container->SetStreamingURLLoader(weak_first_streaming_loader);

  net::RedirectInfo original_redirect_info = SyntheticRedirect(redirect_url);

  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(original_redirect_info,
                         network::mojom::URLResponseHead::New());

  test_url_loader_factory.AddResponse(
      original_url, nullptr, "", network::URLLoaderCompletionStatus(),
      std::move(redirects),
      network::TestURLLoaderFactory::kResponseOnlyRedirectsNoDestination);
  on_receive_redirect_loop.Run();

  prefetch_container->AddRedirectHop(redirect_info);

  CHECK(weak_first_streaming_loader);
  weak_first_streaming_loader->HandleRedirect(
      PrefetchRedirectStatus::kSwitchNetworkContext, redirect_info,
      std::move(redirect_head));

  std::unique_ptr<network::ResourceRequest> redirect_request =
      std::make_unique<network::ResourceRequest>();
  redirect_request->url = redirect_url;
  redirect_request->method = "GET";

  base::RunLoop on_response_received_loop;
  base::RunLoop on_response_complete_loop;

  // Starts the followup PrefetchStreamingURLLoader.
  // GetResponseReaderForCurrentPrefetch() now points to a new ResponseReader
  // after `AddRedirectHop()` above.
  base::WeakPtr<PrefetchResponseReader> weak_second_response_reader =
      prefetch_container->GetResponseReaderForCurrentPrefetch();
  auto weak_second_streaming_loader =
      PrefetchStreamingURLLoader::CreateAndStart(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory),
          *redirect_request, TRAFFIC_ANNOTATION_FOR_TESTS,
          /*timeout_duration=*/base::TimeDelta(),
          base::BindOnce(
              [](base::RunLoop* on_response_received_loop,
                 network::mojom::URLResponseHead* head) {
                on_response_received_loop->Quit();
                return std::optional<PrefetchErrorOnResponseReceived>();
              },
              &on_response_received_loop),
          base::BindOnce(
              [](base::RunLoop* on_response_complete_loop,
                 const network::URLLoaderCompletionStatus& completion_status) {
                on_response_complete_loop->Quit();
              },
              &on_response_complete_loop),
          base::BindRepeating(
              [](const net::RedirectInfo& redirect_info,
                 network::mojom::URLResponseHeadPtr response_head) {
                NOTREACHED();
              }),
          base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                         prefetch_container->GetWeakPtr()),
          weak_second_response_reader);

  prefetch_container->SetStreamingURLLoader(weak_second_streaming_loader);

  network::URLLoaderCompletionStatus status(net::OK);
  test_url_loader_factory.AddResponse(
      redirect_url, network::mojom::URLResponseHead::New(), "test body", status,
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::kResponseDefault);

  on_response_received_loop.Run();
  on_response_complete_loop.Run();

  // `weak_first_streaming_loader` should be deleted after
  // `HandleRedirect(kSwitchNetworkContext)`.
  CHECK(!weak_first_streaming_loader);

  CHECK(weak_second_streaming_loader);
  CHECK(weak_second_response_reader);
  CHECK(weak_second_response_reader->Servable(base::TimeDelta::Max()));
}

PrefetchTestURLLoaderClient::PrefetchTestURLLoaderClient() = default;
PrefetchTestURLLoaderClient::~PrefetchTestURLLoaderClient() = default;

mojo::PendingReceiver<network::mojom::URLLoader>
PrefetchTestURLLoaderClient::BindURLloaderAndGetReceiver() {
  return remote_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<network::mojom::URLLoaderClient>
PrefetchTestURLLoaderClient::BindURLLoaderClientAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void PrefetchTestURLLoaderClient::DisconnectMojoPipes() {
  remote_.reset();
  receiver_.reset();
}

void PrefetchTestURLLoaderClient::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  NOTREACHED();
}

void PrefetchTestURLLoaderClient::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  CHECK(!cached_metadata);
  CHECK(!body_);
  body_ = std::move(body);

  if (auto_draining_) {
    StartDraining();
  }
}

void PrefetchTestURLLoaderClient::StartDraining() {
  // Drains |body_| into |body_content_|
  CHECK(body_);
  CHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body_));
}

void PrefetchTestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  received_redirects_.emplace_back(redirect_info, std::move(head));
}

void PrefetchTestURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  NOTREACHED();
}

void PrefetchTestURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  total_transfer_size_diff_ += transfer_size_diff;
}

void PrefetchTestURLLoaderClient::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  completion_status_ = status;
}

void PrefetchTestURLLoaderClient::OnDataAvailable(
    base::span<const uint8_t> data) {
  body_content_.append(base::as_string_view(data));
  total_bytes_read_ += data.size();
}

void PrefetchTestURLLoaderClient::OnDataComplete() {
  body_finished_ = true;
}

ScopedMockContentBrowserClient::ScopedMockContentBrowserClient() {
  old_browser_client_ = SetBrowserClientForTesting(this);

  // Ignore `WillCreateURLLoaderFactory(kDocumentSubResource)` calls (triggered
  // by e.g. `RenderFrameHostImpl::CommitNavigation()`) to suppress gmock
  // warning/failures. This is safe to ignore, because prefetch-related tests
  // are only for navigational prefetch and don't care about unrelated
  // subresource URLLoaderFactories.
  EXPECT_CALL(
      *this,
      WillCreateURLLoaderFactory(
          _, _, _,
          ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource, _,
          _, _, _, _, _, _, _, _, _))
      .Times(::testing::AnyNumber());
}

ScopedMockContentBrowserClient::~ScopedMockContentBrowserClient() {
  EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
}

TestPrefetchService::TestPrefetchService(BrowserContext* browser_context)
    : PrefetchService(browser_context) {}

TestPrefetchService::~TestPrefetchService() = default;

void TestPrefetchService::PrefetchUrl(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  prefetch_container->DisablePrecogLoggingForTest();
  prefetches_.push_back(prefetch_container);
}

void TestPrefetchService::EvictPrefetch(size_t index) {
  ASSERT_LT(index, prefetches_.size());
  ASSERT_TRUE(prefetches_[index]);
  base::WeakPtr<PrefetchContainer> prefetch_container = prefetches_[index];
  prefetches_.erase(prefetches_.begin() + index);
  MayReleasePrefetch(prefetch_container);
}

PrefetchingMetricsTestBase::PrefetchingMetricsTestBase()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

PrefetchingMetricsTestBase::~PrefetchingMetricsTestBase() = default;

void PrefetchingMetricsTestBase::SetUp() {
  RenderViewHostTestHarness::SetUp();
  test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  attempt_entry_builder_ =
      std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
          content_preloading_predictor::kSpeculationRules);
}
void PrefetchingMetricsTestBase::TearDown() {
  test_ukm_recorder_.reset();
  attempt_entry_builder_.reset();
  RenderViewHostTestHarness::TearDown();
}

void PrefetchingMetricsTestBase::ExpectPrefetchNoNetErrorOrResponseReceived(
    const base::HistogramTester& histogram_tester,
    bool is_eligible,
    bool browser_initiated_prefetch) {
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

  if (!browser_initiated_prefetch) {
    std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
        PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
    EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
    EXPECT_EQ(referring_page_metrics->prefetch_eligible_count,
              is_eligible ? 1 : 0);
    EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);
  }
}

void PrefetchingMetricsTestBase::ExpectPrefetchNotEligible(
    const base::HistogramTester& histogram_tester,
    PreloadingEligibility expected_eligibility,
    bool is_accurate,
    bool browser_initiated_prefetch) {
  ExpectPrefetchNoNetErrorOrResponseReceived(histogram_tester,
                                             /*is_eligible=*/false,
                                             browser_initiated_prefetch);

  if (!browser_initiated_prefetch) {
    ExpectCorrectUkmLogs({.eligibility = expected_eligibility,
                          .holdback = PreloadingHoldbackStatus::kUnspecified,
                          .outcome = PreloadingTriggeringOutcome::kUnspecified,
                          .is_accurate = is_accurate});
  }
}

void PrefetchingMetricsTestBase::ExpectPrefetchFailedBeforeResponseReceived(
    const base::HistogramTester& histogram_tester,
    PrefetchStatus expected_prefetch_status,
    bool is_accurate) {
  ExpectPrefetchNoNetErrorOrResponseReceived(histogram_tester,
                                             /*is_eligible=*/true);
  histogram_tester.ExpectUniqueSample("Preloading.Prefetch.PrefetchStatus",
                                      expected_prefetch_status, 1);
  ExpectCorrectUkmLogs(
      {.outcome = PreloadingTriggeringOutcome::kFailure,
       .failure = ToPreloadingFailureReason(expected_prefetch_status),
       .is_accurate = is_accurate});
}

void PrefetchingMetricsTestBase::ExpectPrefetchFailedNetError(
    const base::HistogramTester& histogram_tester,
    int expected_net_error_code,
    blink::mojom::SpeculationEagerness eagerness,
    bool is_accurate_triggering,
    bool browser_initiated_prefetch) {
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL", false, 1);

  histogram_tester.ExpectTotalCount("PrefetchProxy.Prefetch.Mainframe.RespCode",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "PrefetchProxy.Prefetch.Mainframe.NetError",
      std::abs(expected_net_error_code), 1);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.BodyLength", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.TotalTime", 0);
  histogram_tester.ExpectTotalCount(
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", 0);

  histogram_tester.ExpectUniqueSample("Preloading.Prefetch.PrefetchStatus",
                                      PrefetchStatus::kPrefetchFailedNetError,
                                      1);

  if (!browser_initiated_prefetch) {
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
}

void PrefetchingMetricsTestBase::ExpectPrefetchFailedAfterResponseReceived(
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
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 0);

  histogram_tester.ExpectUniqueSample("Preloading.Prefetch.PrefetchStatus",
                                      expected_prefetch_status, 1);
  ExpectCorrectUkmLogs(
      {.outcome = PreloadingTriggeringOutcome::kFailure,
       .failure = ToPreloadingFailureReason(expected_prefetch_status)});
}

void PrefetchingMetricsTestBase::ExpectPrefetchSuccess(
    const base::HistogramTester& histogram_tester,
    int expected_body_length,
    blink::mojom::SpeculationEagerness eagerness,
    bool is_accurate) {
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
      "PrefetchProxy.Prefetch.Mainframe.ConnectTime", kConnectTimeDuration, 1);

  std::optional<PrefetchReferringPageMetrics> referring_page_metrics =
      PrefetchReferringPageMetrics::GetForCurrentDocument(main_rfh());
  EXPECT_EQ(referring_page_metrics->prefetch_attempted_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_eligible_count, 1);
  EXPECT_EQ(referring_page_metrics->prefetch_successful_count, 1);

  ExpectCorrectUkmLogs({.is_accurate = is_accurate, .eagerness = eagerness});
}

ukm::SourceId PrefetchingMetricsTestBase::ForceLogsUploadAndGetUkmId(
    GURL navigate_url) {
  MockNavigationHandle mock_handle;
  mock_handle.set_is_in_primary_main_frame(true);
  mock_handle.set_is_same_document(false);
  mock_handle.set_has_committed(true);
  mock_handle.set_url(std::move(navigate_url));
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

void PrefetchingMetricsTestBase::ExpectCorrectUkmLogs(
    ExpectCorrectUkmLogsArgs args,
    GURL navigate_url) {
  const auto source_id = ForceLogsUploadAndGetUkmId(std::move(navigate_url));
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

}  // namespace content
