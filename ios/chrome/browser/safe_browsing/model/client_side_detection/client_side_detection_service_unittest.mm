// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"

#import <vector>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/observer_list.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/scoped_observation.h"
#import "base/strings/strcat.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/threading/thread_restrictions.h"
#import "components/optimization_guide/core/delivery/model_info.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/client_side_phishing_model.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_status_code.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace safe_browsing {
namespace {

base::FilePath GetModelFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("safe_browsing")
      .AppendASCII("client_model.pb");
}

base::FilePath GetAdditionalFilesPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("safe_browsing")
      .AppendASCII("visual_model_ios.tflite");
}

}  // namespace

class ClientSidePhishingModelObserverTracker
    : public optimization_guide::OptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      model_observers_.AddObserver(observer);
    }
  }

  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      model_observers_.RemoveObserver(observer);
    }
  }

  void NotifyModelFileUpdate(
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_files_path) {
    optimization_guide::proto::PredictionModel prediction_model;
    prediction_model.mutable_model()->set_download_url(model_file_path.value());
    prediction_model.mutable_model_info()->set_version(123);
    for (const auto& file : additional_files_path) {
      prediction_model.mutable_model_info()
          ->add_additional_files()
          ->set_file_path(file.value());
    }
    auto model_info =
        optimization_guide::ModelInfo::CreateFromProto(prediction_model);
    ASSERT_TRUE(model_info);
    for (auto& observer : model_observers_) {
      observer.OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
          *model_info);
    }
  }

  void NotifyModelFileUpdateNull() {
    for (auto& observer : model_observers_) {
      observer.OnModelUpdated(
          optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
          std::nullopt);
    }
  }

 private:
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      model_observers_;
};

class ScorerWaiter : public ClientSideDetectionService::Observer {
 public:
  explicit ScorerWaiter(ClientSideDetectionService* service) {
    scoped_observation_.Observe(service);
  }
  ~ScorerWaiter() override = default;

  void Wait() { ASSERT_TRUE(future_.Wait()); }

  // ClientSideDetectionService::Observer:
  void OnScorerChanged() override { future_.GetCallback().Run(); }

 private:
  base::test::TestFuture<void> future_;
  base::ScopedObservation<ClientSideDetectionService,
                          ClientSideDetectionService::Observer>
      scoped_observation_{this};
};

// Mock observer to verify when the ClientSideDetectionService notifies its
// observers about changes to the active Scorer instance.
class MockObserver : public ClientSideDetectionService::Observer {
 public:
  MOCK_METHOD(void, OnScorerChanged, (), (override));
};

