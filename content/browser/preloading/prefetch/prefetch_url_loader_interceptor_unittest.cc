// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include <map>
#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_test_util_internal.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace content {
namespace {

// These tests leak mojo objects (like the PrefetchFromStringURLLoader) because
// they do not have valid mojo channels, which would normally delete the bound
// objects on destruction. This is expected and cannot be easily fixed without
// rewriting these as browsertests. The trade off for the speed and flexibility
// of unittests is an intentional decision.
#if defined(LEAK_SANITIZER)
#define DISABLE_ASAN(x) DISABLED_##x
#else
#define DISABLE_ASAN(x) x
#endif

const char kDNSCanaryCheckAddress[] = "http://testdnscanarycheck.com";
const char kTLSCanaryCheckAddress[] = "http://testtlscanarycheck.com";

void UnreachableFallback(ResponseHeadUpdateParams) {
  NOTREACHED_IN_MIGRATION();
}

// "arg" type is `url::Origin`.
MATCHER(HasOpaqueFrameOrigin, "") {
  return arg.opaque();
}

// "arg" type is `net::IsolationInfo`.
MATCHER(IsEmptyIsolationInfo, "") {
  return arg.IsEmpty();
}

class TestPrefetchOriginProber : public PrefetchOriginProber {
 public:
  TestPrefetchOriginProber(BrowserContext* browser_context,
                           bool should_probe_origins_response,
                           const GURL& probe_url,
                           PrefetchProbeResult probe_result)
      : PrefetchOriginProber(browser_context,
                             GURL(kDNSCanaryCheckAddress),
                             GURL(kTLSCanaryCheckAddress)),
        should_probe_origins_response_(should_probe_origins_response),
        probe_url_(probe_url),
        probe_result_(probe_result) {}

  TestPrefetchOriginProber(BrowserContext* browser_context,
                           bool should_probe_origins_response,
                           const GURL& probe_url)
      : PrefetchOriginProber(browser_context,
                             GURL(kDNSCanaryCheckAddress),
                             GURL(kTLSCanaryCheckAddress)),
        should_probe_origins_response_(should_probe_origins_response),
        probe_url_(probe_url) {}

  bool ShouldProbeOrigins() const override {
    return should_probe_origins_response_;
  }

  void Probe(const GURL& url, OnProbeResultCallback callback) override {
    EXPECT_TRUE(should_probe_origins_response_);
    EXPECT_EQ(url, probe_url_);

    num_probes_++;

    if (probe_result_) {
      std::move(callback).Run(probe_result_.value());
    } else {
      callback_ = std::move(callback);
    }
  }

  void FinishProbe(PrefetchProbeResult result) {
    EXPECT_TRUE(callback_);
    std::move(callback_).Run(result);
  }

  int num_probes() const { return num_probes_; }

 private:
  bool should_probe_origins_response_;

  GURL probe_url_;
  std::optional<PrefetchProbeResult> probe_result_;
  OnProbeResultCallback callback_;

  int num_probes_{0};
};

class ScopedMockContentBrowserClient : public TestContentBrowserClient {
 public:
  ScopedMockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedMockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

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

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void TakePrefetchOriginProber(
      std::unique_ptr<TestPrefetchOriginProber> test_origin_prober) {
    test_origin_prober_ = std::move(test_origin_prober);
  }

  void AddOnStartCookieCopyClosure(const GURL& prefetch_url,
                                   const GURL& redirect_url,
                                   base::OnceClosure closure) {
    auto key = std::make_pair(prefetch_url, redirect_url);
    EXPECT_TRUE(on_start_cookie_copy_closure_.find(key) ==
                on_start_cookie_copy_closure_.end());

    on_start_cookie_copy_closure_[key] = std::move(closure);
  }

  int num_probes() const { return test_origin_prober_->num_probes(); }

  TestPrefetchOriginProber* test_origin_prober() {
    return test_origin_prober_.get();
  }

 private:
  PrefetchOriginProber* GetPrefetchOriginProber() const override {
    return test_origin_prober_.get();
  }

