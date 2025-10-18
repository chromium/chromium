// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/uuid.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/segmentation_platform/internal/constants.h"
#import "components/segmentation_platform/internal/database/client_result_prefs.h"
#import "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/prediction_options.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segment_selection_result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/segmentation_platform/public/service_proxy.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/ukm_data_manager_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace segmentation_platform {
namespace {

// Sets signals relevant for the Price Tracking module
void ConfigureForPriceTrackingModule(scoped_refptr<InputContext> input_context,
                                     bool enable = true) {
  // Required signal for Price Tracking module
  input_context->metadata_args.emplace(
      segmentation_platform::kIsNewUser,
      segmentation_platform::processing::ProcessedValue::FromFloat(0.0f));

  input_context->metadata_args.emplace(
      segmentation_platform::kIsSynced,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          enable ? 1.0f : 0.0f));
}

// Sets signals relevant for the Lens ephemeral module
void ConfigureForLensModule(scoped_refptr<InputContext> input_context,
                            bool enable = true) {
  float signal_value = enable ? 1.0f : 0.0f;

  // Required signals for Lens module
  input_context->metadata_args.emplace(
      segmentation_platform::kLensNotUsedRecently,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          signal_value));
  input_context->metadata_args.emplace(
      segmentation_platform::kLensAllowedByEnterprisePolicy,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          signal_value));

  input_context->metadata_args.emplace(
      segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          !signal_value));
  input_context->metadata_args.emplace(
      segmentation_platform::tips_manager::signals::
          kOpenedWebsiteInAnotherLanguage,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          !signal_value));
  input_context->metadata_args.emplace(
      segmentation_platform::tips_manager::signals::kUsedGoogleTranslation,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          !signal_value));
}