class ClientSideDetectionServiceTest : public PlatformTest {
 public:
  ClientSideDetectionServiceTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  }

 protected:
  void SetUp() override {
    model_observer_tracker_ =
        std::make_unique<ClientSidePhishingModelObserverTracker>();
    csd_service_ = std::make_unique<ClientSideDetectionService>(
        profile_->GetPrefs(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        model_observer_tracker_.get());
  }

  void TearDown() override { csd_service_.reset(); }

  void UpdateModelForService(ClientSideDetectionService* service,
                             ClientSidePhishingModelObserverTracker* tracker) {
    ScorerWaiter waiter(service);
    tracker->NotifyModelFileUpdate(GetModelFilePath(),
                                   {GetAdditionalFilesPath()});
    waiter.Wait();
  }

  void UpdateModel(ClientSidePhishingModelObserverTracker* tracker) {
    UpdateModelForService(csd_service_.get(), tracker);
  }

  bool SendClientReportPhishingRequest(const GURL& phishing_url,
                                       const std::string& access_token) {
    auto request = std::make_unique<ClientPhishingRequest>();
    request->set_url(phishing_url.spec());
    request->set_client_score(0.8f);
    request->set_is_phishing(true);

    base::test::TestFuture<GURL, bool, std::optional<net::HttpStatusCode>,
                           std::optional<IntelligentScanVerdict>>
        future;
    csd_service_->SendClientReportPhishingRequest(
        std::move(request), future.GetCallback(), access_token);
    return future.Get<bool>();
  }

  void AddDummyVisualTfLiteModelScores(ClientPhishingRequest& request) {
    // This is unrelated to image embedding matching, but it is required to
    // reach the image embedding matching logic in
    // `ClassifyPhishingThroughThresholds`.
    const size_t thresholds_size =
        csd_service_->GetVisualTfLiteModelThresholds().size();
    for (size_t i = 0; i < thresholds_size; ++i) {
      ClientPhishingRequest::CategoryScore* category =
          request.add_tflite_model_scores();
      category->set_label("dummy_label");
      category->set_value(0.0);
    }
  }

  ClientSidePhishingModel* GetModel(ClientSideDetectionService* service) {
    return service->client_side_phishing_model_.get();
  }

  void OnScorerCreatedForTesting(ClientSideDetectionService* service,
                                 std::unique_ptr<Scorer> scorer) {
    service->OnScorerCreated(service->current_model_generation_,
                             std::move(scorer));
  }

  void OnScorerCreatedWithIdForTesting(ClientSideDetectionService* service,
                                       int generation_id,
                                       std::unique_ptr<Scorer> scorer) {
    service->OnScorerCreated(generation_id, std::move(scorer));
  }

  int GetCurrentModelGeneration(ClientSideDetectionService* service) {
    return service->current_model_generation_;
  }

  GURL GetPhishingReportUrl() {
    return ClientSideDetectionService::GetClientReportUrl(
        ClientSideDetectionServiceBase::kClientReportPhishingUrl);
  }

  void FlushCurrentSequence() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestProfileIOS> profile_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  std::unique_ptr<ClientSideDetectionService> csd_service_;
};

TEST_F(ClientSideDetectionServiceTest, ThrottlingLimit) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");

  ClientPhishingResponse response;
  response.set_phishy(true);
  std::string response_str = response.SerializeAsString();

  for (int i = 0; i < 3; ++i) {
    test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                         response_str);
    EXPECT_TRUE(SendClientReportPhishingRequest(url, "token"));
  }

  EXPECT_FALSE(SendClientReportPhishingRequest(url, "token"));
}

TEST_F(ClientSideDetectionServiceTest, ESBThrottlingLimit) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  FlushCurrentSequence();
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");

  ClientPhishingResponse response;
  response.set_phishy(true);
  std::string response_str = response.SerializeAsString();

  for (int i = 0; i < 10; ++i) {
    test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                         response_str);
    EXPECT_TRUE(SendClientReportPhishingRequest(url, "token"));
  }

  EXPECT_FALSE(SendClientReportPhishingRequest(url, "token"));
}

TEST_F(ClientSideDetectionServiceTest, CacheIntegration) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");

  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                       response.SerializeAsString());

  EXPECT_TRUE(SendClientReportPhishingRequest(url, "token"));

  bool is_phishing = false;
  EXPECT_TRUE(csd_service_->GetValidCachedResult(url, &is_phishing));
  EXPECT_TRUE(is_phishing);
}

TEST_F(ClientSideDetectionServiceTest, ScorerProfileIsolation) {
  // Verify both start with null scorers.
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());

  TestProfileIOS::Builder builder;
  std::unique_ptr<TestProfileIOS> profile2 = std::move(builder).Build();

  auto model_observer_tracker2 =
      std::make_unique<ClientSidePhishingModelObserverTracker>();
  auto csd_service2 = std::make_unique<ClientSideDetectionService>(
      profile2->GetPrefs(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      model_observer_tracker2.get());

  EXPECT_THAT(csd_service2->GetScorer(), testing::IsNull());

  // Update model for service 1.
  UpdateModel(model_observer_tracker_.get());

  // Service 1 should have a scorer, but Service 2 should still be null
  // (isolated).
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());
  EXPECT_THAT(csd_service2->GetScorer(), testing::IsNull());

  // Update model for service 2.
  UpdateModelForService(csd_service2.get(), model_observer_tracker2.get());

  // Both should have scorers, but they should be different instances.
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());
  EXPECT_THAT(csd_service2->GetScorer(), testing::NotNull());
  EXPECT_NE(csd_service_->GetScorer(), csd_service2->GetScorer());
}