  void CopyIsolatedCookies(const PrefetchContainer::Reader& reader) override {
    if (!reader.IsIsolatedNetworkContextRequiredToServe()) {
      return;
    }

    reader.OnIsolatedCookieCopyStart();

    auto itr = on_start_cookie_copy_closure_.find(
        std::make_pair(reader.GetPrefetchContainer()->GetURL(),
                       reader.GetCurrentURLToServe()));
    EXPECT_TRUE(itr != on_start_cookie_copy_closure_.end());
    EXPECT_TRUE(itr->second);
    std::move(itr->second).Run();
  }

  std::unique_ptr<TestPrefetchOriginProber> test_origin_prober_;

  std::map<std::pair<GURL, GURL>, base::OnceClosure>
      on_start_cookie_copy_closure_;
};

}  //  namespace

class PrefetchURLLoaderInterceptorTestBase : public RenderViewHostTestHarness {
 public:
  PrefetchURLLoaderInterceptorTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_content_browser_client_ =
        std::make_unique<ScopedMockContentBrowserClient>();

    std::unique_ptr<TestPrefetchService> prefetch_service =
        std::make_unique<TestPrefetchService>(browser_context());

    PrefetchService::SetFromFrameTreeNodeIdForTesting(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
        std::move(prefetch_service));

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());

    NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://example.com/referrer"));

    auto navigation_simulator = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://test.com"), web_contents());
    navigation_simulator->Start();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);

    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void TearDown() override {
    interceptor_.release();

    RenderViewHostTestHarness::TearDown();
  }

  TestPrefetchService* GetPrefetchService() {
    return static_cast<TestPrefetchService*>(
        PrefetchService::GetFromFrameTreeNodeId(
            web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));
  }

  RenderFrameHostImpl* main_rfhi() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  void CreateInterceptor(
      std::optional<blink::DocumentToken> initiator_document_token) {
    interceptor_ = std::make_unique<PrefetchURLLoaderInterceptor>(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
        std::move(initiator_document_token),
        /*serving_page_metrics_container=*/nullptr);
  }

  void MaybeCreateLoaderAndWait(const GURL& test_url) {
    MaybeCreateLoader(test_url);
    WaitForCallback(test_url);
  }

  void MaybeCreateLoader(const GURL& test_url) {
    network::ResourceRequest request;
    request.url = test_url;
    request.resource_type =
        static_cast<int>(blink::mojom::ResourceType::kMainFrame);
    request.method = "GET";
    MaybeCreateLoader(request);
  }

  void MaybeCreateLoader(const network::ResourceRequest& request) {
    interceptor_->MaybeCreateLoader(
        request, browser_context(),
        base::BindOnce(&PrefetchURLLoaderInterceptorTestBase::LoaderCallback,
                       base::Unretained(this), request.url),
        base::BindOnce(UnreachableFallback));
  }

  void WaitForCallback(const GURL& url) {
    auto itr = was_intercepted_.find(url);
    if (itr != was_intercepted_.end()) {
      return;
    }

    base::RunLoop run_loop;
    on_loader_callback_closure_[url] = run_loop.QuitClosure();
    run_loop.Run();
  }

  void LoaderCallback(
      const GURL& url,
      std::optional<NavigationLoaderInterceptor::Result> interceptor_result) {
    was_intercepted_[url] =
        interceptor_result &&
        interceptor_result->single_request_factory != nullptr;

    auto itr = on_loader_callback_closure_.find(url);
    if (itr != on_loader_callback_closure_.end() && itr->second) {
      std::move(itr->second).Run();
    }
  }

  std::optional<bool> was_intercepted(const GURL& url) {
    if (was_intercepted_.find(url) == was_intercepted_.end()) {
      return std::nullopt;
    }
    return was_intercepted_[url];
  }

  NavigationRequest* navigation_request() {
    return FrameTreeNode::GloballyFindByID(
               web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId())
        ->navigation_request();
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;

    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateForTesting(url, value, base::Time::Now()));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

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

    // This will run until the cookie is set.
    run_loop.Run();

    // This will run until the cookie listener gets the cookie change.
    task_environment()->RunUntilIdle();

    return result;
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  ScopedMockContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const test::PreloadingAttemptUkmEntryBuilder* attempt_entry_builder() {
    return attempt_entry_builder_.get();
  }

  void ExpectCorrectUkmLogs(const GURL& expected_url,
                            bool is_accurate_trigger,
                            PreloadingTriggeringOutcome expected_outcome,
                            PreloadingFailureReason expected_failure_reason =
                                PreloadingFailureReason::kUnspecified) {
    MockNavigationHandle mock_handle;
    mock_handle.set_is_in_primary_main_frame(true);
    mock_handle.set_is_same_document(false);
    mock_handle.set_has_committed(true);
    mock_handle.set_url(expected_url);
    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents());

    auto* preloading_data_impl =
        static_cast<PreloadingDataImpl*>(preloading_data);
    preloading_data_impl->DidStartNavigation(&mock_handle);
    preloading_data_impl->DidFinishNavigation(&mock_handle);

    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 1u);

    const auto expected_attempts = {attempt_entry_builder()->BuildEntry(
        mock_handle.GetNextPageUkmSourceId(), PreloadingType::kPrefetch,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        expected_outcome, expected_failure_reason,
        /*accurate=*/is_accurate_trigger,
        /*ready_time=*/base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
        blink::mojom::SpeculationEagerness::kEager)};

    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
    // We do not test the `PreloadingPrediction` as it is added in
    // `PreloadingDecider`.
  }

  blink::DocumentToken MainDocumentToken() {
    return static_cast<RenderFrameHostImpl*>(main_rfh())->GetDocumentToken();
  }

  void AddPrefetch(std::unique_ptr<PrefetchContainer> prefetch_container) {
    GetPrefetchService()->AddPrefetchContainerWithoutStartingPrefetch(
        std::move(prefetch_container));
  }

  std::unique_ptr<PrefetchContainer> CreateSpeculationRulesPrefetchContainer(
      const GURL& prefetch_url,
      PrefetchType prefetch_type,
      const blink::DocumentToken& referring_document_token) {
    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents());
    PreloadingURLMatchCallback matcher =
        PreloadingDataImpl::GetPrefetchServiceMatcher(
            *GetPrefetchService(),
            PrefetchContainer::Key(referring_document_token, prefetch_url));

    auto* attempt = static_cast<PreloadingAttemptImpl*>(
        preloading_data->AddPreloadingAttempt(
            GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type()),
            PreloadingType::kPrefetch, std::move(matcher),
            /*planned_max_preloading_type=*/std::nullopt,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));

    attempt->SetSpeculationEagerness(prefetch_type.GetEagerness());

    return std::make_unique<PrefetchContainer>(
        *main_rfhi(), referring_document_token, prefetch_url,
        std::move(prefetch_type), blink::mojom::Referrer(),
        /*no_vary_search_expected=*/std::nullopt,
        /*prefetch_document_manager=*/nullptr, attempt->GetWeakPtr());
  }

  std::unique_ptr<PrefetchContainer> CreateSpeculationRulesPrefetchContainer(
      const GURL& prefetch_url,
      PrefetchType prefetch_type) {
    return CreateSpeculationRulesPrefetchContainer(prefetch_url, prefetch_type,
                                                   MainDocumentToken());
  }

  std::unique_ptr<PrefetchContainer> CreateEmbedderPrefetchContainer(
      const GURL& prefetch_url,
      PrefetchType prefetch_type,
      const std::optional<url::Origin> referring_origin = std::nullopt) {
    return std::make_unique<PrefetchContainer>(
        *web_contents(), prefetch_url, std::move(prefetch_type),
        blink::mojom::Referrer(), std::move(referring_origin),
        /*no_vary_search_expected=*/std::nullopt, /*attempt=*/nullptr);
  }

  void SimulateCookieCopyProcess(PrefetchContainer& prefetch_container) {
    PrefetchContainer::Reader reader = prefetch_container.CreateReader();
    ASSERT_TRUE(reader.IsIsolatedNetworkContextRequiredToServe());
    reader.OnIsolatedCookieCopyStart();
    task_environment()->FastForwardBy(base::Milliseconds(10));
    reader.OnIsolatedCookieCopyComplete();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_for_reusable_;
  base::test::ScopedFeatureList scoped_feature_list_for_new_wait_loop_;

 private:
  std::unique_ptr<PrefetchURLLoaderInterceptor> interceptor_;

  base::HistogramTester histogram_tester_;

  std::map<GURL, bool> was_intercepted_;
  std::map<GURL, base::OnceClosure> on_loader_callback_closure_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  std::unique_ptr<ScopedMockContentBrowserClient> test_content_browser_client_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;

  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kIgnoreSignedInState};
};

