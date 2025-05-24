// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTIL_INTERNAL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTIL_INTERNAL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

enum class PrefetchReusableForTests { kDisabled, kEnabled };
std::ostream& operator<<(std::ostream& ostream, PrefetchReusableForTests);

std::vector<PrefetchReusableForTests> PrefetchReusableValuesForTests();

void MakeServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container,
    network::mojom::URLResponseHeadPtr head,
    const std::string body);

network::TestURLLoaderFactory::PendingRequest
MakeManuallyServableStreamingURLLoaderForTest(
    PrefetchContainer* prefetch_container);

OnPrefetchRedirectCallback CreatePrefetchRedirectCallbackForTest(
    base::RunLoop* on_receive_redirect_loop,
    net::RedirectInfo* out_redirect_info,
    network::mojom::URLResponseHeadPtr* out_redirect_head);

void MakeServableStreamingURLLoaderWithRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

void MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
    PrefetchContainer* prefetch_container,
    const GURL& original_url,
    const GURL& redirect_url);

class PrefetchTestURLLoaderClient : public network::mojom::URLLoaderClient,
                                    public mojo::DataPipeDrainer::Client {
 public:
  PrefetchTestURLLoaderClient();
  ~PrefetchTestURLLoaderClient() override;

  PrefetchTestURLLoaderClient(const PrefetchTestURLLoaderClient&) = delete;
  PrefetchTestURLLoaderClient& operator=(const PrefetchTestURLLoaderClient&) =
      delete;

  mojo::PendingReceiver<network::mojom::URLLoader>
  BindURLloaderAndGetReceiver();
  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindURLLoaderClientAndGetRemote();
  void DisconnectMojoPipes();

  // By default, auto draining is enabled, i.e. body data pipe is started
  // draining when received. If auto draining is disabled by
  // `SetAutoDraining(false)`, `StartDraining()` should be explicitly called
  // only once.
  void SetAutoDraining(bool auto_draining) { auto_draining_ = auto_draining; }
  void StartDraining();

  std::string body_content() { return body_content_; }
  uint32_t total_bytes_read() { return total_bytes_read_; }
  bool body_finished() { return body_finished_; }
  int32_t total_transfer_size_diff() { return total_transfer_size_diff_; }
  std::optional<network::URLLoaderCompletionStatus> completion_status() {
    return completion_status_;
  }
  const std::vector<
      std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>&
  received_redirects() {
    return received_redirects_;
  }

 private:
  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override;

  void OnDataComplete() override;

  mojo::Remote<network::mojom::URLLoader> remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  bool auto_draining_{true};
  mojo::ScopedDataPipeConsumerHandle body_;

  std::string body_content_;
  uint32_t total_bytes_read_{0};
  bool body_finished_{false};
  int32_t total_transfer_size_diff_{0};

  std::optional<network::URLLoaderCompletionStatus> completion_status_;

  std::vector<std::pair<net::RedirectInfo, network::mojom::URLResponseHeadPtr>>
      received_redirects_;
};

class ScopedMockContentBrowserClient : public TestContentBrowserClient {
 public:
  ScopedMockContentBrowserClient();
  ~ScopedMockContentBrowserClient() override;

  MOCK_METHOD(
      void,
      WillCreateURLLoaderFactory,
      (BrowserContext * browser_context,
       RenderFrameHost* frame,
       int render_process_id,
       URLLoaderFactoryType type,
       const url::Origin& request_initiator,
       const net::IsolationInfo& isolation_info,
       std::optional<int64_t> navigation_id,
       ukm::SourceIdObj ukm_source_id,
       network::URLLoaderFactoryBuilder& factory_builder,
       mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
           header_client,
       bool* bypass_redirect_checks,
       bool* disable_secure_dns,
       network::mojom::URLLoaderFactoryOverridePtr* factory_override,
       scoped_refptr<base::SequencedTaskRunner>
           navigation_response_task_runner),
      (override));

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

class TestPrefetchService final : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context);
  ~TestPrefetchService() override;

  void PrefetchUrl(
      base::WeakPtr<PrefetchContainer> prefetch_container) override;
  void EvictPrefetch(size_t index);

  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_;
};

// Helper for testing prefetching-side (i.e. not serving-side) metrics including
// "PrefetchProxy.Prefetch.*" UMAs, `PrefetchReferringPageMetrics` and
// Preloading_Attempt UKMs.
class PrefetchingMetricsTestBase : public RenderViewHostTestHarness {
 public:
  PrefetchingMetricsTestBase();
  ~PrefetchingMetricsTestBase() override;

  const int kTotalTimeDuration = 4321;
  const int kConnectTimeDuration = 123;

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const test::PreloadingAttemptUkmEntryBuilder* attempt_entry_builder() {
    return attempt_entry_builder_.get();
  }

  void SetUp() override;
  void TearDown() override;

  // Prefetch didn't receive any net errors nor non-redirect responses.
  // Use more specific methods below to check UKMs, if applicable.
  void ExpectPrefetchNoNetErrorOrResponseReceived(
      const base::HistogramTester& histogram_tester,
      bool is_eligible,
      bool browser_initiated_prefetch = false);

  // Prefetch was not started because it was not eligible.
  void ExpectPrefetchNotEligible(const base::HistogramTester& histogram_tester,
                                 PreloadingEligibility expected_eligibility,
                                 bool is_accurate = false,
                                 bool browser_initiated_prefetch = false);

  // Prefetch was started but failed before the final response nor any network
  // error is received.
  void ExpectPrefetchFailedBeforeResponseReceived(
      const base::HistogramTester& histogram_tester,
      PrefetchStatus expected_prefetch_status,
      bool is_accurate = false);

  // Prefetch was started but failed due to a network error, before the final
  // response is received.
  void ExpectPrefetchFailedNetError(
      const base::HistogramTester& histogram_tester,
      int expected_net_error_code,
      blink::mojom::SpeculationEagerness eagerness =
          blink::mojom::SpeculationEagerness::kEager,
      bool is_accurate_triggering = false,
      bool browser_initiated_prefetch = false);

  // Prefetch was started but failed on or after the final response is
  // received.
  void ExpectPrefetchFailedAfterResponseReceived(
      const base::HistogramTester& histogram_tester,
      net::HttpStatusCode expected_response_code,
      int expected_body_length,
      PrefetchStatus expected_prefetch_status);

  void ExpectPrefetchSuccess(const base::HistogramTester& histogram_tester,
                             int expected_body_length,
                             blink::mojom::SpeculationEagerness eagerness =
                                 blink::mojom::SpeculationEagerness::kEager,
                             bool is_accurate = false);

  // `navigate_url` is used as `MockNavigationHandle`'s URL to simulate a
  // navigation possibly using the prefetch. It is passed outside
  // `ExpectCorrectUkmLogsArgs` to keep `ExpectCorrectUkmLogsArgs` non-complex.
  ukm::SourceId ForceLogsUploadAndGetUkmId(
      GURL navigate_url = GURL("http://Not.Accurate.Trigger.Url/"));
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
  void ExpectCorrectUkmLogs(
      ExpectCorrectUkmLogsArgs args,
      GURL navigate_url = GURL("http://Not.Accurate.Trigger.Url/"));

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_TEST_UTIL_INTERNAL_H_
