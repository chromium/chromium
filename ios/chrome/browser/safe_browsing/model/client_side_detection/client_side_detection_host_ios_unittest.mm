// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_host_ios.h"

#import <UIKit/UIKit.h>

#import "base/containers/span.h"
#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_tick_clock.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/safe_browsing/core/browser/db/test_database_manager.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/safebrowsing_switches.h"
#import "components/safe_browsing/core/common/threat_enums.h"
#import "components/safe_browsing/core/common/visual_utils.h"
#import "components/safe_browsing/ios/browser/client_side_detection_feature_cache.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/tabs/model/tab_helper_filter.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace safe_browsing {
namespace {

constexpr char kExampleUrl[] = "https://example.com";
constexpr std::string_view kExampleUrlPattern = "example.com/";
constexpr char kDifferentUrl[] = "https://different.example.com";
constexpr std::string_view kDifferentUrlPattern = "different.example.com/";
constexpr int kCacheDurationSec = 60;
constexpr char kPhishingUrl[] = "https://phishing.example.com";
constexpr char kReferrerUrl[] = "https://referrer.example.com/";
constexpr char kLoopbackIpStr[] = "127.0.0.1";
constexpr char kLoopbackIpUrl[] = "http://127.0.0.1";
constexpr char kPrivateIpStr[] = "192.168.1.1";
constexpr char kPrivateIpUrl[] = "http://192.168.1.1";
constexpr char kLocalhostUrl[] = "http://localhost";
constexpr char kIntranetUrl[] = "http://intranet-page";

class MockIntelligentScanDelegate
    : public safe_browsing::IntelligentScanDelegate {
 public:
  MOCK_METHOD(bool,
              ShouldRequestIntelligentScan,
              (safe_browsing::ClientPhishingRequest * verdict),
              (override));
  MOCK_METHOD(safe_browsing::IntelligentScanDelegate::ModelType,
              GetIntelligentScanModelType,
              (bool log_failed_eligibility_reason),
              (override));
  MOCK_METHOD(
      std::optional<base::UnguessableToken>,
      StartIntelligentScan,
      (std::string rendered_texts,
       safe_browsing::IntelligentScanDelegate::IntelligentScanDoneCallback
           callback),
      (override));
  MOCK_METHOD(bool,
              CancelIntelligentScan,
              (const base::UnguessableToken& scan_id),
              (override));
  MOCK_METHOD(bool,
              ShouldShowScamWarning,
              (std::optional<safe_browsing::IntelligentScanVerdict> verdict),
              (override));
  MOCK_METHOD(void, OnScamWarningShown, (), (override));
};

class FakeSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()),
        match_csd_allowlist_(false),
        match_hc_allowlist_(false) {}

  bool CanCheckUrl(const GURL& url) const override { return true; }

  AsyncMatch CheckCsdAllowlistUrl(const GURL& url, Client* client) override {
    return match_csd_allowlist_ ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH;
  }

  void CheckUrlForHighConfidenceAllowlist(
      const GURL& url,
      CheckUrlForHighConfidenceAllowlistCallback callback) override {
    std::move(callback).Run(match_hc_allowlist_, std::nullopt);
  }

  void SetMatchCsdAllowlist(bool match) { match_csd_allowlist_ = match; }
  void SetMatchHcAllowlist(bool match) { match_hc_allowlist_ = match; }

 protected:
  ~FakeSafeBrowsingDatabaseManager() override = default;

 private:
  bool match_csd_allowlist_;
  bool match_hc_allowlist_;
};

class FakeOptimizationGuideModelProvider
    : public optimization_guide::OptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {}

  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {}
};

class MockClientSideDetectionService
    : public safe_browsing::ClientSideDetectionService {
 public:
  MockClientSideDetectionService(
      PrefService* prefs,
      optimization_guide::OptimizationGuideModelProvider* opt_guide)
      : safe_browsing::ClientSideDetectionService(prefs, nullptr, opt_guide) {}
  MOCK_METHOD(void,
              ClassifyPhishingThroughThresholds,
              (safe_browsing::ClientPhishingRequest * verdict),
              (override));
  MOCK_METHOD(void,
              SendClientReportPhishingRequest,
              (std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
               safe_browsing::ClientSideDetectionServiceBase::
                   ClientReportPhishingRequestCallback callback,
               const std::string& access_token),
              (override));
  MOCK_METHOD(bool, IsModelAvailable, (), (const, override));
  MOCK_METHOD(bool, GetValidCachedResult, (const GURL&, bool*), (override));
  MOCK_METHOD(bool,
              IsPrivateIPAddress,
              (const net::IPAddress&),
              (const, override));
  MOCK_METHOD(bool, AtPhishingReportLimit, (), (override));
  MOCK_METHOD(bool, HasImageEmbeddingModel, (), (const, override));
  MOCK_METHOD(bool,
              IsModelMetadataImageEmbeddingVersionMatching,
              (),
              (const, override));
};

class MockPhishingImageEmbedder : public safe_browsing::PhishingImageEmbedder {
 public:
  MOCK_METHOD(void,
              BeginImageEmbedding,
              (const gfx::Image& image,
               bool can_extract_visual_features,
               DoneCallback callback),
              (override));
  MOCK_METHOD(void, CancelPendingImageEmbedding, (), (override));
};

std::unique_ptr<KeyedService> BuildMockClientSideDetectionService(
    ProfileIOS* profile) {
  return std::make_unique<MockClientSideDetectionService>(profile->GetPrefs(),
                                                          nullptr);
}

UIImage* CreateTestImage() {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(1, 1)];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context){
  }];
}

}  // namespace

class ClientSideDetectionHostIOSTest : public PlatformTest {
 protected:
  ClientSideDetectionHostIOSTest()
      : profile_([]() {
          TestProfileIOS::Builder builder;
          builder.AddTestingFactory(
              SyncServiceFactory::GetInstance(),
              base::BindRepeating(&CreateTestSyncService));
          builder.AddTestingFactory(
              ClientSideDetectionServiceFactory::GetInstance(),
              base::BindRepeating(&BuildMockClientSideDetectionService));
          return std::move(builder).Build();
        }()),
        mock_service_(profile_->GetPrefs(), &test_opt_guide_) {
    ClientSideDetectionServiceFactory::GetInstance();
    web_state_.SetBrowserState(profile_.get());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);