namespace {

class PrefetchURLLoaderInterceptorTest
    : public PrefetchURLLoaderInterceptorTestBase,
      public ::testing::WithParamInterface<
          std::tuple<PrefetchReusableForTests,
                     /*should_enable_new_wait_loop*/ bool>> {
  void SetUp() override {
    PrefetchURLLoaderInterceptorTestBase::SetUp();

    switch (std::get<0>(GetParam())) {
      case PrefetchReusableForTests::kDisabled:
        scoped_feature_list_for_reusable_.InitAndDisableFeature(
            features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        scoped_feature_list_for_reusable_.InitAndEnableFeature(
            features::kPrefetchReusable);
        break;
    }

    if (std::get<1>(GetParam())) {
      scoped_feature_list_for_new_wait_loop_.InitAndEnableFeature(
          features::kPrefetchNewWaitLoop);
    } else {
      scoped_feature_list_for_new_wait_loop_.InitAndDisableFeature(
          features::kPrefetchNewWaitLoop);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PrefetchURLLoaderInterceptorTest,
    testing::Combine(testing::ValuesIn(PrefetchReusableValuesForTests()),
                     testing::Bool()));

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyCompleted)) {
  const GURL kTestUrl("https://foo.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));

  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Simulate the cookie copy process starting and finishing before
  // |MaybeCreateLoader| is called.
  SimulateCookieCopyProcess(*prefetch_container);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyInProgress)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Simulate the cookie copy process starting, but not finishing until after
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));

  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoader(kTestUrl);

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until after the cookie copy process is completed.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationNoCookieCopyNeeded)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  // No cookies are copied for prefetches where |use_isolated_network_context|
  // is false (i.e. same origin prefetches).
  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/false,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigation_Embedder)) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);

  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  // Creates a same-origin embedder prefetch, which means cookie copy is not
  // needed.
  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateEmbedderPrefetchContainer(
          kTestUrl,
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/false),
          url::Origin::Create(kTestUrl));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  // Creates PrefetchURLLoaderInterceptor where initiator_document_token is
  // empty (i.e., this will be the case of normal browser-initiated
  // navigations)
  CreateInterceptor(/*initiator_document_token=*/std::nullopt);
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetch)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  // With no prefetch set, the navigation shouldn't be intercepted.

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);

  auto actual = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_Attempt::kEntryName,
      test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(actual.size(), 0u);
}