// Sets signals relevant for the Enhanced Safe Browsing ephemeral module
void ConfigureForEnhancedSafeBrowsingModule(
    scoped_refptr<InputContext> input_context,
    bool enable = true) {
  float signal_value = enable ? 1.0f : 0.0f;

  // Required signals for Enhanced Safe Browsing module
  input_context->metadata_args.emplace(
      segmentation_platform::kLacksEnhancedSafeBrowsing,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          signal_value));
  input_context->metadata_args.emplace(
      segmentation_platform::kEnhancedSafeBrowsingAllowedByEnterprisePolicy,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          signal_value));
}

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
         {features::kSegmentationPlatformEphemeralCardRanker, {}}},
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
    profile_data_ = std::make_unique<ProfileData>(test_utils_.get(), "");
    WaitForServiceInit();

    ProfileIOS* otr_profile =
        profile_data_->profile->CreateOffTheRecordProfileWithTestingFactories(
            {TestProfileIOS::TestingFactory{
                SegmentationPlatformServiceFactory::GetInstance(),
                SegmentationPlatformServiceFactory::GetDefaultFactory()}});
    ASSERT_FALSE(
        SegmentationPlatformServiceFactory::GetForProfile(otr_profile));
  }

  void TearDown() override {
    web_task_env_.RunUntilIdle();
    profile_data_.reset();
    test_utils_.reset();
  }

  void InitServiceAndCacheResults(const std::string& segmentation_key) {
    WaitForServiceInit();
    WaitForClientResultPrefUpdate(segmentation_key);
    const std::string output = profile_data_->profile->GetPrefs()->GetString(
        kSegmentationClientResultPrefs);

    // TODO(b/297091996): Remove this when leak is fixed.
    web_task_env_.RunUntilIdle();

    profile_data_.reset();

    // Creating profile and initialising segmentation service again with prefs
    // from the last session.
    profile_data_ = std::make_unique<ProfileData>(test_utils_.get(), output);
    // Copying the prefs from last session.
    WaitForServiceInit();
    // TODO(b/297091996): Remove this when leak is fixed.
    web_task_env_.RunUntilIdle();
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service_ = profile_data_->profile->GetPrefs();
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
    pref_registrar_.Init(profile_data_->profile->GetPrefs());
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
        : result_pref(result_pref), test_utils(test_utils) {
      TestProfileIOS::Builder builder;
      builder.AddTestingFactory(
          SegmentationPlatformServiceFactory::GetInstance(),
          base::BindOnce(&ProfileData::SetUpEnvironment, base::Unretained(this))
              .Then(SegmentationPlatformServiceFactory::GetDefaultFactory()));
      builder.AddTestingFactory(
          commerce::ShoppingServiceFactory::GetInstance(),
          base::BindRepeating([](ProfileIOS* profile)
                                  -> std::unique_ptr<KeyedService> {
            std::unique_ptr<bookmarks::BookmarkNode> bookmark =
                std::make_unique<bookmarks::BookmarkNode>(
                    /*id=*/100, base::Uuid::GenerateRandomV4(), GURL());
            std::unique_ptr<commerce::MockShoppingService> shopping_service =
                std::make_unique<commerce::MockShoppingService>();
            shopping_service->SetGetAllShoppingBookmarksValue({bookmark.get()});
            return std::move(shopping_service);
          }));
      profile = std::move(builder).Build();
      service =
          SegmentationPlatformServiceFactory::GetForProfile(profile.get());
    }

    ~ProfileData() { test_utils->WillDestroyProfile(profile.get()); }

    ProfileData(ProfileData&) = delete;

    // Setup environment required to create the SegmentationPlatformService.
    ProfileIOS* SetUpEnvironment(ProfileIOS* setup_profile) {
      setup_profile->GetPrefs()->SetString(kSegmentationClientResultPrefs,
                                           result_pref);
      test_utils->SetupForProfile(setup_profile);
      return setup_profile;
    }

    const std::string result_pref;
    const raw_ptr<UkmDataManagerTestUtils> test_utils;
    std::unique_ptr<TestProfileIOS> profile;
    raw_ptr<SegmentationPlatformService> service;
    std::unique_ptr<bookmarks::BookmarkNode> bookmark_;
  };

  void WaitForServiceInit() {
    if (profile_data_->service->IsPlatformInitialized()) {
      return;
    }
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    profile_data_->service->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    while (!profile_data_->service->IsPlatformInitialized()) {
      base::RunLoop().RunUntilIdle();
    }

    profile_data_->service->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  void ExpectGetClassificationResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status,
      std::optional<std::vector<std::string>> expected_labels) {
    base::RunLoop loop;
    profile_data_->service->GetClassificationResult(
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
      std::optional<std::vector<std::string>> expected_labels,
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
  std::unique_ptr<ProfileData> profile_data_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, Test) {
  // TODO(crbug.com/40227968): Add test for the API once the initialization is
  // fixed.
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestMetricsEnabled) {
  WaitForServiceInit();
  SegmentationPlatformServiceImpl* service =
      reinterpret_cast<SegmentationPlatformServiceImpl*>(
          profile_data_->service.get());
  EXPECT_TRUE(service->IsMetricsEnabledForTesting());
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
  int shop_card_freshness_impression_count = -1;

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
  input_context->metadata_args.emplace(
      segmentation_platform::kShopCardFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shop_card_freshness_impression_count));

  ExpectGetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, prediction_options,
      input_context, PredictionStatus::kSucceeded,
      std::vector<std::string>{"MostVisitedTiles", "Shortcuts", "SafetyCheck",
                               "TabResumption", "ParcelTracking", "ShopCard"});
}