    database_manager_ = base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();
    FakeSafeBrowsingService* sb_service = static_cast<FakeSafeBrowsingService*>(
        GetApplicationContext()->GetSafeBrowsingService());
    sb_service->SetDatabaseManager(database_manager_);
    mock_service_.SetScorerForTesting(
        std::make_unique<safe_browsing::Scorer>());
  }

  void TearDown() override {
    mock_service_.SetScorerForTesting(nullptr);
    PlatformTest::TearDown();
  }

  std::unique_ptr<ClientSideDetectionHostIOS> CreateHost() {
    return std::make_unique<ClientSideDetectionHostIOS>(
        &web_state_, &mock_service_,
        VerdictCacheManagerFactory::GetForProfile(profile_.get()),
        profile_->GetPrefs(),
        IdentityManagerFactory::GetForProfile(profile_.get()),
        ios::HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS));
  }

  void set_last_request_type(ClientSideDetectionHostIOS* host,
                             safe_browsing::ClientSideDetectionType type) {
    host->set_last_request_type(type);
  }

  safe_browsing::ClientSideDetectionType last_request_type(
      ClientSideDetectionHostIOS* host) {
    return host->last_request_type();
  }

  bool is_csd_running(ClientSideDetectionHostIOS* host) {
    return host->is_csd_running();
  }

  void set_is_csd_running(ClientSideDetectionHostIOS* host, bool value) {
    host->set_is_csd_running(value);
  }

  bool is_classifying(ClientSideDetectionHostIOS* host) {
    return host->is_classifying();
  }

  void set_is_classifying(ClientSideDetectionHostIOS* host, bool value) {
    host->set_is_classifying(value);
  }

  bool should_send_as_force_request(ClientSideDetectionHostIOS* host) {
    return host->should_send_as_force_request();
  }

  void set_should_send_as_force_request(ClientSideDetectionHostIOS* host,
                                        bool value) {
    host->set_should_send_as_force_request(value);
  }

  bool trigger_model_request_sent_as_force_request(
      ClientSideDetectionHostIOS* host) {
    return host->trigger_model_request_sent_as_force_request();
  }

  void set_send_sample_ping(ClientSideDetectionHostIOS* host, bool value) {
    host->send_sample_ping_ = value;
  }

  gfx::Image classification_image(ClientSideDetectionHostIOS* host) {
    return host->classification_image_;
  }

  void set_classification_image(ClientSideDetectionHostIOS* host,
                                const gfx::Image& image) {
    host->classification_image_ = image;
  }

  void OnSnapshotReceived(ClientSideDetectionHostIOS* host,
                          const GURL& url,
                          UIImage* ui_image) {
    host->OnSnapshotReceived(url, ui_image);
  }

  void OnClassificationDone(ClientSideDetectionHostIOS* host,
                            const GURL& url,
                            const gfx::Image& image,
                            safe_browsing::ClientSideDetectionType request_type,
                            base::TimeTicks classification_start_time,
                            const safe_browsing::ClientPhishingRequest& verdict,
                            safe_browsing::PhishingClassifier::Result result) {
    host->OnClassificationDone(url, image, request_type,
                               classification_start_time, verdict, result);
  }

  visual_utils::CanExtractVisualFeaturesResult
  DetermineVisualFeaturesExtraction(ClientSideDetectionHostIOS* host) {
    return host->DetermineVisualFeaturesExtraction();
  }

  void OnImageEmbeddingDone(
      ClientSideDetectionHostIOS* host,
      std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      safe_browsing::PhishingImageEmbedder::Result result,
      const safe_browsing::ImageFeatureEmbedding& image_embedding,
      const safe_browsing::VisualFeatures& visual_features) {
    host->OnImageEmbeddingDone(
        std::move(verdict), did_match_high_confidence_allowlist, is_invalid_ip,
        result, image_embedding, visual_features);
  }

  void set_image_embedder(
      ClientSideDetectionHostIOS* host,
      std::unique_ptr<safe_browsing::PhishingImageEmbedder> embedder) {
    host->image_embedder_ = std::move(embedder);
  }

  base::TimeTicks image_embedding_start_time(ClientSideDetectionHostIOS* host) {
    return host->image_embedding_start_time();
  }

  void MaybeStartImageEmbedding(
      ClientSideDetectionHostIOS* host,
      std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist = std::nullopt,
      bool is_invalid_ip = false,
      safe_browsing::PhishingDetectorResult result =
          safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS) {
    host->MaybeStartImageEmbedding(std::move(verdict),
                                   did_match_high_confidence_allowlist,
                                   is_invalid_ip, result);
  }

  // Expects bucket count for both the unsuffixed and request-type-suffixed
  // event histograms.
  void ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent event,
      safe_browsing::ClientSideDetectionType request_type,
      base::HistogramBase::Count32 expected_count = 1) {
    histogram_tester_.ExpectBucketCount(
        "SBClientPhishing.ClientSideDetectionEvent", event, expected_count);
    histogram_tester_.ExpectBucketCount(
        base::StrCat({"SBClientPhishing.ClientSideDetectionEvent.",
                      safe_browsing::GetRequestTypeName(request_type)}),
        event, expected_count);
  }

  // Expects standard pre-classification start and complete event logs.
  void ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType request_type,
      base::HistogramBase::Count32 expected_count = 1) {
    ExpectClientSideDetectionEvent(
        ClientSideDetectionEvent::kTriggerStartsPreClassification, request_type,
        expected_count);
    ExpectClientSideDetectionEvent(
        ClientSideDetectionEvent::kPreClassificationCheckComplete, request_type,
        expected_count);
  }

  // Runs standard navigation and pre-classification trigger for sampling tests.
  void RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType request_type) {
    safe_browsing::SetSafeBrowsingState(
        profile_->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    SnapshotTabHelper::CreateForWebState(&web_state_);
    web_state_.SetContentsMimeType("text/html");

    web::FakeNavigationContext context;
    context.SetUrl(GURL(kExampleUrl));
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(GURL(kExampleUrl));
    web_state_.OnNavigationFinished(&context);

    host->MaybeStartPreClassification(request_type);

    ExpectPreClassificationEvents(request_type);
  }

  // Injects a FORCE_REQUEST real-time URL verdict in VerdictCacheManager.
  void SetForceRequestRTResponseInCacheManager(std::string_view pattern) {
    safe_browsing::VerdictCacheManager* cache_manager =
        VerdictCacheManagerFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(cache_manager);
    safe_browsing::RTLookupResponse response;
    safe_browsing::RTLookupResponse::ThreatInfo* threat_info =
        response.add_threat_info();
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::SUSPICIOUS);
    threat_info->set_cache_expression_using_match_type(pattern);
    threat_info->set_cache_duration_sec(kCacheDurationSec);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED);
    threat_info->set_cache_expression_match_type(
        safe_browsing::RTLookupResponse::ThreatInfo::EXACT_MATCH);
    response.set_client_side_detection_type(
        safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
    cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  }

  // Simulates the completion of an asynchronous Safe Browsing real-time check
  // for `url`.
  void SimulateAsyncSafeBrowsingCheckFinished(ClientSideDetectionHostIOS* host,
                                              const GURL& url) {
    SafeBrowsingQueryManager::Query query(url, "GET");
    SafeBrowsingQueryManager::Result result;
    SafeBrowsingQueryManager::QueryData query_data(
        nullptr, query, QueryType::kAsync, result,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck::
            kUrlRealTimeCheck);
    host->SafeBrowsingAsyncQueryFinished(query_data);
  }

  // Simulates the completion of an asynchronous Safe Browsing check and asserts
  // force request status and histograms.
  void TestAsyncSafeBrowsingCheck(
      const GURL& query_url,
      std::optional<
          ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult>
          expected_result,
      bool expect_force_request) {
    safe_browsing::SetSafeBrowsingState(
        profile_->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    SnapshotTabHelper::CreateForWebState(&web_state_);
    web_state_.SetContentsMimeType("text/html");

    GURL main_url(kExampleUrl);
    web::FakeNavigationContext context;
    context.SetUrl(main_url);
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(main_url);
    web_state_.OnNavigationFinished(&context);

    SimulateAsyncSafeBrowsingCheckFinished(host.get(), query_url);

    if (expected_result.has_value()) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.ClientSideDetection."
          "AsyncCheckTriggerForceRequestResult",
          *expected_result, 1);
    } else {
      histogram_tester_.ExpectTotalCount("SBClientPhishing.ClientSideDetection."
                                         "AsyncCheckTriggerForceRequestResult",
                                         0);
    }
    EXPECT_EQ(should_send_as_force_request(host.get()), expect_force_request);
    if (expect_force_request) {
      histogram_tester_.ExpectUniqueSample(
          "SBClientPhishing.PreClassificationCheckResult.ForceRequest",
          safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
      EXPECT_EQ(last_request_type(host.get()),
                safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
    }
  }

  // Configures Safe Browsing, creates tab helpers, and returns a host for
  // redirect tests.
  std::unique_ptr<ClientSideDetectionHostIOS> SetUpHostWithSafeBrowsing(
      ::FakeSafeBrowsingClient& client) {
    safe_browsing::SetSafeBrowsingState(
        profile_->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

    static_cast<FakeSafeBrowsingService*>(client.GetSafeBrowsingService())
        ->SetDatabaseManager(database_manager_);
    SafeBrowsingQueryManager::CreateForWebState(&web_state_, &client);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_, &client);

    SnapshotTabHelper::CreateForWebState(&web_state_);
    web_state_.SetContentsMimeType("text/html");

    return CreateHost();
  }

  // Simulates a navigation through a chain of redirected URLs ending with the
  // final committed URL.
  void SimulateRedirectChain(::FakeSafeBrowsingClient& client,
                             base::span<const GURL> url_chain) {
    ASSERT_FALSE(url_chain.empty());
    for (size_t i = 0; i < url_chain.size(); ++i) {
      web_state_.ShouldAllowRequest(
          [NSURLRequest requestWithURL:net::NSURLWithGURL(url_chain[i])],
          web::WebStatePolicyDecider::RequestInfo(
              ui::PageTransition::PAGE_TRANSITION_LINK,
              /*target_frame_is_main=*/true,
              /*target_frame_is_cross_origin=*/false,
              /*target_window_is_cross_origin=*/false,
              /*is_user_initiated=*/false, /*user_tapped_recently=*/false),
          base::DoNothing());
      client.run_sync_callbacks();

      if (i > 0) {
        web::FakeNavigationContext redirect_context;
        web_state_.OnNavigationRedirected(&redirect_context);
      }
    }

    const GURL& final_url = url_chain.back();
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(final_url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    web_state_.ShouldAllowResponse(
        response,
        web::WebStatePolicyDecider::ResponseInfo(/*for_main_frame=*/true),
        base::DoNothing());
    client.run_sync_callbacks();

    web::FakeNavigationContext commit_context;
    commit_context.SetUrl(final_url);
    commit_context.SetHasCommitted(true);
    commit_context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(final_url);
    web_state_.OnNavigationFinished(&commit_context);
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager_;
  web::FakeWebState web_state_;
  FakeOptimizationGuideModelProvider test_opt_guide_;
  MockClientSideDetectionService mock_service_;
  base::HistogramTester histogram_tester_;
};

// Tests that GetFeatureCache() creates the feature cache on-demand when it does
// not already exist on the WebState.
TEST_F(ClientSideDetectionHostIOSTest, GetFeatureCacheCreatesOnDemand) {
  EXPECT_FALSE(ClientSideDetectionFeatureCache::FromWebState(&web_state_));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  safe_browsing::ClientSideDetectionFeatureCacheBase* cache =
      host->GetFeatureCache();
  ASSERT_TRUE(cache);
  EXPECT_EQ(ClientSideDetectionFeatureCache::FromWebState(&web_state_), cache);
}

// Tests that GetFeatureCache() returns nullptr when the WebState is destroyed.
TEST_F(ClientSideDetectionHostIOSTest, GetFeatureCacheNullWebState) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->WebStateDestroyed(&web_state_);
  EXPECT_EQ(host->GetFeatureCache(), nullptr);
}

// Tests that GetRedirectChain() returns an empty vector when
// SafeBrowsingTabHelper is not attached to the WebState.
TEST_F(ClientSideDetectionHostIOSTest,
       GetRedirectChainNoSafeBrowsingTabHelper) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  EXPECT_TRUE(host->GetRedirectChain().empty());
}