// Tests that the navigation shouldn't be intercepted if there is no matching
// prefetch. Currently, a referring DocumentToken (note that thiswill be nullopt
// when browser-initiated prefetch) and a prefetch url (which compose
// PrefetchContainer::Key) will be taken into account when matching.
TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoMatching)) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchBrowserInitiatedTriggers);

  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  // Creates speculation rules prefetch that has different prefetch url from
  // kTestUrl.
  std::unique_ptr<PrefetchContainer>
      prefetch_container_speculation_rules_diff_url =
          CreateSpeculationRulesPrefetchContainer(
              GURL("https://example.com/different"),
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/false,
                           blink::mojom::SpeculationEagerness::kEager));
  prefetch_container_speculation_rules_diff_url
      ->SimulateAttemptAtInterceptorForTest();
  MakeServableStreamingURLLoaderForTest(
      prefetch_container_speculation_rules_diff_url.get(),
      network::mojom::URLResponseHead::New(), "test body");
  AddPrefetch(
      std::move(std::move(prefetch_container_speculation_rules_diff_url)));

  // Creates a speculation rules prefetch that has a different DocumentToken
  // from the current main document's.
  std::unique_ptr<PrefetchContainer>
      prefetch_container_speculation_rules_diff_token =
          CreateSpeculationRulesPrefetchContainer(
              kTestUrl,
              PrefetchType(PreloadingTriggerType::kSpeculationRule,
                           /*use_prefetch_proxy=*/false,
                           blink::mojom::SpeculationEagerness::kEager),
              blink::DocumentToken());
  prefetch_container_speculation_rules_diff_token
      ->SimulateAttemptAtInterceptorForTest();
  MakeServableStreamingURLLoaderForTest(
      prefetch_container_speculation_rules_diff_token.get(),
      network::mojom::URLResponseHead::New(), "test body");
  AddPrefetch(
      std::move(std::move(prefetch_container_speculation_rules_diff_token)));

  // Creates an embedder prefetch, whose DocumentToken will be nullopt.
  std::unique_ptr<PrefetchContainer> prefetch_container_embedder =
      CreateEmbedderPrefetchContainer(
          kTestUrl,
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/false),
          url::Origin::Create(kTestUrl));
  prefetch_container_embedder->SimulateAttemptAtInterceptorForTest();
  MakeServableStreamingURLLoaderForTest(prefetch_container_embedder.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");
  AddPrefetch(std::move(prefetch_container_embedder));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  // Neither of above three prefetches will not be served and the navigation
  // will not be intercepted, because their referring DocumentToken or/and
  // prefetch url aren't align with navigation's params.
  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);
  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  // Without a prefetched response, the navigation shouldn't be intercepted.
  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kReady);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationStalePrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Advance time enough so that the response is considered stale.
  task_environment()->FastForwardBy(2 * PrefetchCacheableDuration());

  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kReady);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationCookiesChanged)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->RegisterCookieListener(cookie_manager());
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Since the cookies associated with |kTestUrl| have changed, the prefetch can
  // no longer be served.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));

  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  // kenoss@ is not sure what this test is checking.
  //
  // - `PrefetchProxy.AccurateTriggering` should be true if a navigation started
  //   that is potentially matching to the prefetchand `PrefetchContainer` is
  //   alive at the timing.
  // - It is done in the above `MaybeCreateLoaderAndWait()`, which emulates part
  //   of navigation, `PrefetchURLLoaderInterceptor::MaybeCreateLoader()`.
  //
  // So, the prefetch is recorded as accurate by the navigation to `kTestUrl`.
  // But,
  //
  // - `ExpectCorrectUkmLogs()` emulates another part of navigation,
  //   `PreloadingDataImpl::DidStart/FinishNavigation()`.
  // - But there are discordance:
  //   - `PrefetchURLLoaderInterceptor` uses `NavigationRequest` that is taken
  //     from `FrameTreeNode`. `ExpectCorrectUkmLogs()` creates
  //     `MockNavigationHandle`. They are different.
  //   - In this test, URLs are different.
  //
  // TODO(crbug.com/359802755): Investigate more and use correct URLs.
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kFailure,
                       ToPreloadingFailureReason(
                           PrefetchStatus::kPrefetchNotUsedCookiesChanged));
}

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(ProbeSuccess)) {
  const GURL kTestUrl("https://cross-site.example");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  SimulateCookieCopyProcess(*prefetch_container);

  AddPrefetch(std::move(prefetch_container));

  // Set up |TestPrefetchOriginProber| to require a probe and simulate a
  // successful probe.
  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl,
          PrefetchProbeResult::kDNSProbeSuccess));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  EXPECT_EQ(GetPrefetchService()->num_probes(), 1);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(ProbeFailure)) {
  const GURL kTestUrl("https://cross-site.example");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  SimulateCookieCopyProcess(*prefetch_container);

  AddPrefetch(std::move(prefetch_container));

  // Set up |TestPrefetchOriginProber| to require a probe and simulate a
  // unsuccessful probe.
  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl,
          PrefetchProbeResult::kDNSProbeFailure));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  EXPECT_EQ(GetPrefetchService()->num_probes(), 1);
  // Ditto to `ExpectCorrectUkmLogs()` in
  // `DoNotInterceptNavigationCookiesChanged`.
  //
  // TODO(crbug.com/359802755): Investigate more and use correct URLs.
  ExpectCorrectUkmLogs(
      GURL("http://Not.Accurate.Trigger/"),
      /*is_accurate_trigger=*/true, PreloadingTriggeringOutcome::kFailure,
      ToPreloadingFailureReason(PrefetchStatus::kPrefetchNotUsedProbeFailed));
}

