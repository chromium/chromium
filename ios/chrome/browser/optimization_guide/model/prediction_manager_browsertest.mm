// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/containers/contains.h"
#import "base/files/file_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/component_updater/pref_names.h"
#import "components/download/internal/background_service/ios/background_download_task_helper.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"
#import "components/optimization_guide/core/optimization_guide_enums.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_prefs.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_target_model_observer.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/variations/hashing.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr int kSuccessfulModelVersion = 123;

std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse() {
  auto get_models_response =
      std::make_unique<optimization_guide::proto::GetModelsResponse>();

  auto prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(2);
  model_info->set_optimization_target(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_engine_versions(
      optimization_guide::proto::ModelEngineVersion::
          MODEL_ENGINE_VERSION_TFLITE_2_8);
  prediction_model->mutable_model()->set_download_url(
      "https://example.com/model");
  *get_models_response->add_models() = *prediction_model.get();

  return get_models_response;
}

class ModelFileObserver
    : public optimization_guide::OptimizationTargetModelObserver {
 public:
  using ModelFileReceivedCallback = base::OnceCallback<void(
      optimization_guide::proto::OptimizationTarget,
      base::optional_ref<const optimization_guide::ModelInfo>)>;

  ModelFileObserver() = default;
  ~ModelFileObserver() override = default;

  void set_model_file_received_callback(ModelFileReceivedCallback callback) {
    file_received_callback_ = std::move(callback);
  }

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override {
    if (file_received_callback_)
      std::move(file_received_callback_).Run(optimization_target, model_info);
  }

 private:
  ModelFileReceivedCallback file_received_callback_;
};

enum class PredictionModelsFetcherRemoteResponseType {
  kSuccessfulWithValidModelFile = 0,
  kSuccessfulWithInvalidModelFile = 1,
  kSuccessfulWithValidModelFileAndInvalidAdditionalFiles = 2,
  kSuccessfulWithValidModelFileAndValidAdditionalFiles = 3,
  kUnsuccessful = 4,
};

}  // namespace

class PredictionManagerTestBase : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    InitializeFeatureList();
    OptimizationGuideServiceFactory::InitializePredictionModelStore();

    download::BackgroundDownloadTaskHelper::SetIgnoreLocalSSLErrorForTesting(
        true);

    models_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    models_server_->ServeFilesFromSourceDirectory(
        "ios/chrome/test/data/optimization_guide");
    models_server_->RegisterRequestHandler(
        base::BindRepeating(&PredictionManagerTestBase::HandleGetModelsRequest,
                            base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(
        "ios/chrome/test/data/optimization_guide");
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");

    model_file_url_ = models_server_->GetURL("/signed_valid_model.crx3");
    model_file_with_good_additional_file_url_ =
        models_server_->GetURL("/additional_file_exists.crx3");
    model_file_with_nonexistent_additional_file_url_ =
        models_server_->GetURL("/additional_file_doesnt_exist.crx3");
    SetUpCommandLine(scoped_command_line_.GetProcessCommandLine());

    auto testing_prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterProfilePrefs(testing_prefs->registry());
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.SetPrefService(std::move(testing_prefs));
    profile_ = std::move(builder).Build();
  }

  void CreateOffTheRecordProfile() {
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            OptimizationGuideServiceFactory::GetInstance(),
            OptimizationGuideServiceFactory::GetDefaultFactory()}});
  }

  void TearDown() override {
    download::BackgroundDownloadTaskHelper::SetIgnoreLocalSSLErrorForTesting(
        false);
    // Reinitialize the store, so that tests do not use state from the
    // previous test.
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->ResetForTesting();
    PlatformTest::TearDown();
  }

  void SetResponseType(
      PredictionModelsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void RegisterWithKeyedService(ModelFileObserver* model_file_observer) {
    OptimizationGuideServiceFactory::GetForProfile(profile_.get())
        ->AddObserverForOptimizationTargetModel(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            std::nullopt, model_file_observer);
  }

  void SetComponentUpdatesEnabled(bool enabled) {
    GetApplicationContext()->GetLocalState()->SetBoolean(
        ::prefs::kComponentUpdatesEnabled, enabled);
  }

 protected:
  virtual void InitializeFeatureList() = 0;

  virtual void SetUpCommandLine(base::CommandLine* cmd) {
    cmd->AppendSwitch("enable-spdy-proxy-auth");

    cmd->AppendSwitch(
        optimization_guide::switches::kGoogleApiKeyConfigurationCheckOverride);
    cmd->AppendSwitchASCII(
        optimization_guide::switches::kOptimizationGuideServiceGetModelsURL,
        models_server_->GetURL("/").spec());

    cmd->AppendSwitchASCII("force-variation-ids", "4");
    auto* variations_ids_provider =
        variations::VariationsIdsProvider::GetInstance();
    ASSERT_TRUE(variations_ids_provider != nullptr);
    variations_ids_provider->ForceVariationIds({}, {"4"});
    cmd->AppendSwitch("append-variations-headers-to-localhost");
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    // Returning nullptr will cause the test server to fallback to serving the
    // file from the test data directory.
    if (request.GetURL() == model_file_url_)
      return nullptr;
    if (request.GetURL() == model_file_with_good_additional_file_url_)
      return nullptr;
    if (request.GetURL() == model_file_with_nonexistent_additional_file_url_)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    // The request to the remote Optimization Guide Service should always be a
    // POST.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    // Variations headers are not supported by iOS model fetcher.

    optimization_guide::proto::GetModelsRequest models_request;
    EXPECT_TRUE(models_request.ParseFromString(request.content));

    response->set_code(net::HTTP_OK);
    std::unique_ptr<optimization_guide::proto::GetModelsResponse>
        get_models_response = BuildGetModelsResponse();
    if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                              kSuccessfulWithInvalidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          https_url_with_content_.spec());
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithValidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::
                   kSuccessfulWithValidModelFileAndInvalidAdditionalFiles) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_with_nonexistent_additional_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::
                   kSuccessfulWithValidModelFileAndValidAdditionalFiles) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_with_good_additional_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  GURL model_file_url_;
  GURL model_file_with_good_additional_file_url_;
  GURL model_file_with_nonexistent_additional_file_url_;
  GURL https_url_with_content_;
  GURL https_url_without_content_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedCommandLine scoped_command_line_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  PredictionModelsFetcherRemoteResponseType response_type_ =
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile;
};