// Tests that GetRedirectChain() returns an empty vector when the WebState is
// destroyed.
TEST_F(ClientSideDetectionHostIOSTest, GetRedirectChainNullWebState) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->WebStateDestroyed(&web_state_);
  EXPECT_TRUE(host->GetRedirectChain().empty());
}

// Tests that GetRedirectChain() returns the redirect chain from
// SafeBrowsingTabHelper.
TEST_F(ClientSideDetectionHostIOSTest,
       GetRedirectChainFromSafeBrowsingTabHelper) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kEmpty);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  safe_browsing::ClientSideDetectionHostBase* csd_host =
      sb_tab_helper->client_side_detection_host();
  ASSERT_TRUE(csd_host);

  EXPECT_EQ(csd_host->GetRedirectChain(), sb_tab_helper->GetRedirectChain());
}

// Tests that ClientSideDetectionHostIOS is created via SafeBrowsingTabHelper
// when the feature is enabled and there is no filter.
TEST_F(ClientSideDetectionHostIOSTest, HostCreatedWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kEmpty);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_TRUE(sb_tab_helper->client_side_detection_host());
}

// Tests that ClientSideDetectionHostIOS is not created when the feature is
// disabled.
TEST_F(ClientSideDetectionHostIOSTest, DisabledFeaturePreventsHostCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kEmpty);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_FALSE(sb_tab_helper->client_side_detection_host());
}

// Tests that ClientSideDetectionHostIOS is not created for prerender WebStates.
TEST_F(ClientSideDetectionHostIOSTest, PrerenderFilterPreventsHostCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kPrerender);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_FALSE(sb_tab_helper->client_side_detection_host());
}

// Tests that ClientSideDetectionHostIOS is not created for lens overlay
// WebStates.
TEST_F(ClientSideDetectionHostIOSTest, LensOverlayFilterPreventsHostCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kLensOverlay);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_FALSE(sb_tab_helper->client_side_detection_host());
}

// Tests that ClientSideDetectionHostIOS is not created for incognito
// (off-the-record) WebStates.
TEST_F(ClientSideDetectionHostIOSTest, OffTheRecordPreventsHostCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  // Set up an incognito profile.
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      ClientSideDetectionServiceFactory::GetInstance(),
      base::BindRepeating(&BuildMockClientSideDetectionService));
  std::unique_ptr<TestProfileIOS> original_profile = std::move(builder).Build();
  ProfileIOS* otr_profile = original_profile->GetOffTheRecordProfile();

  web::FakeWebState otr_web_state;
  otr_web_state.SetBrowserState(otr_profile);
  otr_web_state.SetWebFramesManager(
      web::ContentWorld::kPageContentWorld,
      std::make_unique<web::FakeWebFramesManager>());
  otr_web_state.SetWebFramesManager(
      web::ContentWorld::kIsolatedWorld,
      std::make_unique<web::FakeWebFramesManager>());
  otr_web_state.SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());

  AttachTabHelpers(&otr_web_state, TabHelperFilter::kEmpty);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&otr_web_state);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_FALSE(sb_tab_helper->client_side_detection_host());
}

// Tests that toggling Safe Browsing pref dynamically creates and destroys the
// ClientSideDetectionHostIOS.
TEST_F(ClientSideDetectionHostIOSTest,
       SafeBrowsingPrefToggleDestroysAndRecreatesHost) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionEnabledIos);

  AttachTabHelpers(&web_state_, TabHelperFilter::kEmpty);
  SafeBrowsingTabHelper* sb_tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(sb_tab_helper);
  EXPECT_TRUE(sb_tab_helper->client_side_detection_host());

  // Disable Safe Browsing -> host should be destroyed.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       SafeBrowsingState::NO_SAFE_BROWSING);
  EXPECT_FALSE(sb_tab_helper->client_side_detection_host());

  // Re-enable Safe Browsing (Standard Protection) -> host should be re-created.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  EXPECT_TRUE(sb_tab_helper->client_side_detection_host());

  // Enhanced Protection -> host remains active.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_TRUE(sb_tab_helper->client_side_detection_host());
}

// Tests that snapshot failure prevents visual classification and logs
// kSnapshotFailed.
TEST_F(ClientSideDetectionHostIOSTest, SnapshotFailedPreventsClassification) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  // Create SnapshotTabHelper but don't set a delegate or view, so snapshot
  // fails.
  SnapshotTabHelper::CreateForWebState(&web_state_);

  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.SetContentsMimeType("text/html");

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .Times(0);

  // Trigger page load.
  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::SUCCESS);

  // Fast-forward mock time to trigger the stabilization delay.
  task_environment_.FastForwardBy(base::Milliseconds(750));

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kImageClassificationBegin, 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.iOS.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kSnapshotFailed, 1);
  EXPECT_FALSE(is_csd_running(host.get()));
}

// Tests that missing SnapshotTabHelper prevents visual classification and logs
// kSnapshotHelperMissing.
TEST_F(ClientSideDetectionHostIOSTest,
       SnapshotHelperMissingPreventsClassification) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.SetContentsMimeType("text/html");

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .Times(0);

  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::SUCCESS);

  // Fast-forward mock time to trigger the stabilization delay.
  task_environment_.FastForwardBy(base::Milliseconds(750));

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kImageClassificationBegin, 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.iOS.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kSnapshotHelperMissing, 1);
  EXPECT_FALSE(is_csd_running(host.get()));
}

TEST_F(ClientSideDetectionHostIOSTest,
       OnSnapshotReceivedLogsImageClassificationBegin) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  UIImage* test_image = CreateTestImage();

  OnSnapshotReceived(host.get(), GURL(kExampleUrl), test_image);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
}

// Tests that `OnSnapshotReceived` early-returns when the scorer is missing.
TEST_F(ClientSideDetectionHostIOSTest,
       NoScorerAfterSnapshotPreventsClassification) {
  mock_service_.SetScorerForTesting(nullptr);
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  UIImage* test_image = CreateTestImage();

  OnSnapshotReceived(host.get(), GURL(kExampleUrl), test_image);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
      /*expected_count=*/0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.iOS.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot, 1);
  EXPECT_FALSE(is_csd_running(host.get()));
}

TEST_F(ClientSideDetectionHostIOSTest,
       NoScorerBeforeSnapshotPreventsClassification) {
  mock_service_.SetScorerForTesting(nullptr);
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  // Create SnapshotTabHelper.
  SnapshotTabHelper::CreateForWebState(&web_state_);

  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.SetContentsMimeType("text/html");

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .Times(0);

  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::SUCCESS);

  // Fast-forward mock time to trigger the stabilization delay.
  task_environment_.FastForwardBy(base::Milliseconds(750));

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kImageClassificationBegin, 0);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.iOS.VisualClassificationEarlyReturnReason",
      VisualClassificationEarlyReturnReason::kScorerMissingBeforeSnapshot, 1);
  EXPECT_FALSE(is_csd_running(host.get()));
}

TEST_F(ClientSideDetectionHostIOSTest, NavigationErrorNoClassification) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .Times(0);

  // Trigger page load with failure.
  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::FAILURE);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult", 0);
}

// Tests that if ClassifyPhishingThroughThresholds sets is_phishing for
// TRIGGER_MODELS, a client phishing report will be dispatched.
TEST_F(ClientSideDetectionHostIOSTest,
       OnVisualClassificationDoneTriggersReport) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  GURL url("https://example.com");
  std::vector<double> scores = {0.9, 0.8};

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([](safe_browsing::ClientPhishingRequest* verdict) {
        verdict->set_is_phishing(true);
        verdict->set_client_side_detection_type(
            safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
      });

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  host->OnVisualClassificationDoneForTesting(url, scores);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kImageClassificationComplete, 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kVerdictProtoParseComplete, 1);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.LocalModelDetectsPhishing.TriggerModel", true, 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kLocalModelResultComplete, 1);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kMiscellaneousFieldsAdded, 1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kNetworkRequestSent, 1);
}

// Tests that sample pings are correctly tagged with SAMPLE_REPORT type when
// send_sample_ping is enabled.
TEST_F(ClientSideDetectionHostIOSTest, SamplePingPropagation) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  set_send_sample_ping(host.get(), true);

  GURL url("https://example.com");
  std::vector<double> scores = {0.9, 0.8};

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([](safe_browsing::ClientPhishingRequest* verdict) {
        verdict->set_is_phishing(false);
        verdict->set_client_side_detection_type(
            safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
      });

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
             safe_browsing::ClientSideDetectionServiceBase::
                 ClientReportPhishingRequestCallback callback,
             const std::string& access_token) {
            EXPECT_EQ(verdict->report_type(),
                      safe_browsing::ClientPhishingRequest::SAMPLE_REPORT);
          });

  host->OnVisualClassificationDoneForTesting(url, scores);
}

// Tests that a FORCE_REQUEST real-time lookup verdict triggers a phishing
// report even when local visual classification scores evaluate to non-phishing.
TEST_F(ClientSideDetectionHostIOSTest,
       OnVisualClassificationDoneTriggersReportForcedByRealTimeVerdict) {
  auto host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto scorer = std::make_unique<safe_browsing::Scorer>();
  mock_service_.SetScorerForTesting(std::move(scorer));

  GURL url(kExampleUrl);
  web_state_.SetCurrentURL(url);

  SetForceRequestRTResponseInCacheManager(kExampleUrlPattern);

  std::vector<double> scores = {0.1, 0.2};

  // Model classification evaluates to non-phishing.
  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([](safe_browsing::ClientPhishingRequest* verdict) {
        verdict->set_is_phishing(false);
      });

  // Because classification was forced by real-time lookup, a report must still
  // be sent.
  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  host->OnVisualClassificationDoneForTesting(url, scores);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.LocalModelDetectsPhishing.TriggerModel", false, 1);
}