enum class NotServableReason {
  kOnCompleteFailure,
  kAnotherRequest,
  kAnotherRequestCompleted,
};

class PrefetchURLLoaderInterceptorBecomeNotServableTest
    : public PrefetchURLLoaderInterceptorTestBase,
      public ::testing::WithParamInterface<
          std::tuple<PrefetchReusableForTests,
                     /*should_enable_new_wait_loop*/ bool,
                     NotServableReason>> {
  void SetUp() override {
    PrefetchURLLoaderInterceptorTestBase::SetUp();

    switch (std::get<0>(GetParam())) {
      case PrefetchReusableForTests::kDisabled:
        scoped_feature_list_for_reusable_.InitAndDisableFeature(
            features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        scoped_feature_list_for_reusable_.InitAndEnableFeature(
            features::kPrefetchReusable);
        break;
    }

    if (std::get<1>(GetParam())) {
      scoped_feature_list_for_new_wait_loop_.InitAndEnableFeature(
          features::kPrefetchNewWaitLoop);
    } else {
      scoped_feature_list_for_new_wait_loop_.InitAndDisableFeature(
          features::kPrefetchNewWaitLoop);
    }
  }
};

TEST_P(PrefetchURLLoaderInterceptorBecomeNotServableTest, DISABLE_ASAN(Basic)) {
  // It is possible for a prefetch to initially be marked as servable, but
  // becomes not servable at some point between PrefetchURLLoaderInterceptor
  // gets the prefetch and when it tries to serve it. This can happen when
  // waiting for a probe to complete or the cookie copy to complete.

  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  mojo::ScopedDataPipeProducerHandle producer_handle;
  {
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    std::string content = "test body";
    CHECK_EQ(
        mojo::CreateDataPipe(content.size(), producer_handle, consumer_handle),
        MOJO_RESULT_OK);
    CHECK_EQ(MOJO_RESULT_OK,
             producer_handle->WriteAllData(base::as_byte_span(content)));
    pending_request.client->OnReceiveResponse(
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        std::nullopt);
  }

  // Simulate the cookie copy process starting, but not finishing until after
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  MaybeCreateLoader(request);

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until after the cookie copy process is completed.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  // Simulate the prefetch becoming not servable anymore.
  PrefetchRequestHandler another_request;
  switch (std::get<2>(GetParam())) {
    case NotServableReason::kOnCompleteFailure:
      producer_handle.reset();
      pending_request.client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      break;

    case NotServableReason::kAnotherRequest:
      // Another request is created for the same PrefetchContainer while
      // prefetching is still ongoing.
      another_request =
          weak_prefetch_container->CreateReader().CreateRequestHandler();
      break;

    case NotServableReason::kAnotherRequestCompleted:
      // Another request is created for the same PrefetchContainer while
      // prefetching is still ongoing,
      another_request =
          weak_prefetch_container->CreateReader().CreateRequestHandler();

      // and, prefetch and the other request completed.
      {
        producer_handle.reset();
        pending_request.client->OnComplete(
            network::URLLoaderCompletionStatus(net::OK));

        std::unique_ptr<PrefetchTestURLLoaderClient> client =
            std::make_unique<PrefetchTestURLLoaderClient>();

        std::move(another_request)
            .Run(request, client->BindURLloaderAndGetReceiver(),
                 client->BindURLLoaderClientAndGetRemote());
        // Wait until the URLLoaderClient completion.
        task_environment()->RunUntilIdle();
        EXPECT_EQ(client->body_content(), "test body");
        client->DisconnectMojoPipes();
      }
      break;
  }

  task_environment()->RunUntilIdle();

  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());

  switch (std::get<2>(GetParam())) {
    case NotServableReason::kOnCompleteFailure:
      EXPECT_FALSE(was_intercepted(kTestUrl).value());
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kReady);
      break;

    case NotServableReason::kAnotherRequest:
      EXPECT_FALSE(was_intercepted(kTestUrl).value());
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kSuccess);
      producer_handle.reset();
      pending_request.client->OnComplete(
          network::URLLoaderCompletionStatus(net::OK));
      task_environment()->RunUntilIdle();
      break;

    case NotServableReason::kAnotherRequestCompleted:
      switch (std::get<0>(GetParam())) {
        case PrefetchReusableForTests::kDisabled:
          EXPECT_FALSE(was_intercepted(kTestUrl).value());
          break;
        case PrefetchReusableForTests::kEnabled:
          // The first request doesn't become non-servable if
          // `kPrefetchReusable` is enabled, because after the other
          // request is done, the body tee is clonable again.
          EXPECT_TRUE(was_intercepted(kTestUrl).value());
          break;
      }
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kSuccess);
      break;
  }

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PrefetchURLLoaderInterceptorBecomeNotServableTest,
    testing::Combine(
        testing::ValuesIn(PrefetchReusableValuesForTests()),
        testing::Bool(),
        testing::Values(NotServableReason::kOnCompleteFailure,
                        NotServableReason::kAnotherRequest,
                        NotServableReason::kAnotherRequestCompleted)));

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(HandleRedirects)) {
  const GURL kTestUrl("https://example.com");
  const GURL kRedirectUrl("https://redirect.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()))
      .Times(2);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->MakeResourceRequest({});
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderWithRedirectForTest(prefetch_container.get(),
                                                    kTestUrl, kRedirectUrl);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).has_value());

  base::RunLoop on_start_cookie_copy_run_loop;
  GetPrefetchService()->AddOnStartCookieCopyClosure(
      kTestUrl, kRedirectUrl, on_start_cookie_copy_run_loop.QuitClosure());

  MaybeCreateLoader(kRedirectUrl);
  on_start_cookie_copy_run_loop.Run();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  auto reader = weak_prefetch_container->CreateReader();
  reader.AdvanceCurrentURLToServe();
  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kRedirectUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).has_value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 2);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(HandleRedirectsWithSwitchInNetworkContext)) {
  const GURL kTestUrl("https://example.com");
  const GURL kRedirectUrl("https://redirect.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          HasOpaqueFrameOrigin(), IsEmptyIsolationInfo(),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::_, testing::IsNull(), testing::NotNull(), testing::IsNull(),
          testing::IsNull(), testing::IsNull()))
      .Times(2);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->MakeResourceRequest({});
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
      prefetch_container.get(), kTestUrl, kRedirectUrl);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).has_value());

  base::RunLoop on_start_cookie_copy_run_loop;
  GetPrefetchService()->AddOnStartCookieCopyClosure(
      kTestUrl, kRedirectUrl, on_start_cookie_copy_run_loop.QuitClosure());

  MaybeCreateLoader(kRedirectUrl);

  auto reader = weak_prefetch_container->CreateReader();
  on_start_cookie_copy_run_loop.Run();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  reader.AdvanceCurrentURLToServe();
  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kRedirectUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).has_value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 2);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(HandleRedirectsWithCookieChange)) {
  const GURL kTestUrl("https://example.com");
  const GURL kRedirectUrl("https://redirect.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->MakeResourceRequest({});
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderWithRedirectForTest(prefetch_container.get(),
                                                    kTestUrl, kRedirectUrl);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoaderAndWait(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).has_value());

  // Update cookies for redirect URL. This should make the prefech unusable.
  weak_prefetch_container->RegisterCookieListener(cookie_manager());
  ASSERT_TRUE(SetCookie(kRedirectUrl, "test-cookie"));

  MaybeCreateLoaderAndWait(kRedirectUrl);

  EXPECT_TRUE(was_intercepted(kRedirectUrl).has_value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).value());

  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kFailure,
                       ToPreloadingFailureReason(
                           PrefetchStatus::kPrefetchNotUsedCookiesChanged));
}

