// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

#import "base/base64.h"
#import "base/metrics/persistent_histogram_allocator.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/variations/pref_names.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/seed_response.h"
#import "components/variations/service/ui_string_overrider.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/synthetic_trial_registry.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_test_utils.h"
#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_service_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// The following headers should be imported after
// "ios_chrome_variations_seed_store.h".
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+fetcher.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+testing.h"

namespace {

using ::variations::kTestSeedData;
using ::variations::SeedApplicationStage;

// Returns a seed. If `valid` is true, this will return a seed using
// kTestSeedData, otherwise it will return a seed with unmatching signature.
std::unique_ptr<variations::SeedResponse> GetSeed(bool valid) {
  std::string data;
  base::Base64Decode(kTestSeedData.base64_compressed_data, &data);

  auto seed = std::make_unique<variations::SeedResponse>();
  seed->signature =
      valid ? kTestSeedData.base64_signature : "invalid signature";
  seed->data = data;
  seed->is_gzip_compressed = true;
  seed->date = base::Time::Now();
  return seed;
}

}  // namespace

#pragma mark - Tests

// Tests the IOSChromeVariationsSeedStore, particularly its integration with the
// core variations service.
class IOSChromeVariationsSeedStoreTest : public PlatformTest {
 protected:
  IOSChromeVariationsSeedStoreTest()
      : enabled_state_provider_(
            new metrics::TestEnabledStateProvider(false, false)) {
    original_feature_list_ = base::FeatureList::ClearInstanceForTesting();
    // Disable field trial testing to enable test seed.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);
  }

  void TearDown() override {
    [IOSChromeVariationsSeedStore resetForTesting];
    base::GlobalHistogramAllocator::ReleaseForTesting();
    // Remove the test instance created by `SetUpFieldTrials` calls and restore
    // the original.
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(original_feature_list_));
    PlatformTest::TearDown();
  }

  // Returns local state.
  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  // Lazy getter for metrics state manager used to set up variations service.
  metrics::MetricsStateManager* GetMetricsStateManager() {
    // Lazy-initialize the metrics_state_manager so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = metrics::MetricsStateManager::Create(
          GetLocalState(), enabled_state_provider_.get(), std::wstring(),
          base::FilePath());
      metrics_state_manager_->InstantiateFieldTrialList();
    }
    return metrics_state_manager_.get();
  }

  // Sets up variations service.
  void SetUpVariationsService() {
    if (variations_service_) {
      return;
    }
    CHECK(!synthetic_trial_registry_);
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
    variations_service_ = variations::VariationsService::Create(
        std::make_unique<IOSChromeVariationsServiceClient>(), GetLocalState(),
        GetMetricsStateManager(), "dummy-disable-background-switch",
        variations::UIStringOverrider(),
        network::TestNetworkConnectionTracker::CreateGetter(),
        synthetic_trial_registry_.get());
  }

  // Sets up field trials. If the test seed is simulate-fetched, this step
  // should have applied the seed and added the studies.
  void SetUpFieldTrials() {
    CHECK(variations_service_);
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    variations_service_->SetUpFieldTrials(
        std::vector<std::string>(), std::string(),
        std::vector<base::FeatureList::FeatureOverrideInfo>(),
        std::move(feature_list), &ios_field_trials_);
  }

  // Verify that the study in the test seed is `applied`.
  void VerifyTestSeedTrialExists(bool applied) {
    bool trial_exists =
        base::FieldTrialList::TrialExists(kTestSeedData.study_names[0]);
    EXPECT_EQ(applied, trial_exists);
  }

 private:
  // Test set up dependencies.
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<base::FeatureList> original_feature_list_;
  // Variations service dependencies.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  // Variations service.
  std::unique_ptr<variations::VariationsService> variations_service_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  IOSChromeFieldTrials ios_field_trials_;
};

// Tests the scenario when a valid seed is fetched and stored.
TEST_F(IOSChromeVariationsSeedStoreTest, testGoodSeedFetched) {
  ASSERT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  // Simulate seed fetch.
  [IOSChromeVariationsSeedStore updateSharedSeed:GetSeed(/*valid=*/true)];
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedStored);
  // Set up variations service and create field trial.
  SetUpVariationsService();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedImported);
  SetUpFieldTrials();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedApplied);
  VerifyTestSeedTrialExists(true);
}

// Tests the scenario when an invalid seed is fetched and stored.
TEST_F(IOSChromeVariationsSeedStoreTest, testBadSeedFetched) {
  ASSERT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  // Simulate seed download.
  [IOSChromeVariationsSeedStore updateSharedSeed:GetSeed(/*valid=*/false)];
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedStored);
  // Set up variations service and create field trial.
  SetUpVariationsService();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedImported);
  SetUpFieldTrials();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedImported);
  VerifyTestSeedTrialExists(false);
}

// Tests the scenario when no seed is fetched.
TEST_F(IOSChromeVariationsSeedStoreTest, testNoSeedFetched) {
  ASSERT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  // Set up variations service and create field trial before seed is fetched and
  // applied.
  SetUpVariationsService();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  SetUpFieldTrials();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  VerifyTestSeedTrialExists(false);
}

// Tests the scenario when a seed is stored but the variations service does NOT
// need to import it. This is an edge case that should only happen when a
// previous run crashes on FRE screens.
TEST_F(IOSChromeVariationsSeedStoreTest, testLateSeedFetched) {
  // Simulate a seed in the core variations seed store.
  GetLocalState()->SetString(variations::prefs::kVariationsSeedSignature,
                             "seed");

  ASSERT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kNoSeed);
  // Simulate seed fetch.
  [IOSChromeVariationsSeedStore updateSharedSeed:GetSeed(/*valid=*/true)];
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedStored);
  // Set up variations service and create field trial.
  SetUpVariationsService();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedStored);
  SetUpFieldTrials();
  EXPECT_EQ([IOSChromeVariationsSeedStore seedApplicationStage],
            SeedApplicationStage::kSeedStored);
  VerifyTestSeedTrialExists(false);
}