TEST_F(ClientSideDetectionHostIOSTest, CacheHitPreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock_service_,
              GetValidCachedResult(GURL(kExampleUrl), testing::_))
      .WillOnce([](const GURL& url, bool* is_phishing) {
        *is_phishing = true;
        return true;
      });

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_RESULT_FROM_CACHE,
      1);
}

TEST_F(ClientSideDetectionHostIOSTest,
       LocalIPAndIntranetPreventClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);

  context.SetUrl(GURL(kLocalhostUrl));
  web_state_.SetCurrentURL(GURL(kLocalhostUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE,
      1);

  context.SetUrl(GURL(kIntranetUrl));
  web_state_.SetCurrentURL(GURL(kIntranetUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE,
      2);

  EXPECT_CALL(mock_service_,
              IsPrivateIPAddress(testing::Property(
                  &net::IPAddress::ToString, testing::Eq(kLoopbackIpStr))))
      .WillOnce(testing::Return(true));

  context.SetUrl(GURL(kLoopbackIpUrl));
  web_state_.SetCurrentURL(GURL(kLoopbackIpUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP, 1);

  EXPECT_CALL(mock_service_,
              IsPrivateIPAddress(testing::Property(&net::IPAddress::ToString,
                                                   testing::Eq(kPrivateIpStr))))
      .WillOnce(testing::Return(true));

  context.SetUrl(GURL(kPrivateIpUrl));
  web_state_.SetCurrentURL(GURL(kPrivateIpUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP, 2);
}

TEST_F(ClientSideDetectionHostIOSTest, SuccessfulGatingLogsClassify) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  EXPECT_EQ(web_state_.GetContentsMimeType(), "text/html");

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
}

TEST_F(ClientSideDetectionHostIOSTest, ReportLimitPreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock_service_, GetValidCachedResult(testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(mock_service_, AtPhishingReportLimit())
      .WillOnce(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_TOO_MANY_REPORTS,
      1);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.RequestTypeAtReportLimit",
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_TOO_MANY_REPORTS);
}

TEST_F(ClientSideDetectionHostIOSTest, CsdAllowlistPreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_MATCH_CSD_ALLOWLIST,
      1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_MATCH_CSD_ALLOWLIST);
}

// Tests that if the URL matches the CSD allowlist, classification still
// continues when the request type is `FORCE_REQUEST`.
TEST_F(ClientSideDetectionHostIOSTest, CsdAllowlistSkippedForForceRequest) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(false);

  ON_CALL(mock_service_, AtPhishingReportLimit())
      .WillByDefault(testing::Return(false));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            safe_browsing::PreClassificationCheckResult::CLASSIFY);
}

// Tests that if the URL matches the CSD allowlist, classification still
// continues when the request type is `USER_REPORT`.
TEST_F(ClientSideDetectionHostIOSTest, CsdAllowlistSkippedForUserReport) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(false);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::USER_REPORT);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::USER_REPORT);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            safe_browsing::PreClassificationCheckResult::CLASSIFY);
}

// Tests that if the URL matches the CSD allowlist, classification still
// continues when the `kSkipCSDAllowlistOnPreclassification` flag is set.
TEST_F(ClientSideDetectionHostIOSTest, CsdAllowlistSkippedWhenFlagPresent) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      safe_browsing::switches::kSkipCSDAllowlistOnPreclassification);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(false);

  ON_CALL(mock_service_, GetValidCachedResult(testing::_, testing::_))
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_service_, AtPhishingReportLimit())
      .WillByDefault(testing::Return(false));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            safe_browsing::PreClassificationCheckResult::CLASSIFY);
}

// Test that classification is stopped when the database manager is null,
// even for `FORCE_REQUEST`.
TEST_F(ClientSideDetectionHostIOSTest,
       NoDatabaseManagerPreventsClassificationForForceRequest) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  FakeSafeBrowsingService* sb_service = static_cast<FakeSafeBrowsingService*>(
      GetApplicationContext()->GetSafeBrowsingService());
  sb_service->SetDatabaseManager(nullptr);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_NO_DATABASE_MANAGER,
      1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            safe_browsing::PreClassificationCheckResult::
                NO_CLASSIFY_NO_DATABASE_MANAGER);
}

// Test that if the URL matches the High Confidence allowlist, classification
// is prevented even when the `kSkipCSDAllowlistOnPreclassification` flag is
// set.
// TODO(crbug.com/556065493): All allowlists should be skipped when the
// `kSkipCSDAllowlistOnPreclassification` flag is set, not just the CSD
// allowlist.
TEST_F(ClientSideDetectionHostIOSTest,
       HcAllowlistPreventsClassificationWhenSkipCsdFlagPresent) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      safe_browsing::switches::kSkipCSDAllowlistOnPreclassification);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(true);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_MATCH_HC_ALLOWLIST,
      1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            safe_browsing::PreClassificationCheckResult::
                NO_CLASSIFY_MATCH_HC_ALLOWLIST);
}

// Tests that if the URL matches the CSD allowlist, classification still
// continues when send_sample_ping_ is true.
TEST_F(ClientSideDetectionHostIOSTest,
       CsdAllowlistMatchWithSamplePingContinuesClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(false);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, GetValidCachedResult(testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_service_, AtPhishingReportLimit())
      .WillRepeatedly(testing::Return(false));
  mock_service_.SetScorerForTesting(std::make_unique<safe_browsing::Scorer>());

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_sample_ping_rate_for_testing(1.0);

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::CLASSIFY);
}

TEST_F(ClientSideDetectionHostIOSTest,
       HighConfidenceAllowlistPreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchHcAllowlist(true);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_MATCH_HC_ALLOWLIST,
      1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST);
}

TEST_F(ClientSideDetectionHostIOSTest,
       AllowlistChecksPassedContinuesClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(false);
  database_manager_->SetMatchHcAllowlist(false);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, GetValidCachedResult(testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_service_, AtPhishingReportLimit())
      .WillRepeatedly(testing::Return(false));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL(kExampleUrl));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::CLASSIFY);
}

// Tests that Chrome UI URLs (chrome://) stop classification before other
// pre-classification checks.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationChromeUISchemePreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("chrome://version"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("chrome://version"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_CHROME_UI_PAGE,
      1);

  ClientSideDetectionFeatureCache* feature_cache =
      ClientSideDetectionFeatureCache::FromWebState(&web_state_);
  ASSERT_TRUE(feature_cache);
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_cache->GetDebuggingMetadataForURL(GURL("chrome://version"));
  ASSERT_TRUE(debugging_metadata);
  EXPECT_EQ(debugging_metadata->preclassification_check_result(),
            PreClassificationCheckResult::NO_CLASSIFY_CHROME_UI_PAGE);
}

// Tests that local resources (file://) are caught before unsupported
// scheme checks when kClientSideDetectionLocalResourceCheckFix is enabled.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationLocalResourcePreventsClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionLocalResourceCheckFix);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("file:///path/to/page.html"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("file:///path/to/page.html"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE,
      1);
}

// Tests that error documents are caught before unsupported MIME type
// checks when kClientSideDetectionSkipErrorPage is enabled.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationErrorDocumentPreventsClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionSkipErrorPage);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("application/pdf");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/error"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  context.SetError([NSError errorWithDomain:@"test" code:-1 userInfo:nil]);
  web_state_.SetCurrentURL(GURL("https://example.com/error"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_ERROR_DOCUMENT,
      1);
}

// Tests that unsupported MIME types are caught before private IP checks.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationUnsupportedMimeTypePreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("application/pdf");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kPrivateIpUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kPrivateIpUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_UNSUPPORTED_MIME_TYPE,
      1);
}

// Tests that unsupported schemes (e.g. ftp://) are caught.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationUnsupportedSchemePreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("ftp://example.com/test.html"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("ftp://example.com/test.html"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_SCHEME_NOT_SUPPORTED,
      1);
}

// Tests that ShowBlockingPage does NOT store an unsafe resource and does NOT
// trigger reload when kCsdEnforceIos is disabled (default).
TEST_F(ClientSideDetectionHostIOSTest,
       ShowBlockingPageEnforcementDisabledByDefault) {
  SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  GURL phishing_url(kPhishingUrl);
  host->ShowBlockingPage(phishing_url,
                         safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
                         /*intelligent_scan_verdict=*/std::nullopt,
                         /*should_show_scam_warning=*/false);

  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  ASSERT_TRUE(container);
  EXPECT_EQ(container->GetMainFrameUnsafeResource(), nullptr);

  web::FakeNavigationManager* nav_manager =
      static_cast<web::FakeNavigationManager*>(
          web_state_.GetNavigationManager());
  EXPECT_FALSE(nav_manager->ReloadWasCalled());
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kWarningShown, 0);
}