TEST_F(ClientSideDetectionServiceTest, ScorerProfileIsolationWhenDisabled) {
  // Verify both start with null scorers.
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());

  TestProfileIOS::Builder builder;
  std::unique_ptr<TestProfileIOS> profile2 = std::move(builder).Build();
  // Disable Safe Browsing for the second profile.
  profile2->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  // Create service 2 with its own independent model observer tracker.
  auto model_observer_tracker2 =
      std::make_unique<ClientSidePhishingModelObserverTracker>();
  auto csd_service2 = std::make_unique<ClientSideDetectionService>(
      profile2->GetPrefs(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      model_observer_tracker2.get());

  EXPECT_THAT(csd_service2->GetScorer(), testing::IsNull());

  // Update model for service 1.
  UpdateModel(model_observer_tracker_.get());

  // Manually update model for service 2. We can't use UpdateModelForService
  // because ScorerWaiter expects a Scorer to be created, which won't happen
  // since the service is disabled.
  model_observer_tracker2->NotifyModelFileUpdate(GetModelFilePath(),
                                                 {GetAdditionalFilesPath()});
  // Flush to process any async callbacks if they were triggered for service 2.
  FlushCurrentSequence();

  // Service 1 should have a scorer, but Service 2 should remain null.
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());
  EXPECT_THAT(csd_service2->GetScorer(), testing::IsNull());
}

TEST_F(ClientSideDetectionServiceTest, ProfileDisableIsolation) {
  // Create a second profile.
  TestProfileIOS::Builder builder;
  std::unique_ptr<TestProfileIOS> profile2 = std::move(builder).Build();

  // Create a second CSD service (with its own independent model observer
  // tracker) for the second profile.
  auto model_observer_tracker2 =
      std::make_unique<ClientSidePhishingModelObserverTracker>();
  auto csd_service2 = std::make_unique<ClientSideDetectionService>(
      profile2->GetPrefs(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      model_observer_tracker2.get());

  // Update model for both services independently.
  UpdateModel(model_observer_tracker_.get());
  UpdateModelForService(csd_service2.get(), model_observer_tracker2.get());

  // Verify both have scorers.
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());
  ASSERT_THAT(csd_service2->GetScorer(), testing::NotNull());

  // Disable Safe Browsing in the second profile.
  profile2->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  // Flush to process any side effects of the pref change.
  FlushCurrentSequence();

  // The second service's scorer should be cleared, but the first service's
  // scorer should remain.
  EXPECT_THAT(csd_service2->GetScorer(), testing::IsNull());
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());
}

TEST_F(ClientSideDetectionServiceTest, PhishingClassifierScorerInjection) {
  // Update model to get a scorer.
  UpdateModel(model_observer_tracker_.get());
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // Verify that explicitly passing the Scorer works for the PhishingClassifier.
  PhishingClassifier classifier;
  EXPECT_FALSE(classifier.is_ready());
  classifier.set_scorer(csd_service_->GetScorer());
  EXPECT_TRUE(classifier.is_ready());

  // Verify that it is not ready when injected with null.
  classifier.set_scorer(nullptr);
  EXPECT_FALSE(classifier.is_ready());
}

TEST_F(ClientSideDetectionServiceTest, PhishingImageEmbedderScorerInjection) {
  // Update model to get a scorer.
  UpdateModel(model_observer_tracker_.get());
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // Verify that explicitly passing the Scorer works for the
  // PhishingImageEmbedder.
  PhishingImageEmbedder embedder;
  EXPECT_FALSE(embedder.is_ready());
  embedder.set_scorer(csd_service_->GetScorer());
  EXPECT_TRUE(embedder.is_ready());

  // Verify that it is not ready when injected with null.
  embedder.set_scorer(nullptr);
  EXPECT_FALSE(embedder.is_ready());
}

TEST_F(ClientSideDetectionServiceTest, ModelUpdatesScorer) {
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());

  UpdateModel(model_observer_tracker_.get());

  auto* model = GetModel(csd_service_.get());
  ASSERT_THAT(model, testing::NotNull());
  EXPECT_EQ(CSDModelType::kFlatbuffer, model->GetModelType());
  EXPECT_TRUE(model->GetModelSharedMemoryRegion().IsValid());
  EXPECT_TRUE(model->GetVisualTfLiteModel().IsValid());
  EXPECT_TRUE(model->IsEnabled());

  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());
}

