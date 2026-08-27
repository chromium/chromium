// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_host_ios.h"

#import <UIKit/UIKit.h>

#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
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
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
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
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace safe_browsing {
namespace {

constexpr char kExampleUrl[] = "https://example.com";
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

  // 1. Simulate TRIGGER_MODELS (tier 3) running.
  set_last_request_type(host.get(),
                        safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);
  set_is_csd_running(host.get(), true);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::TRIGGER_MODELS);

  // 2. Start FORCE_REQUEST (tier 1 - higher priority). Should preempt
  // TRIGGER_MODELS.
  host->MaybeStartPreClassification(
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  EXPECT_EQ(last_request_type(host.get()),
            safe_browsing::ClientSideDetectionType::FORCE_REQUEST);

  // 3. Simulate FORCE_REQUEST running.
  set_is_csd_running(host.get(), true);

  // 4. Attempt TRIGGER_MODELS (tier 3 - lower priority) while FORCE_REQUEST is
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

}  // namespace safe_browsing