// Tests that ShowBlockingPage stores an unsafe resource and triggers reload
// when kCsdEnforceIos is enabled.
TEST_F(ClientSideDetectionHostIOSTest, ShowBlockingPageEnforcementEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionEnabledIos,
      {{"CsdEnforceIos", "true"}});

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  GURL phishing_url(kPhishingUrl);
  host->ShowBlockingPage(phishing_url,
                         safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
                         /*intelligent_scan_verdict=*/std::nullopt,
                         /*should_show_scam_warning=*/false);

  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  ASSERT_TRUE(container);
  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource->url, phishing_url);
  EXPECT_EQ(resource->original_url, phishing_url);
  EXPECT_EQ(resource->navigation_url, phishing_url);
  EXPECT_EQ(
      resource->threat_type,
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
  EXPECT_EQ(resource->threat_source,
            safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION);

  web::FakeNavigationManager* nav_manager =
      static_cast<web::FakeNavigationManager*>(
          web_state_.GetNavigationManager());
  EXPECT_TRUE(nav_manager->ReloadWasCalled());

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kWarningShown, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetectionEvent.TriggerModel",
      ClientSideDetectionEvent::kWarningShown, 1);
}

// Tests that MaybeStartPreClassification returns early without performing any
// checks when kClientSideDetectionKillswitch is enabled.
TEST_F(ClientSideDetectionHostIOSTest, PreClassificationKillswitchEarlyReturn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionKillswitch);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/test.html"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/test.html"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult", 0);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionEvent", 0);
}

// Tests that classification is skipped when
// kClientSideDetectionOnlyESBClassification is enabled and the user is in
// standard protection.
TEST_F(ClientSideDetectionHostIOSTest,
       OnlyESBClassificationBlocksStandardProtection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionOnlyESBClassification);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/test.html"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/test.html"));
  web_state_.OnNavigationFinished(&context);

  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::SUCCESS);
  task_environment_.FastForwardBy(base::Milliseconds(750));

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.PreClassificationCheckResult", 0);

  // Directly invoking MaybeStartPreClassification with TRIGGER_MODELS is also
  // blocked.
  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.ClientSideDetectionEvent", 0);
}

// Tests that classification proceeds when
// kClientSideDetectionOnlyESBClassification is enabled and the user has
// enhanced protection enabled.
TEST_F(ClientSideDetectionHostIOSTest,
       OnlyESBClassificationAllowsEnhancedProtection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionOnlyESBClassification);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/test.html"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/test.html"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
}

// Tests that `OnSnapshotReceived` converts `TRIGGER_MODELS` to
// `IMAGE_EMBEDDING_MATCH` when `kClientSideDetectionImageEmbeddingMatch` is
// enabled and the user has enhanced protection enabled.
TEST_F(ClientSideDetectionHostIOSTest,
       OnSnapshotReceivedConvertsToImageEmbeddingMatchWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kClientSideDetectionImageEmbeddingMatch,
       safe_browsing::kClientSideDetectionOnlyESBClassification},
      {});

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  mock_service_.SetScorerForTesting(std::make_unique<safe_browsing::Scorer>());

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  base::test::TestFuture<safe_browsing::ClientSideDetectionType> future;
  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([&](safe_browsing::ClientPhishingRequest* verdict) {
        ASSERT_TRUE(verdict);
        verdict->set_is_phishing(false);
        future.SetValue(verdict->client_side_detection_type());
      });

  UIImage* test_image = CreateTestImage();

  OnSnapshotReceived(host.get(), GURL(kExampleUrl), test_image);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_EQ(future.Get(),
            safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationComplete,
      safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
}

// Tests that `OnSnapshotReceived` does not convert `TRIGGER_MODELS` to
// `IMAGE_EMBEDDING_MATCH` when enhanced protection is disabled.
TEST_F(ClientSideDetectionHostIOSTest,
       OnSnapshotReceivedRemainsTriggerModelsWhenStandardProtection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kClientSideDetectionImageEmbeddingMatch,
       safe_browsing::kClientSideDetectionOnlyESBClassification},
      {});

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  mock_service_.SetScorerForTesting(std::make_unique<safe_browsing::Scorer>());

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  base::test::TestFuture<safe_browsing::ClientSideDetectionType> future;
  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([&](safe_browsing::ClientPhishingRequest* verdict) {
        ASSERT_TRUE(verdict);
        verdict->set_is_phishing(false);
        future.SetValue(verdict->client_side_detection_type());
      });

  UIImage* test_image = CreateTestImage();

  OnSnapshotReceived(host.get(), GURL(kExampleUrl), test_image);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_EQ(future.Get(),
            safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationComplete,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
}

// Tests that cache hit is restricted to TRIGGER_MODELS and does not suppress
// other request types such as CREDIT_CARD_FORM.
TEST_F(ClientSideDetectionHostIOSTest,
       CacheHitRestrictedToTriggerModelsAndSkippedForOtherTriggers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionCreditCardForm,
      {{safe_browsing::kCsdCreditCardFormSampleRate.name, "1.0"}});

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(mock_service_,
              GetValidCachedResult(GURL(kExampleUrl), testing::_))
      .WillRepeatedly([](const GURL& url, bool* is_phishing) {
        *is_phishing = true;
        return true;
      });

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  // CREDIT_CARD_FORM should ignore cache and continue pre-classification to
  // CLASSIFY.
  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
}

// Tests that navigation resets host state, cancels pending classification, and
// emits InterruptedByNavigation metric if classification was running.
TEST_F(ClientSideDetectionHostIOSTest,
       NavigationResetsStateAndEmitsInterruptedMetric) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/page1"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/page1"));
  web_state_.OnNavigationFinished(&context);

  // Simulate CSD running for TRIGGER_MODELS.
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_is_csd_running(host.get(), true);
  set_should_send_as_force_request(host.get(), true);

  EXPECT_TRUE(is_csd_running(host.get()));

  // Navigate to Page 2 while CSD was running.
  web::FakeNavigationContext context2;
  context2.SetUrl(GURL("https://example.com/page2"));
  context2.SetHasCommitted(true);
  context2.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/page2"));
  web_state_.OnNavigationFinished(&context2);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection.InterruptedByNavigation",
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS, 1);

  EXPECT_FALSE(is_csd_running(host.get()));
  EXPECT_FALSE(is_classifying(host.get()));
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::
                CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);
  EXPECT_FALSE(should_send_as_force_request(host.get()));
  EXPECT_FALSE(trigger_model_request_sent_as_force_request(host.get()));
}

// Tests tier checking: higher priority requests cancel lower priority requests,
// and lower priority requests are blocked when higher priority requests are
// running.
TEST_F(ClientSideDetectionHostIOSTest, TierPrioritizationAndCancellation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kClientSideDetectionTierSystem);

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  // Simulate TRIGGER_MODELS (tier 3) running.
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_is_csd_running(host.get(), true);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  // Start FORCE_REQUEST (tier 1 - higher priority). Should preempt
  // TRIGGER_MODELS.
  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

  // Simulate FORCE_REQUEST running.
  set_is_csd_running(host.get(), true);

  // Attempt TRIGGER_MODELS (tier 3 - lower priority) while FORCE_REQUEST is
  // running. Should be blocked and emit BlockingRequestType.
  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.BlockingRequestType.TriggerModel",
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST, 1);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
}

// Tests that duration metrics use tick_clock() instead of wall clock.
TEST_F(ClientSideDetectionHostIOSTest, TickClockUsageForTimingMetrics) {
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_tick_clock_for_testing(&test_clock);

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  test_clock.Advance(base::Milliseconds(50));

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.HighConfidenceAllowlistCheckDuration", 1);
}

// Tests that referrer chain is added only when Enhanced Protection is enabled,
// and omitted when Standard Protection is enabled.
TEST_F(ClientSideDetectionHostIOSTest, ReferrerChainGatedOnEnhancedProtection) {
  web::FakeNavigationManager* nav_manager =
      static_cast<web::FakeNavigationManager*>(
          web_state_.GetNavigationManager());
  auto item = web::NavigationItem::Create();
  item->SetURL(GURL(kExampleUrl));
  item->SetReferrer(
      web::Referrer(GURL(kReferrerUrl), web::ReferrerPolicyDefault));
  nav_manager->SetLastCommittedItem(item.get());

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  // Enhanced Protection enabled: Referrer chain is added.
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  safe_browsing::ClientPhishingRequest verdict_esb;
  verdict_esb.set_url(kExampleUrl);
  host->AddReferrerChain(&verdict_esb);

  ASSERT_EQ(verdict_esb.referrer_chain_size(), 1);
  EXPECT_EQ(verdict_esb.referrer_chain(0).url(), kExampleUrl);
  EXPECT_EQ(verdict_esb.referrer_chain(0).referrer_url(), kReferrerUrl);

  // Standard Protection: Referrer chain is NOT added.
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  safe_browsing::ClientPhishingRequest verdict_standard;
  verdict_standard.set_url(kExampleUrl);
  host->AddReferrerChain(&verdict_standard);

  EXPECT_EQ(verdict_standard.referrer_chain_size(), 0);
}

// Tests that ShowBlockingPage invokes OnScamWarningShown on the intelligent
// scan delegate when should_show_scam_warning is true.
TEST_F(ClientSideDetectionHostIOSTest,
       ShowBlockingPageNotifiesScamWarningShown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionEnabledIos,
      {{"CsdEnforceIos", "true"}});

  MockIntelligentScanDelegate mock_delegate;
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_intelligent_scan_delegate_for_testing(&mock_delegate);

  EXPECT_CALL(mock_delegate, OnScamWarningShown()).Times(1);

  GURL phishing_url(kPhishingUrl);
  safe_browsing::IntelligentScanVerdict scan_verdict =
      safe_browsing::IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1;

  host->ShowBlockingPage(phishing_url,
                         safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
                         scan_verdict,
                         /*should_show_scam_warning=*/true);

  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  ASSERT_TRUE(container);
  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource->threat_subtype,
            safe_browsing::ThreatSubtype::SCAM_EXPERIMENT_VERDICT_1);
}