TEST_F(ClientSideDetectionServiceTest, KillswitchPreventsScorerCreation) {
  // 1. Enable Killswitch.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kClientSideDetectionKillswitch);

  // 2. Recreate the service.
  csd_service_ = std::make_unique<ClientSideDetectionService>(
      profile_->GetPrefs(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      model_observer_tracker_.get());

  // 3. Verify scorer is null.
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());
}

TEST_F(ClientSideDetectionServiceTest, StaleTasksAreDiscarded) {
  // 1. Enable SB and set a valid model -> Scorer is non-null.
  UpdateModel(model_observer_tracker_.get());
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());

  int current_gen = GetCurrentModelGeneration(csd_service_.get());

  MockObserver observer;
  base::ScopedObservation<ClientSideDetectionService,
                          ClientSideDetectionService::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(csd_service_.get());

  // 2. Call with stale ID. Should be discarded.
  EXPECT_CALL(observer, OnScorerChanged()).Times(0);
  OnScorerCreatedWithIdForTesting(csd_service_.get(), current_gen - 1, nullptr);

  // Scorer should NOT have changed (still non-null).
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // 3. Call with current ID. Should be accepted.
  EXPECT_CALL(observer, OnScorerChanged()).Times(1);
  OnScorerCreatedWithIdForTesting(csd_service_.get(), current_gen, nullptr);

  // Scorer should have changed (now null because we passed nullptr).
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());
}

TEST_F(ClientSideDetectionServiceTest, ModelUpdateFailure_NullModel) {
  // First update to a valid model so we have a scorer.
  UpdateModel(model_observer_tracker_.get());
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // Now simulate a null model update (e.g. to remove bad model).
  ScorerWaiter waiter(csd_service_.get());
  model_observer_tracker_->NotifyModelFileUpdateNull();
  waiter.Wait();

  // Scorer should be cleared.
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());
}

TEST_F(ClientSideDetectionServiceTest, ScorerCreationFailure_ClearsScorer) {
  // First update to a valid model so we have a scorer.
  UpdateModel(model_observer_tracker_.get());
  EXPECT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // Now simulate scorer creation failure on background thread.
  OnScorerCreatedForTesting(csd_service_.get(), nullptr);

  // Scorer should be cleared.
  EXPECT_THAT(csd_service_->GetScorer(), testing::IsNull());
}

TEST_F(ClientSideDetectionServiceTest, IntelligentScanVerdictParsing) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");

  ClientPhishingResponse response;
  response.set_phishy(true);
  response.set_intelligent_scan_verdict(
      IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1);
  test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                       response.SerializeAsString());

  auto request = std::make_unique<ClientPhishingRequest>();
  request->set_url(url.spec());
  request->set_client_score(0.8f);

  base::test::TestFuture<GURL, bool, std::optional<net::HttpStatusCode>,
                         std::optional<IntelligentScanVerdict>>
      future;
  csd_service_->SendClientReportPhishingRequest(std::move(request),
                                                future.GetCallback(), "token");

  auto [returned_url, is_phishing, status, verdict] = future.Get();
  EXPECT_TRUE(is_phishing);
  EXPECT_THAT(verdict, testing::Optional(
                           IntelligentScanVerdict::SCAM_EXPERIMENT_VERDICT_1));
}

TEST_F(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithToken) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");
  std::string access_token = "fake access token";

  ClientPhishingResponse response;
  response.set_phishy(true);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::optional<std::string> auth_header =
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization);
        EXPECT_THAT(auth_header,
                    testing::Optional(base::StrCat({"Bearer ", access_token})));
      }));

  test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                       response.SerializeAsString());

  EXPECT_TRUE(SendClientReportPhishingRequest(url, access_token));
}