// Tests that the EphemeralHomeModuleBackend returns the PriceTracking module
// when the corresponding signals are present.
TEST_F(SegmentationPlatformServiceFactoryTest,
       TestEphemeralHomeModuleBackendForPriceTracking) {
  home_modules::HomeModulesCardRegistry* registry =
      SegmentationPlatformServiceFactory::GetHomeCardRegistryForProfile(
          profile_data_->profile.get());
  ASSERT_TRUE(registry);
  EXPECT_EQ(3u, registry->get_all_cards_by_priority().size());

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();

  ConfigureForPriceTrackingModule(input_context, true);

  // Disable other cards to ensure price tracking is shown
  ConfigureForLensModule(input_context, false);
  ConfigureForEnhancedSafeBrowsingModule(input_context, false);

  std::vector<std::string> result = {
      segmentation_platform::kPriceTrackingNotificationPromo};
  ExpectGetClassificationResult(
      kEphemeralHomeModuleBackendKey, prediction_options, input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

// Tests that the EphemeralHomeModuleBackend returns the Lens Search module
// when the corresponding signals are present.
TEST_F(SegmentationPlatformServiceFactoryTest,
       TestEphemeralHomeModuleBackendForLensSearch) {
  home_modules::HomeModulesCardRegistry* registry =
      SegmentationPlatformServiceFactory::GetHomeCardRegistryForProfile(
          profile_data_->profile.get());
  ASSERT_TRUE(registry);
  EXPECT_EQ(3u, registry->get_all_cards_by_priority().size());

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();

  ConfigureForLensModule(input_context, true);

  // Disable other cards to ensure Lens is shown
  ConfigureForPriceTrackingModule(input_context, false);
  ConfigureForEnhancedSafeBrowsingModule(input_context, false);

  std::vector<std::string> result = {
      segmentation_platform::kLensEphemeralModuleSearchVariation};
  ExpectGetClassificationResult(
      kEphemeralHomeModuleBackendKey, prediction_options, input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

// Tests that the EphemeralHomeModuleBackend returns the Enhanced Safe Browsing
// module when the corresponding signals are present.
TEST_F(SegmentationPlatformServiceFactoryTest,
       TestEphemeralHomeModuleBackendForEnhancedSafeBrowsing) {
  home_modules::HomeModulesCardRegistry* registry =
      SegmentationPlatformServiceFactory::GetHomeCardRegistryForProfile(
          profile_data_->profile.get());
  ASSERT_TRUE(registry);
  EXPECT_EQ(3u, registry->get_all_cards_by_priority().size());

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();

  ConfigureForEnhancedSafeBrowsingModule(input_context, true);

  // Disable other cards to ensure Enhanced Safe Browsing is shown
  ConfigureForPriceTrackingModule(input_context, false);
  ConfigureForLensModule(input_context, false);

  std::vector<std::string> result = {
      segmentation_platform::kEnhancedSafeBrowsingEphemeralModule};
  ExpectGetClassificationResult(
      kEphemeralHomeModuleBackendKey, prediction_options, input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

// Tests priority ordering when multiple cards have their signals enabled.
TEST_F(SegmentationPlatformServiceFactoryTest,
       TestEphemeralHomeModuleBackendForPriorityOrdering) {
  home_modules::HomeModulesCardRegistry* registry =
      SegmentationPlatformServiceFactory::GetHomeCardRegistryForProfile(
          profile_data_->profile.get());
  ASSERT_TRUE(registry);
  EXPECT_EQ(3u, registry->get_all_cards_by_priority().size());

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();
  // Enable signals for all modules
  ConfigureForPriceTrackingModule(input_context, true);
  ConfigureForLensModule(input_context, true);
  ConfigureForEnhancedSafeBrowsingModule(input_context, true);

  // The highest priority card should be returned first. In this case, Price
  // Tracking takes precedence over others.
  std::vector<std::string> result = {
      segmentation_platform::kPriceTrackingNotificationPromo};
  ExpectGetClassificationResult(
      kEphemeralHomeModuleBackendKey, prediction_options, input_context,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/result);
}

// Verify that kIosDefaultBrowserPromoKey fails execution since it should never
// be executed by the client.
TEST_F(SegmentationPlatformServiceFactoryTest, TestDefaultBrowserModel) {
  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kIosDefaultBrowserPromoKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kFailed, std::nullopt);
}

}  // namespace segmentation_platform