// Tests that High Confidence and CSD allowlist telemetry metrics are emitted.
TEST_F(ClientSideDetectionHostIOSTest, AllowlistTelemetryMetricsEmitted) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(true);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.HighConfidenceAllowlistCheckDuration", 1);
  // ClientSideAllowlistMatchResult::kCsdAndHighConfidenceMatch is 3.
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.MatchHighConfidenceAllowlist", 3, 1);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.MatchHighConfidenceAllowlist.TriggerModel", 3, 1);
}

// Tests the different enum outcomes of MatchHighConfidenceAllowlist telemetry.
TEST_F(ClientSideDetectionHostIOSTest,
       MatchHighConfidenceAllowlistAllEnumOutcomes) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, GetValidCachedResult(testing::_, testing::_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_service_, AtPhishingReportLimit())
      .WillRepeatedly(testing::Return(false));

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  // Outcome 1: CSD match only (kCsdMatch = 1).
  {
    base::HistogramTester tester;
    database_manager_->SetMatchCsdAllowlist(true);
    database_manager_->SetMatchHcAllowlist(false);

    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    web::FakeNavigationContext context;
    context.SetUrl(GURL("https://example.com/csd_only"));
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(GURL("https://example.com/csd_only"));
    web_state_.OnNavigationFinished(&context);

    host->MaybeStartPreClassification(
        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

    tester.ExpectUniqueSample("SBClientPhishing.MatchHighConfidenceAllowlist",
                              1, 1);
  }

  // Outcome 2: HC match only (kHighConfidenceMatch = 2).
  {
    base::HistogramTester tester;
    database_manager_->SetMatchCsdAllowlist(false);
    database_manager_->SetMatchHcAllowlist(true);

    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    web::FakeNavigationContext context;
    context.SetUrl(GURL("https://example.com/hc_only"));
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(GURL("https://example.com/hc_only"));
    web_state_.OnNavigationFinished(&context);

    host->MaybeStartPreClassification(
        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

    tester.ExpectUniqueSample("SBClientPhishing.MatchHighConfidenceAllowlist",
                              2, 1);
  }

  // Outcome 3: No match on either allowlist (kNoMatch = 0).
  {
    base::HistogramTester tester;
    database_manager_->SetMatchCsdAllowlist(false);
    database_manager_->SetMatchHcAllowlist(false);

    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    web::FakeNavigationContext context;
    context.SetUrl(GURL("https://example.com/no_match"));
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(GURL("https://example.com/no_match"));
    web_state_.OnNavigationFinished(&context);

    host->MaybeStartPreClassification(
        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

    tester.ExpectUniqueSample("SBClientPhishing.MatchHighConfidenceAllowlist",
                              0, 1);
  }

  // Outcome 4: CSD match with send_sample_ping_ == true (kNoMatch = 0).
  {
    base::HistogramTester tester;
    database_manager_->SetMatchCsdAllowlist(true);
    database_manager_->SetMatchHcAllowlist(false);

    std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
    host->set_sample_ping_rate_for_testing(1.0);

    web::FakeNavigationContext context;
    context.SetUrl(GURL(kExampleUrl));
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.SetCurrentURL(GURL(kExampleUrl));
    web_state_.OnNavigationFinished(&context);

    host->MaybeStartPreClassification(
        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

    tester.ExpectUniqueSample("SBClientPhishing.MatchHighConfidenceAllowlist",
                              0, 1);
  }
}

// Tests that MatchCSDAllowlistOnUnfamiliarLoginPage is emitted for
// UNFAMILIAR_LOGIN_PAGE requests.
TEST_F(ClientSideDetectionHostIOSTest,
       AllowlistTelemetryEmittedOnUnfamiliarLoginPage) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  database_manager_->SetMatchCsdAllowlist(true);
  database_manager_->SetMatchHcAllowlist(false);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/login"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/login"));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.MatchCSDAllowlistOnUnfamiliarLoginPage", true, 1);
}

// Tests that load-time force request promotes TRIGGER_MODELS to FORCE_REQUEST
// and emits TriggerModelsConvertedToForceRequestAtLoad.
TEST_F(ClientSideDetectionHostIOSTest,
       LoadTimeForceRequestPromotesTriggerModelsAndEmitsMetric) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, IsModelAvailable())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/page"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL("https://example.com/page"));
  web_state_.OnNavigationFinished(&context);

  // Simulate an async check arriving and marking this page as a force request.
  set_should_send_as_force_request(host.get(), true);

  // Trigger page load and advance timer to fire stabilization delay.
  host->PageLoaded(&web_state_, web::PageLoadCompletionStatus::SUCCESS);
  task_environment_.FastForwardBy(base::Milliseconds(750));

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad", true, 1);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
}

// Tests that classification duration timing is recorded using `tick_clock()`
// by advancing the mock clock between snapshot receipt and classification
// completion.
TEST_F(ClientSideDetectionHostIOSTest,
       ClassificationDurationTimingWithMockClock) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  mock_service_.SetScorerForTesting(std::make_unique<safe_browsing::Scorer>());

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_tick_clock_for_testing(task_environment_.GetMockTickClock());
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  const GURL url(kExampleUrl);

  const base::TimeDelta elapsed = base::Milliseconds(200);
  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .WillOnce([&](safe_browsing::ClientPhishingRequest* verdict) {
        ASSERT_TRUE(verdict);
        verdict->set_is_phishing(false);
        future.SetValue();
      });

  UIImage* test_image = CreateTestImage();
  OnSnapshotReceived(host.get(), url, test_image);

  // Advance mock time and tick clock during async classification.
  task_environment_.AdvanceClock(elapsed);
  EXPECT_TRUE(future.Wait());

  histogram_tester_.ExpectTimeBucketCount(
      "SBClientPhishing.PhishingDetectionDuration", elapsed, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "SBClientPhishing.PhishingDetectionDuration.TriggerModel", elapsed, 1);
}

// Tests that visual extraction failure does not cache the snapshot image.
TEST_F(ClientSideDetectionHostIOSTest,
       OnClassificationDoneVisualExtractionFailedPreventsImageCaching) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  const GURL url(kExampleUrl);
  gfx::Image test_image = gfx::Image(CreateTestImage());
  safe_browsing::ClientPhishingRequest verdict;

  EXPECT_CALL(mock_service_, ClassifyPhishingThroughThresholds(testing::_))
      .Times(0);

  OnClassificationDone(
      host.get(), url, test_image,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
      /*classification_start_time=*/base::TimeTicks::Now(), verdict,
      safe_browsing::PhishingClassifier::Result::kVisualExtractionFailed);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that navigating while visual classification is in flight cancels
// pending requests and clears classification state.
TEST_F(ClientSideDetectionHostIOSTest, ClassificationCancelledOnNavigation) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_is_classifying(host.get(), true);
  set_is_csd_running(host.get(), true);

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  EXPECT_FALSE(is_classifying(host.get()));
  EXPECT_FALSE(is_csd_running(host.get()));
  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that FORCE_REQUEST triggers skip image classification scoring and
// immediately dispatch a report when
// `kSkipImageClassificationScoringForNonPageLoadTriggers` is enabled.
TEST_F(ClientSideDetectionHostIOSTest,
       ForceRequestSkipsImageClassificationScoring) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kSkipImageClassificationScoringForNonPageLoadTriggers},
      {});

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  database_manager_->SetMatchCsdAllowlist(false);
  database_manager_->SetMatchHcAllowlist(false);

  auto host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<safe_browsing::ClientPhishingRequest> request,
             safe_browsing::ClientSideDetectionServiceBase::
                 ClientReportPhishingRequestCallback callback,
             const std::string& access_token) {
            ASSERT_TRUE(request);
            EXPECT_EQ(request->client_side_detection_type(),
                      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
            EXPECT_DOUBLE_EQ(request->client_score(), 0.0);
          });

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

  EXPECT_FALSE(is_csd_running(host.get()));
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.PhishingDetectorResult.ForceRequest",
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SKIPPED, 1);
}