TEST_F(ClientSideDetectionServiceTest,
       SendClientReportPhishingRequestWithoutToken) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");
  std::string access_token;

  ClientPhishingResponse response;
  response.set_phishy(true);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_FALSE(
            request.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
      }));

  test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                       response.SerializeAsString());

  EXPECT_TRUE(SendClientReportPhishingRequest(url, access_token));
}

TEST_F(ClientSideDetectionServiceTest, IsPrivateIPAddress) {
  net::IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("10.1.2.3"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_TRUE(csd_service_->IsPrivateIPAddress(address));

  EXPECT_TRUE(address.AssignFromIPLiteral("1.2.3.4"));
  EXPECT_FALSE(csd_service_->IsPrivateIPAddress(address));
}

TEST_F(ClientSideDetectionServiceTest, NoImageEmbeddingMatch) {
  UpdateModel(model_observer_tracker_.get());

  // Setup TargetEmbeddings.
  constexpr float threshold = 0.95f;
  tflite::task::vision::FeatureVector target_fv;
  for (int i = 0; i < 64; ++i) {
    target_fv.add_value_float(i / 100.0);
  }
  std::vector<TargetEmbedding> test_targets;
  test_targets.push_back(TargetEmbedding(target_fv, threshold));
  csd_service_->SetTargetImageEmbeddingsForTesting(std::move(test_targets));

  // ClientPhishingRequest's image embedding doesn't meet threshold.
  ClientPhishingRequest request;
  request.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  AddDummyVisualTfLiteModelScores(request);

  // Add an image_feature_embedding that will NOT match.
  auto* features = request.mutable_image_feature_embedding();
  for (int i = 0; i < 64; ++i) {
    features->add_embedding_value(100);  // No match.
  }

  csd_service_->ClassifyPhishingThroughThresholds(&request);

  EXPECT_FALSE(request.is_phishing());
  EXPECT_FALSE(request.has_target_image_embedding_score());
}

TEST_F(ClientSideDetectionServiceTest, ImageEmbeddingMatch) {
  UpdateModel(model_observer_tracker_.get());

  ClientPhishingRequest request;
  request.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  AddDummyVisualTfLiteModelScores(request);

  // Setup TargetEmbeddings.
  constexpr float threshold = 0.95f;
  tflite::task::vision::FeatureVector target_fv;
  for (int i = 0; i < 64; ++i) {
    target_fv.add_value_float(0.1);
  }
  std::vector<TargetEmbedding> test_targets;
  test_targets.push_back(TargetEmbedding(target_fv, threshold));
  csd_service_->SetTargetImageEmbeddingsForTesting(std::move(test_targets));

  // Add an image_feature_embedding that will match.
  auto* features = request.mutable_image_feature_embedding();
  for (int i = 0; i < 63; ++i) {
    features->add_embedding_value(0.1);
  }
  features->add_embedding_value(0.1000001);  // Near Perfect match.

  csd_service_->ClassifyPhishingThroughThresholds(&request);

  EXPECT_TRUE(request.is_phishing());
  ASSERT_TRUE(request.has_target_image_embedding_score());
  // Verify that the matched target embedding has the expected SHA-256 hash
  // of the matching test vector defined in
  // components/test/data/safe_browsing/image_embeddings.textproto.
  EXPECT_EQ(request.target_image_embedding_score().id(),
            "ac7d206f95fb23a5ad4d52d76c4acd22d16bdcdbb3a6decc66f8eaabc9b40534");
  EXPECT_NEAR(request.target_image_embedding_score().score(), 1.0, 0.001);
}

TEST_F(ClientSideDetectionServiceTest, ImageEmbeddingSkipped_AlreadyPhishing) {
  UpdateModel(model_observer_tracker_.get());

  // Setup TargetEmbeddings.
  constexpr float threshold = 0.95f;
  tflite::task::vision::FeatureVector target_fv;
  for (int i = 0; i < 64; ++i) {
    target_fv.add_value_float(i / 100.0);
  }
  std::vector<TargetEmbedding> test_targets;
  test_targets.push_back(TargetEmbedding(target_fv, threshold));
  csd_service_->SetTargetImageEmbeddingsForTesting(std::move(test_targets));

  ClientPhishingRequest request;
  request.set_client_side_detection_type(
      ClientSideDetectionType::IMAGE_EMBEDDING_MATCH);
  AddDummyVisualTfLiteModelScores(request);

  // Add an image_feature_embedding that will match.
  auto* features = request.mutable_image_feature_embedding();
  for (int i = 0; i < 64; ++i) {
    features->add_embedding_value(i / 100.0);
  }

  // Simulate the Visual TFLite already having flagged the page as phishy.
  request.set_is_phishing(true);

  csd_service_->ClassifyPhishingThroughThresholds(&request);

  EXPECT_TRUE(request.is_phishing());
  EXPECT_FALSE(request.has_target_image_embedding_score());  // Not set despite
                                                             // perfect match.
}

TEST_F(ClientSideDetectionServiceTest, WebUILogging) {
  UpdateModel(model_observer_tracker_.get());
  GURL url("https://example.com");

  ClientPhishingResponse response;
  response.set_phishy(true);
  test_url_loader_factory_.AddResponse(GetPhishingReportUrl().spec(),
                                       response.SerializeAsString());

  // Clear any existing logs in the singleton.
  WebUIIOSInfoSingleton::GetInstance()->ClearClientPhishingRequestsSent();
  WebUIIOSInfoSingleton::GetInstance()->ClearClientPhishingResponsesReceived();
  WebUIIOSInfoSingleton::GetInstance()->AddListenerForTesting();

  // Ensure listener and logs are cleared after test.
  base::ScopedClosureRunner clear_listener(base::BindOnce([]() {
    auto* singleton = WebUIIOSInfoSingleton::GetInstance();
    singleton->ClearListenerForTesting();
    singleton->ClearClientPhishingRequestsSent();
    singleton->ClearClientPhishingResponsesReceived();
  }));

  EXPECT_TRUE(SendClientReportPhishingRequest(url, "token"));

  // Flush the current sequence to ensure the async logging tasks execute.
  FlushCurrentSequence();

  const auto& requests =
      WebUIIOSInfoSingleton::GetInstance()->client_phishing_requests_sent();
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(url.spec(), requests[0].request.url());

  const auto& responses = WebUIIOSInfoSingleton::GetInstance()
                              ->client_phishing_responses_received();
  ASSERT_EQ(1u, responses.size());
  EXPECT_TRUE(responses[0]->phishy());
}

TEST_F(ClientSideDetectionServiceTest,
       SafeBrowsingDisabledClearsScorerAndUnsubscribes) {
  // 1. Enable SB and set a valid model -> Scorer is non-null.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  UpdateModel(model_observer_tracker_.get());
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // 2. Disable SB -> Should unsubscribe and clear scorer.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  // Flush to process any side effects of pref change.
  FlushCurrentSequence();

  // Scorer should be cleared by the pref change.
  ASSERT_THAT(csd_service_->GetScorer(), testing::IsNull());

  // Set up a mock observer to listen for scorer changes.
  MockObserver observer;
  base::ScopedObservation<ClientSideDetectionService,
                          ClientSideDetectionService::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(csd_service_.get());

  // Expect OnScorerChanged to NOT be called.
  EXPECT_CALL(observer, OnScorerChanged()).Times(0);

  // 3. Trigger a null model update.
  // If still subscribed, this would trigger OnModelUpdated which notifies
  // observers. If unsubscribed, observers should NOT be notified.
  model_observer_tracker_->NotifyModelFileUpdateNull();

  // Flush to process any async callbacks if they were triggered (they shouldn't
  // be).
  FlushCurrentSequence();
}

// Tests that when `ClientSideDetectionService` is destroyed while blocking is
// disallowed on the UI thread, the active `Scorer` (holding file handles) is
// destroyed on a background thread without triggering assertion failures.
TEST_F(ClientSideDetectionServiceTest,
       ScorerDestroyedOnBackgroundThreadOnShutdown) {
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  UpdateModel(model_observer_tracker_.get());
  ASSERT_THAT(csd_service_->GetScorer(), testing::NotNull());

  // Disallow blocking on the UI thread during shutdown.
  base::ScopedDisallowBlocking disallow_blocking;
  csd_service_.reset();
}

}  // namespace safe_browsing