// Regression test for crbug.com/327289525.
TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(ProbeFailsAfterPrefetchBecomesNotServable)) {
  const GURL kTestUrl("https://cross-site.example");

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtRequestStartForTest();

  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  // Start serving the response.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  {
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    std::string content = "test body";
    CHECK_EQ(
        mojo::CreateDataPipe(content.size(), producer_handle, consumer_handle),
        MOJO_RESULT_OK);
    CHECK_EQ(MOJO_RESULT_OK,
             producer_handle->WriteAllData(base::as_byte_span(content)));
    pending_request.client->OnReceiveResponse(
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        std::nullopt);
  }

  SimulateCookieCopyProcess(*prefetch_container);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));
  ASSERT_EQ(weak_prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchContainer::ServableState::kServable);

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoader(kTestUrl);

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until the origin probe is complete.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  // Simulate the prefetch completing with an error.
  producer_handle.reset();
  pending_request.client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  task_environment()->RunUntilIdle();
  // The prefetch is no longer servable, but the origin probe is still
  // running.
  ASSERT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchFailedNetError);
  EXPECT_EQ(GetPrefetchService()->num_probes(), 1);

  // Finish the origin probe now.
  GetPrefetchService()->test_origin_prober()->FinishProbe(
      PrefetchProbeResult::kDNSProbeFailure);
  // Prefetch status should be unchanged.
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchFailedNetError);

  WaitForCallback(kTestUrl);
  EXPECT_FALSE(was_intercepted(kTestUrl).value());
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(PrefetchFailsAfterProbeFails)) {
  const GURL kTestUrl("https://cross-site.example");

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      CreateSpeculationRulesPrefetchContainer(
          kTestUrl, PrefetchType(PreloadingTriggerType::kSpeculationRule,
                                 /*use_prefetch_proxy=*/true,
                                 blink::mojom::SpeculationEagerness::kEager));
  prefetch_container->SimulateAttemptAtRequestStartForTest();

  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  // Start serving the response.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  {
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    std::string content = "test body";
    CHECK_EQ(
        mojo::CreateDataPipe(content.size(), producer_handle, consumer_handle),
        MOJO_RESULT_OK);
    CHECK_EQ(MOJO_RESULT_OK,
             producer_handle->WriteAllData(base::as_byte_span(content)));
    pending_request.client->OnReceiveResponse(
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        std::nullopt);
  }

  SimulateCookieCopyProcess(*prefetch_container);

  auto weak_prefetch_container = prefetch_container->GetWeakPtr();
  AddPrefetch(std::move(prefetch_container));
  ASSERT_EQ(weak_prefetch_container->GetServableState(base::TimeDelta::Max()),
            PrefetchContainer::ServableState::kServable);

  CreateInterceptor(MainDocumentToken());
  MaybeCreateLoader(kTestUrl);

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until the origin probe is complete.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  // Finish the origin probe now.
  GetPrefetchService()->test_origin_prober()->FinishProbe(
      PrefetchProbeResult::kDNSProbeFailure);
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotUsedProbeFailed);

  // The decision to use the prefetch is now made and it won't be used because
  // the origin probe failed.
  WaitForCallback(kTestUrl);
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  // Simulate the prefetch completing with an error.
  producer_handle.reset();
  pending_request.client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  task_environment()->RunUntilIdle();

  // The prefetch status should be unchanged.
  EXPECT_EQ(weak_prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotUsedProbeFailed);
}

}  // namespace
}  // namespace content