// Tests that CLIPBOARD_COPY_API pre-classification stops when the sample rate
// is 0.0.
TEST_F(ClientSideDetectionHostIOSTest,
       ClipboardCopyApiSamplingStopsPreClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionClipboardCopyApi,
      {{safe_browsing::kCsdClipboardCopyApiSampleRate.name, "0.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC,
      1);
}

// Tests that CLIPBOARD_COPY_API pre-classification proceeds to CLASSIFY when
// the sample rate is 1.0.
TEST_F(ClientSideDetectionHostIOSTest,
       ClipboardCopyApiSamplingAllowsClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionClipboardCopyApi,
      {{safe_browsing::kCsdClipboardCopyApiSampleRate.name, "1.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.ClipboardCopyApi",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
}

// Tests that CREDIT_CARD_FORM pre-classification stops when the sample rate
// is 0.0.
TEST_F(ClientSideDetectionHostIOSTest,
       CreditCardFormSamplingStopsPreClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionCreditCardForm,
      {{safe_browsing::kCsdCreditCardFormSampleRate.name, "0.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC,
      1);
}

// Tests that CREDIT_CARD_FORM pre-classification proceeds to CLASSIFY when
// the sample rate is 1.0.
TEST_F(ClientSideDetectionHostIOSTest,
       CreditCardFormSamplingAllowsClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionCreditCardForm,
      {{safe_browsing::kCsdCreditCardFormSampleRate.name, "1.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.CreditCardForm",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
}

// Tests that UNFAMILIAR_LOGIN_PAGE pre-classification stops when the sample
// rate is 0.0.
TEST_F(ClientSideDetectionHostIOSTest,
       UnfamiliarLoginPageSamplingStopsPreClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kProactivePasswordProtection,
      {{safe_browsing::kCsdProactivePasswordProtectionSampleRate.name, "0.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.UnfamiliarLoginPage",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC,
      1);
}

// Tests that UNFAMILIAR_LOGIN_PAGE pre-classification proceeds to CLASSIFY when
// the sample rate is 1.0.
TEST_F(ClientSideDetectionHostIOSTest,
       UnfamiliarLoginPageSamplingAllowsClassification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kProactivePasswordProtection,
      {{safe_browsing::kCsdProactivePasswordProtectionSampleRate.name, "1.0"}});

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.UnfamiliarLoginPage",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
}

// Tests that pre-classification stops if the profile is off-the-record.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationOffTheRecordPreventsClassification) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  web_state_.SetBrowserState(otr_profile);

  safe_browsing::SetSafeBrowsingState(
      otr_profile->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kExampleUrl));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(GURL(kExampleUrl));
  web_state_.OnNavigationFinished(&context);

  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  ExpectPreClassificationEvents(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::NO_CLASSIFY_OFF_THE_RECORD,
      1);
}

// Tests that pre-classification stops if the URL is allowlisted by policy.
TEST_F(ClientSideDetectionHostIOSTest,
       PreClassificationAllowlistedByPolicyPreventsClassification) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  base::ListValue allowlist;
  allowlist.Append("example.com");
  profile_->GetPrefs()->SetList(prefs::kSafeBrowsingAllowlistDomains,
                                std::move(allowlist));

  RunSamplingPreClassificationTest(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult",
      safe_browsing::PreClassificationCheckResult::
          NO_CLASSIFY_ALLOWLISTED_BY_POLICY,
      1);
}

// Tests that ShowBlockingPage handles should_show_scam_warning=true with a null
// intelligent_scan_verdict safely without crashing or dereferencing nullopt.
TEST_F(ClientSideDetectionHostIOSTest,
       ShowBlockingPageWithScamWarningAndNullVerdictDoesNotCrash) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kClientSideDetectionEnabledIos,
      {{"CsdEnforceIos", "true"}});

  MockIntelligentScanDelegate mock_delegate;
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_intelligent_scan_delegate_for_testing(&mock_delegate);

  EXPECT_CALL(mock_delegate, OnScamWarningShown()).Times(0);

  GURL phishing_url(kPhishingUrl);
  host->ShowBlockingPage(phishing_url,
                         safe_browsing::ClientSideDetectionType::TRIGGER_MODELS,
                         /*intelligent_scan_verdict=*/std::nullopt,
                         /*should_show_scam_warning=*/true);

  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  ASSERT_TRUE(container);
  const security_interstitials::UnsafeResource* resource =
      container->GetMainFrameUnsafeResource();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource->url, phishing_url);
  EXPECT_EQ(resource->original_url, phishing_url);
  EXPECT_EQ(resource->navigation_url, phishing_url);
  EXPECT_EQ(
      resource->threat_type,
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
  EXPECT_EQ(resource->threat_source,
            safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION);
  // Threat subtype defaults to UNKNOWN because intelligent_scan_verdict is
  // nullopt.
  EXPECT_EQ(resource->threat_subtype, safe_browsing::ThreatSubtype::UNKNOWN);

  web::FakeNavigationManager* nav_manager =
      static_cast<web::FakeNavigationManager*>(
          web_state_.GetNavigationManager());
  EXPECT_TRUE(nav_manager->ReloadWasCalled());
}

// Tests that DetermineVisualFeaturesExtraction emits Viewport metrics and
// returns the visual extraction result when a valid view is present, and
// emits metrics when view is null.
TEST_F(ClientSideDetectionHostIOSTest,
       DetermineVisualFeaturesExtractionEmitsViewportMetrics) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();

  // With no view attached, viewport dimensions are -1 and result is
  // kBelowMinFrame.
  EXPECT_EQ(DetermineVisualFeaturesExtraction(host.get()),
            visual_utils::CanExtractVisualFeaturesResult::kBelowMinFrame);
  histogram_tester_.ExpectBucketCount("SBClientPhishing.Viewport.Width", -1, 1);
  histogram_tester_.ExpectBucketCount("SBClientPhishing.Viewport.Height", -1,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.VisualFeaturesClearReason2",
      visual_utils::CanExtractVisualFeaturesResult::kBelowMinFrame, 1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.Viewport.EncodedResolution", 0);

  // With a valid view attached, dimensions are extracted and visual features
  // can be extracted.
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)];
  web_state_.SetView(view);

  visual_utils::CanExtractVisualFeaturesResult result =
      DetermineVisualFeaturesExtraction(host.get());
  EXPECT_EQ(
      result,
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures);

  histogram_tester_.ExpectBucketCount("SBClientPhishing.Viewport.Width", 800,
                                      1);
  histogram_tester_.ExpectBucketCount("SBClientPhishing.Viewport.Height", 600,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.VisualFeaturesClearReason2",
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures,
      1);
  histogram_tester_.ExpectTotalCount(
      "SBClientPhishing.Viewport.EncodedResolution", 1);
}

// Tests that OnImageEmbeddingDone correctly populates visual features and image
// embedding in ClientPhishingRequest and clears classification_image_ on
// success.
TEST_F(ClientSideDetectionHostIOSTest,
       OnImageEmbeddingDoneSuccessPopulatesFeaturesAndClearsImage) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  safe_browsing::ImageFeatureEmbedding embedding;
  embedding.add_embedding_value(0.42f);

  safe_browsing::VisualFeatures visual_features;
  visual_features.mutable_image()->set_width(100);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<safe_browsing::ClientPhishingRequest> request,
             safe_browsing::ClientSideDetectionServiceBase::
                 ClientReportPhishingRequestCallback callback,
             const std::string& access_token) {
            ASSERT_TRUE(request);
            EXPECT_TRUE(request->has_image_feature_embedding());
            ASSERT_EQ(request->image_feature_embedding().embedding_value_size(),
                      1);
            EXPECT_FLOAT_EQ(
                request->image_feature_embedding().embedding_value(0), 0.42f);
            EXPECT_TRUE(request->has_visual_features());
            EXPECT_EQ(request->visual_features().image().width(), 100);
          });

  OnImageEmbeddingDone(host.get(), std::move(verdict),
                       /*did_match_high_confidence_allowlist=*/false,
                       /*is_invalid_ip=*/false,
                       safe_browsing::PhishingImageEmbedder::Result::kSuccess,
                       embedding, visual_features);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that OnImageEmbeddingDone clears classification_image_ without
// attaching visual features or image embedding on extraction failure.
TEST_F(ClientSideDetectionHostIOSTest,
       OnImageEmbeddingDoneFailureOmitsFeaturesAndClearsImage) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto failed_verdict =
      std::make_unique<safe_browsing::ClientPhishingRequest>();
  failed_verdict->set_url(kExampleUrl);
  failed_verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<safe_browsing::ClientPhishingRequest> request,
             safe_browsing::ClientSideDetectionServiceBase::
                 ClientReportPhishingRequestCallback callback,
             const std::string& access_token) {
            ASSERT_TRUE(request);
            EXPECT_FALSE(request->has_image_feature_embedding());
            EXPECT_FALSE(request->has_visual_features());
          });

  OnImageEmbeddingDone(
      host.get(), std::move(failed_verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingImageEmbedder::Result::kVisualExtractionFailed,
      safe_browsing::ImageFeatureEmbedding(), safe_browsing::VisualFeatures());

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that OnImageEmbeddingDone attaches image embedding and omits visual
// features when visual features are empty.
TEST_F(ClientSideDetectionHostIOSTest,
       OnImageEmbeddingDoneSuccessWithEmptyVisualFeatures) {
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  safe_browsing::ImageFeatureEmbedding embedding;
  embedding.add_embedding_value(0.42f);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<safe_browsing::ClientPhishingRequest> request,
             safe_browsing::ClientSideDetectionServiceBase::
                 ClientReportPhishingRequestCallback callback,
             const std::string& access_token) {
            ASSERT_TRUE(request);
            EXPECT_TRUE(request->has_image_feature_embedding());
            EXPECT_FALSE(request->visual_features().has_image());
          });

  OnImageEmbeddingDone(host.get(), std::move(verdict),
                       /*did_match_high_confidence_allowlist=*/false,
                       /*is_invalid_ip=*/false,
                       safe_browsing::PhishingImageEmbedder::Result::kSuccess,
                       embedding, safe_browsing::VisualFeatures());

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that an asynchronous real-time check with a cached FORCE_REQUEST
// verdict triggers a client-side detection force request.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckWithForceRequestVerdictTriggers) {
  SetForceRequestRTResponseInCacheManager(kExampleUrlPattern);
  TestAsyncSafeBrowsingCheck(
      GURL(kExampleUrl),
      /*expected_result=*/
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kTriggered,
      /*expect_force_request=*/true);
}

