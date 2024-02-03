// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/internal/constants.h"
#import "components/segmentation_platform/internal/database/client_result_prefs.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/prediction_options.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segment_selection_result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/segmentation_platform/public/service_proxy.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/segmentation_platform/model/ukm_data_manager_test_utils.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace segmentation_platform {
namespace {

// Observer that waits for service initialization.
class WaitServiceInitializedObserver : public ServiceProxy::Observer {
 public:
  explicit WaitServiceInitializedObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  void OnServiceStatusChanged(bool initialized, int status_flags) override {
    if (initialized) {
      std::move(closure_).Run();
    }
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace
class SegmentationPlatformServiceFactoryTest : public PlatformTest {
 public:
  SegmentationPlatformServiceFactoryTest()
      : test_utils_(std::make_unique<UkmDataManagerTestUtils>(&ukm_recorder_)) {
    // TODO(b/293500507): Create a base class for testing default models.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{optimization_guide::features::kOptimizationTargetPrediction, {}},
         {features::kSegmentationPlatformFeature, {}},
         {features::kSegmentationPlatformUkmEngine, {}},
         {features::kContextualPageActionShareModel, {}},
         {features::kSegmentationPlatformIosModuleRanker,
          {{kDefaultModelEnabledParam, "true"}}}},
        {});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kSegmentationPlatformRefreshResultsSwitch);
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kSegmentationPlatformDisableModelExecutionDelaySwitch);
  }
  ~SegmentationPlatformServiceFactoryTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    test_utils_->PreProfileInit({});
    profile_ = std::make_unique<ProfileData>(test_utils_.get(), "");
    WaitForServiceInit();

    ChromeBrowserState* otr_browser_state =
        profile_->browser_state
            ->CreateOffTheRecordBrowserStateWithTestingFactories(
                {std::make_pair(
                    SegmentationPlatformServiceFactory::GetInstance(),
                    SegmentationPlatformServiceFactory::GetDefaultFactory())});
    ASSERT_FALSE(SegmentationPlatformServiceFactory::GetForBrowserState(
        otr_browser_state));
  }

  void TearDown() override {
    web_task_env_.RunUntilIdle();
    profile_.reset();
    test_utils_.reset();
  }

  void InitServiceAndCacheResults(const std::string& segmentation_key) {
    WaitForServiceInit();
    WaitForClientResultPrefUpdate(segmentation_key);
    const std::string output = profile_->browser_state->GetPrefs()->GetString(
        kSegmentationClientResultPrefs);

    // TODO(b/297091996): Remove this when leak is fixed.
    web_task_env_.RunUntilIdle();

    profile_.reset();

    // Creating profile and initialising segmentation service again with prefs
    // from the last session.
    profile_ = std::make_unique<ProfileData>(test_utils_.get(), output);
    // Copying the prefs from last session.
    WaitForServiceInit();
    // TODO(b/297091996): Remove this when leak is fixed.
    web_task_env_.RunUntilIdle();
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service_ = profile_->browser_state->GetPrefs();
    std::unique_ptr<ClientResultPrefs> result_prefs_ =
        std::make_unique<ClientResultPrefs>(pref_service_);
    return result_prefs_->ReadClientResultFromPrefs(segmentation_key) !=
           nullptr;
  }

  void OnClientResultPrefUpdated(const std::string& segmentation_key) {
    if (!wait_for_pref_callback_.is_null() &&
        HasClientResultPref(segmentation_key)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForClientResultPrefUpdate(const std::string& segmentation_key) {
    if (HasClientResultPref(segmentation_key)) {
      return;
    }

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(profile_->browser_state->GetPrefs());
    pref_registrar_.Add(
        kSegmentationClientResultPrefs,
        base::BindRepeating(
            &SegmentationPlatformServiceFactoryTest::OnClientResultPrefUpdated,
            base::Unretained(this), segmentation_key));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

 protected:
  struct ProfileData {
    explicit ProfileData(UkmDataManagerTestUtils* test_utils,
                         const std::string& result_pref)
        : test_utils(test_utils) {
      TestChromeBrowserState::Builder builder;
      builder.AddTestingFactory(
          SegmentationPlatformServiceFactory::GetInstance(),
          SegmentationPlatformServiceFactory::GetDefaultFactory());
      browser_state = builder.Build();

      browser_state->GetPrefs()->SetString(kSegmentationClientResultPrefs,
                                           result_pref);
      test_utils->SetupForProfile(browser_state.get());
      service = SegmentationPlatformServiceFactory::GetForBrowserState(
          browser_state.get());
    }

    ~ProfileData() { test_utils->WillDestroyProfile(browser_state.get()); }

    ProfileData(ProfileData&) = delete;

    const raw_ptr<UkmDataManagerTestUtils> test_utils;
    std::unique_ptr<TestChromeBrowserState> browser_state;
    raw_ptr<SegmentationPlatformService> service;
  };

  void WaitForServiceInit() {
    if (profile_->service->IsPlatformInitialized()) {
      return;
    }
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    profile_->service->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    while (!profile_->service->IsPlatformInitialized()) {
      base::RunLoop().RunUntilIdle();
    }

    profile_->service->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  void ExpectGetClassificationResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status,
      absl::optional<std::vector<std::string>> expected_labels) {
    base::RunLoop loop;
    profile_->service->GetClassificationResult(
        segmentation_key, prediction_options, input_context,
        base::BindOnce(
            &SegmentationPlatformServiceFactoryTest::OnGetClassificationResult,
            base::Unretained(this), loop.QuitClosure(), expected_status,
            expected_labels));
    loop.Run();
  }

  void OnGetClassificationResult(
      base::RepeatingClosure closure,
      PredictionStatus expected_status,
      absl::optional<std::vector<std::string>> expected_labels,
      const ClassificationResult& actual_result) {
    EXPECT_EQ(actual_result.status, expected_status);
    if (expected_labels.has_value()) {
      EXPECT_EQ(actual_result.ordered_labels, expected_labels.value());
    }
    std::move(closure).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment web_task_env_;
  base::test::ScopedCommandLine scoped_command_line_;
  ukm::TestUkmRecorder ukm_recorder_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;

  std::unique_ptr<UkmDataManagerTestUtils> test_utils_;
  std::unique_ptr<ProfileData> profile_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, Test) {
  // TODO(crbug.com/1333641): Add test for the API once the initialization is
  // fixed.
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestSearchUserModel) {
  InitServiceAndCacheResults(kSearchUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kSearchUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kSearchUserModelLabelNone));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestIosModuleRankerModel) {
  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  int mvt_freshness_impression_count = -1;
  int shortcuts_freshness_impression_count = -1;
  int safety_check_freshness_impression_count = -1;
  int tab_resumption_freshness_impression_count = -1;
  int parcel_tracking_freshness_impression_count = -1;
  input_context->metadata_args.emplace(
      segmentation_platform::kMostVisitedTilesFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          mvt_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kShortcutsFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shortcuts_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kSafetyCheckFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          safety_check_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kTabResumptionFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          tab_resumption_freshness_impression_count));
  input_context->metadata_args.emplace(
      segmentation_platform::kParcelTrackingFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          parcel_tracking_freshness_impression_count));

  ExpectGetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, prediction_options,
      input_context, PredictionStatus::kSucceeded,
      std::vector<std::string>{"MostVisitedTiles", "Shortcuts", "SafetyCheck",
                               "TabResumption", "ParcelTracking"});
}

}  // namespace segmentation_platform