class PredictionManagerTest : public PredictionManagerTestBase {
 protected:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {optimization_guide::features::kOptimizationHints, {}},
        {optimization_guide::features::kRemoteOptimizationGuideFetching, {}},
        {optimization_guide::features::kOptimizationTargetPrediction,
         {{"fetch_startup_delay_ms", "2000"}}},
        {optimization_guide::features::kOptimizationGuideModelDownloading, {}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
};

TEST_F(PredictionManagerTest, ComponentUpdatesEnabledPrefDisabled) {
  ModelFileObserver model_file_observer;
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  SetComponentUpdatesEnabled(false);
  base::HistogramTester histogram_tester;
  RegisterWithKeyedService(&model_file_observer);
  task_environment_.RunUntilIdle();

  // Should not have made fetch request.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 0);
}

TEST_F(PredictionManagerTest, ModelsAndFeaturesStoreInitialized) {
  ModelFileObserver model_file_observer;
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService(&model_file_observer);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
}

TEST_F(PredictionManagerTest, PredictionModelFetchFailed) {
  ModelFileObserver model_file_observer;
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService(&model_file_observer);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

class PredictionManagerModelDownloadingBrowserTest
    : public PredictionManagerTest {
 public:
  PredictionManagerModelDownloadingBrowserTest() = default;
  ~PredictionManagerModelDownloadingBrowserTest() override = default;

  void SetUp() override {
    model_file_observer_ = std::make_unique<ModelFileObserver>();
    PredictionManagerTest::SetUp();
  }

  ModelFileObserver* model_file_observer() {
    return model_file_observer_.get();
  }

  void RegisterModelFileObserverWithProfile(ProfileIOS* profile = nullptr) {
    OptimizationGuideServiceFactory::GetForProfile(profile ? profile
                                                           : profile_.get())
        ->AddObserverForOptimizationTargetModel(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/std::nullopt, model_file_observer_.get());
  }

  // Sets up the model observer to receive valid ModelInfo.
  void SetUpValidModelInfoReceival(base::RunLoop* run_loop,
                                   const std::set<base::FilePath::StringType>&
                                       expected_additional_files = {}) {
    model_file_observer_->set_model_file_received_callback(base::BindOnce(
        [](base::RunLoop* run_loop,
           const std::set<base::FilePath::StringType>&
               expected_additional_files,
           optimization_guide::proto::OptimizationTarget optimization_target,
           base::optional_ref<const optimization_guide::ModelInfo> model_info) {
          base::ScopedAllowBlockingForTesting scoped_allow_blocking;

          EXPECT_EQ(
              optimization_target,
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
          EXPECT_TRUE(model_info.has_value());

          EXPECT_EQ(123, model_info->GetVersion());
          EXPECT_TRUE(model_info->GetModelFilePath().IsAbsolute());
          EXPECT_TRUE(base::PathExists(model_info->GetModelFilePath()));

          EXPECT_EQ(expected_additional_files.size(),
                    model_info->GetAdditionalFiles().size());
          for (const base::FilePath& add_file :
               model_info->GetAdditionalFiles()) {
            EXPECT_TRUE(add_file.IsAbsolute());
            EXPECT_TRUE(base::PathExists(add_file));
            EXPECT_TRUE(base::Contains(expected_additional_files,
                                       add_file.BaseName().value()));
          }
          run_loop->Quit();
        },
        run_loop, expected_additional_files));
  }

  // Sets up the model observer to not receive any model.
  void SetUpNoModelInfoReceival() {
    model_file_observer_->set_model_file_received_callback(base::BindOnce(
        [](optimization_guide::proto::OptimizationTarget optimization_target,
           base::optional_ref<const optimization_guide::ModelInfo> model_info) {
          FAIL() << "Should not be called";
        }));
  }

 private:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {optimization_guide::features::kOptimizationHints, {}},
        {optimization_guide::features::kRemoteOptimizationGuideFetching, {}},
        {optimization_guide::features::kOptimizationTargetPrediction,
         {{"fetch_startup_delay_ms", "2000"}}},
        {optimization_guide::features::kOptimizationGuideModelDownloading,
         {{"unrestricted_model_downloading", "true"}}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

 protected:
  std::unique_ptr<ModelFileObserver> model_file_observer_;
};

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestIncognitoUsesModelFromRegularProfile) {
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  // Set up model download with regular profile.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    SetUpValidModelInfoReceival(run_loop.get());
    RegisterModelFileObserverWithProfile();

    // Wait until the observer receives the file. We increase the timeout to 60
    // seconds here since the file is on the larger side.
    {
      base::test::ScopedRunLoopTimeout file_download_timeout(FROM_HERE,
                                                             base::Seconds(60));
      run_loop->Run();
    }
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        optimization_guide::PredictionModelDownloadStatus::kSuccess, 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
        kSuccessfulModelVersion, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
        kSuccessfulModelVersion, 1);
  }

  // Now set up model download with incognito profile. Download should not
  // happen, but the OnModelUpdated callback should be triggered.
  {
    base::HistogramTester otr_histogram_tester;
    CreateOffTheRecordProfile();
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    SetUpValidModelInfoReceival(run_loop.get());
    RegisterModelFileObserverWithProfile(profile_->GetOffTheRecordProfile());
    task_environment_.RunUntilIdle();
    otr_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
    otr_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  }
}

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestIncognitoDoesntFetchModels) {
  base::HistogramTester histogram_tester;
  CreateOffTheRecordProfile();

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithInvalidModelFile);

  // Registering should not initiate the fetch and the model updated callback
  // should not be triggered too.
  SetUpNoModelInfoReceival();
  RegisterModelFileObserverWithProfile(profile_->GetOffTheRecordProfile());

  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.StoreInitialized",
      1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestDownloadUrlAcceptedByDownloadServiceButInvalid) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithInvalidModelFile);

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithProfile();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      optimization_guide::PredictionModelDownloadStatus::kFailedCrxVerification,
      1);
  // An unverified file should not notify us that it's ready.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
}

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestSuccessfulModelFileFlow) {
  base::HistogramTester histogram_tester;

  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetUpValidModelInfoReceival(run_loop.get());

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithProfile();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(FROM_HERE,
                                                           base::Seconds(60));
    run_loop->Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      optimization_guide::PredictionModelDownloadStatus::kSuccess, 1);

  // No error when moving the file so there will be no record.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.ReplaceFileError", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
}

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestSuccessfulModelFileFlowWithAdditionalFile) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithValidModelFileAndValidAdditionalFiles);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetUpValidModelInfoReceival(run_loop.get(),
                              {FILE_PATH_LITERAL("good_additional_file.txt")});

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithProfile();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(FROM_HERE,
                                                           base::Seconds(60));
    run_loop->Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      optimization_guide::PredictionModelDownloadStatus::kSuccess, 1);

  // No error when moving the file so there will be no record.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.ReplaceFileError", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
}

TEST_F(PredictionManagerModelDownloadingBrowserTest,
       TestSuccessfulModelFileFlowWithInvalidAdditionalFile) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithValidModelFileAndInvalidAdditionalFiles);
  SetUpNoModelInfoReceival();

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded. But the model
  // with invalid additional file, will not be delivered to observers.
  RegisterModelFileObserverWithProfile();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);
  task_environment_.RunUntilIdle();

  // The additional file does not exist.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      optimization_guide::PredictionModelDownloadStatus::
          kFailedInvalidAdditionalFile,
      1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}