// Tests that an asynchronous real-time check does not trigger a force request
// when the cached verdict does not specify a force request.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckWithoutForceRequestVerdictDoesNotTrigger) {
  TestAsyncSafeBrowsingCheck(
      GURL(kExampleUrl),
      /*expected_result=*/
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kSkippedNotForced,
      /*expect_force_request=*/false);
}

// Tests that an asynchronous real-time check for a URL that does not match the
// current main-frame URL and is not in its redirect chain is ignored and does
// not trigger a force request.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckForDifferentUrlDoesNotTrigger) {
  SetForceRequestRTResponseInCacheManager(kDifferentUrlPattern);
  TestAsyncSafeBrowsingCheck(GURL(kDifferentUrl),
                             /*expected_result=*/std::nullopt,
                             /*expect_force_request=*/false);
}

// Tests that an asynchronous real-time check skips triggering a force request
// if the page-load trigger models request was already sent as a force request.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckDoesNotTriggerWhenAlreadyForced) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  SnapshotTabHelper::CreateForWebState(&web_state_);
  web_state_.SetContentsMimeType("text/html");

  SetForceRequestRTResponseInCacheManager(kExampleUrlPattern);

  GURL main_url(kExampleUrl);
  web::FakeNavigationContext context;
  context.SetUrl(main_url);
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state_.SetCurrentURL(main_url);
  web_state_.OnNavigationFinished(&context);

  // Simulate visual classification completing first and dispatching the forced
  // ping (which sets `trigger_model_request_sent_as_force_request_ = true`).
  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);
  host->OnVisualClassificationDoneForTesting(main_url, {});

  // When the async check finishes later, it should detect that a forced ping
  // was already sent and deduplicate rather than triggering a second ping.
  SimulateAsyncSafeBrowsingCheckFinished(host.get(), main_url);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kSkippedTriggerModelsPingSentAsForceRequest,
      1);
  EXPECT_FALSE(should_send_as_force_request(host.get()));
}

// Tests that an asynchronous real-time check for a redirect URL in the redirect
// chain triggers a force request.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckInRedirectChainTriggersForceRequest) {
  ::FakeSafeBrowsingClient client(profile_->GetPrefs());
  std::unique_ptr<ClientSideDetectionHostIOS> host =
      SetUpHostWithSafeBrowsing(client);

  GURL redirect_url("http://redirect.test");
  GURL main_url(kExampleUrl);

  SetForceRequestRTResponseInCacheManager("redirect.test/");

  SimulateRedirectChain(client, {redirect_url, main_url});
  EXPECT_EQ(host->GetRedirectChain(),
            std::vector<GURL>({redirect_url, main_url}));

  SimulateAsyncSafeBrowsingCheckFinished(host.get(), redirect_url);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kTriggered,
      1);
  EXPECT_TRUE(should_send_as_force_request(host.get()));

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.PreClassificationCheckResult.ForceRequest",
      safe_browsing::PreClassificationCheckResult::CLASSIFY, 1);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
}

// Tests that when multiple redirects in a redirect chain asynchronously finish
// with FORCE_REQUEST verdicts, only one force request is triggered and
// subsequent checks deduplicate cleanly.
TEST_F(ClientSideDetectionHostIOSTest,
       AsyncSBCheckMultipleRedirectsDeduplicates) {
  ::FakeSafeBrowsingClient client(profile_->GetPrefs());
  std::unique_ptr<ClientSideDetectionHostIOS> host =
      SetUpHostWithSafeBrowsing(client);

  GURL redirect_url1("http://redirect1.test");
  GURL redirect_url2("http://redirect2.test");
  GURL main_url(kExampleUrl);

  SetForceRequestRTResponseInCacheManager("redirect1.test/");
  SetForceRequestRTResponseInCacheManager("redirect2.test/");

  SimulateRedirectChain(client, {redirect_url1, redirect_url2, main_url});
  EXPECT_EQ(host->GetRedirectChain(),
            std::vector<GURL>({redirect_url1, redirect_url2, main_url}));

  // First async query finishes for redirect_url1 -> triggers force request.
  SimulateAsyncSafeBrowsingCheckFinished(host.get(), redirect_url1);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kTriggered,
      1);

  // Simulate visual classification completing and sending the forced ping.
  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);
  host->OnVisualClassificationDoneForTesting(main_url, {});

  // Second async query finishes for redirect_url2 -> deduplicates without
  // triggering another ping.
  SimulateAsyncSafeBrowsingCheckFinished(host.get(), redirect_url2);
  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetection."
      "AsyncCheckTriggerForceRequestResult",
      ClientSideDetectionHostBase::AsyncCheckTriggerForceRequestResult::
          kSkippedTriggerModelsPingSentAsForceRequest,
      1);
}

// Tests that MaybeStartImageEmbedding triggers image embedding and emits
// kImageEmbeddingBegin event when all preconditions are met and a valid view
// is attached.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingTriggersEmbeddingWhenEligible) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)];
  web_state_.SetView(view);

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(
                  testing::_, /*can_extract_visual_features=*/true, testing::_))
      .Times(1);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  verdict->mutable_visual_features()->mutable_image()->set_width(100);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  histogram_tester_.ExpectUniqueSample(
      "SBClientPhishing.HasVisualFeaturesImage2", true, 1);
  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageEmbeddingBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding clears the blurred image from visual
// features when visual features extraction is disallowed and passes
// can_extract_visual_features=false to BeginImageEmbedding.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingClearsVisualFeaturesWhenExtractionDisallowed) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  // No view attached so DetermineVisualFeaturesExtraction returns
  // kBelowMinFrame.
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(
      *mock_embedder,
      BeginImageEmbedding(testing::_, /*can_extract_visual_features=*/false,
                          testing::_))
      .Times(1);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  verdict->mutable_visual_features()->mutable_image()->set_width(100);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  histogram_tester_.ExpectTotalCount("SBClientPhishing.HasVisualFeaturesImage2",
                                     0);
  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageEmbeddingBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding passes can_extract_visual_features=true
// to BeginImageEmbedding when result is CLASSIFICATION_SKIPPED even if
// viewport extraction result is not kCanExtractVisualFeatures.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingClassificationSkippedAllowsVisualFeatures) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  // No view attached so DetermineVisualFeaturesExtraction returns
  // kBelowMinFrame.
  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(
                  testing::_, /*can_extract_visual_features=*/true, testing::_))
      .Times(1);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SKIPPED);

  ExpectClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageEmbeddingBegin,
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding skips image embedding when Enhanced
// Protection is disabled, clears classification_image_, and proceeds to
// intelligent scan / report sending.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingSkipsWhenStandardProtection) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(0);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  histogram_tester_.ExpectBucketCount(
      "SBClientPhishing.ClientSideDetectionEvent",
      ClientSideDetectionEvent::kImageEmbeddingBegin, 0);
  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding skips image embedding when the image
// embedding model is not available.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingSkipsWhenModelMissing) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(0);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding skips image embedding when the image
// embedding model metadata version does not match.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingSkipsWhenModelVersionMismatch) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(false));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(0);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding skips image embedding when verdict
// already has image feature embedding.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingSkipsWhenVerdictAlreadyHasEmbedding) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(0);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  verdict->mutable_image_feature_embedding()->add_embedding_value(0.5f);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding skips image embedding when the
// classification snapshot image is empty.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingSkipsWhenClassificationImageIsEmpty) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  // classification_image_ is empty by default.

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(0);
  set_image_embedder(host.get(), std::move(mock_embedder));

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  EXPECT_CALL(mock_service_, SendClientReportPhishingRequest(
                                 testing::_, testing::_, testing::_))
      .Times(1);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  EXPECT_TRUE(classification_image(host.get()).IsEmpty());
}

// Tests that MaybeStartImageEmbedding records image embedding start time using
// the configured tick clock.
TEST_F(ClientSideDetectionHostIOSTest,
       MaybeStartImageEmbeddingRecordsStartTimeWithTickClock) {
  base::SimpleTestTickClock test_clock;
  test_clock.SetNowTicks(base::TimeTicks::Now());

  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  EXPECT_CALL(mock_service_, HasImageEmbeddingModel())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_service_, IsModelMetadataImageEmbeddingVersionMatching())
      .WillRepeatedly(testing::Return(true));

  std::unique_ptr<ClientSideDetectionHostIOS> host = CreateHost();
  host->set_tick_clock_for_testing(&test_clock);
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_classification_image(host.get(), gfx::Image(CreateTestImage()));

  auto mock_embedder = std::make_unique<MockPhishingImageEmbedder>();
  EXPECT_CALL(*mock_embedder,
              BeginImageEmbedding(testing::_, testing::_, testing::_))
      .Times(1);
  set_image_embedder(host.get(), std::move(mock_embedder));

  test_clock.Advance(base::Milliseconds(123));
  base::TimeTicks expected_start_time = test_clock.NowTicks();

  auto verdict = std::make_unique<safe_browsing::ClientPhishingRequest>();
  verdict->set_url(kExampleUrl);
  verdict->set_client_side_detection_type(
      safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  MaybeStartImageEmbedding(
      host.get(), std::move(verdict),
      /*did_match_high_confidence_allowlist=*/false,
      /*is_invalid_ip=*/false,
      safe_browsing::PhishingDetectorResult::CLASSIFICATION_SUCCESS);

  EXPECT_EQ(image_embedding_start_time(host.get()), expected_start_time);
}

}  // namespace safe_browsing
