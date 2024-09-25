// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"

#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/optimization_guide/core/hints_component_util.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#import "components/optimization_guide/core/test_hints_component_creator.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/ukm/test_ukm_recorder.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_utils.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "services/metrics/public/cpp/ukm_source.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// These tests are roughly similarly to the tests in
// optimization_guide_keyed_service_browsertest.cc

namespace {

constexpr char kHintsURL[] = "https://hints.com/with_hints.html";
constexpr char kNoHintsURL[] = "https://nohints.com/no_hints.html";
constexpr char kRedirectURL[] = "https://hints.com/redirect.html";

// Wraps the NavigationContext and OptimizationGuideNavigationData together for
// tests.
class NavigationContextAndData {
 public:
  explicit NavigationContextAndData(const std::string& url) {
    navigation_context_ = std::make_unique<web::FakeNavigationContext>();
    navigation_context_->SetUrl(GURL(url));
    navigation_data_ = std::make_unique<OptimizationGuideNavigationData>(
        navigation_context_->GetNavigationId(),
        /*navigation_start=*/base::TimeTicks::Now());
    navigation_data_->set_navigation_url(navigation_context_->GetUrl());
  }

  std::unique_ptr<web::FakeNavigationContext> navigation_context_;
  std::unique_ptr<OptimizationGuideNavigationData> navigation_data_;
};

}  // namespace

class OptimizationGuideServiceTest : public PlatformTest {
 public:
  OptimizationGuideServiceTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kPurgeHintsStore);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::kGoogleApiKeyConfigurationCheckOverride);

    // The tests are run in the same process and share the same
    // OptimizationHintsComponentUpdateListener due to the global object usage
    // in GetInstance(). So reset the state for each test.
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->ResetStateForTesting();
  }

  ~OptimizationGuideServiceTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    auto testing_prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterProfilePrefs(testing_prefs->registry());

    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(
        optimization_guide::features::kOptimizationHints);
    enabled_features.push_back(
        optimization_guide::features::kRemoteOptimizationGuideFetching);
    if (url_keyed_anonymized_data_collection_enabled_) {
      enabled_features.push_back(
          optimization_guide::features::
              kRemoteOptimizationGuideFetchingAnonymousDataConsent);
      testing_prefs->SetBoolean(
          unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
          true);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.SetPrefService(std::move(testing_prefs));
    profile_ = std::move(builder).Build();
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());
  }

  void CreateOTRProfile() {
    profile_->CreateOffTheRecordProfileWithTestingFactories(
        {TestProfileIOS::TestingFactory{
            OptimizationGuideServiceFactory::GetInstance(),
            OptimizationGuideServiceFactory::GetDefaultFactory()}});
  }

  void PushHintsComponentAndWaitForCompletion() {
    RetryForHistogramUntilCountReached(
        histogram_tester(),
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    base::RunLoop run_loop;
    optimization_guide_service()
        ->GetHintsManager()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    GURL hints_url(kHintsURL);

    const optimization_guide::HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            optimization_guide::proto::NOSCRIPT, {hints_url.host()},
            hints_url.path().substr(1));

    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(component_info);

    run_loop.Run();
    RunUntilIdle();
  }

  void SimulateNavigation(
      NavigationContextAndData* context_and_data,
      const std::optional<GURL> redirect_url = std::nullopt) {
    return SimulateNavigationInBrowserState(
        context_and_data, optimization_guide_service_, redirect_url);
  }

  void SimulateNavigationInBrowserState(
      NavigationContextAndData* context_and_data,
      OptimizationGuideService* optimization_guide_service,
      const std::optional<GURL> redirect_url = std::nullopt) {
    std::vector<GURL> navigation_redirect_chain;
    navigation_redirect_chain.push_back(
        context_and_data->navigation_context_->GetUrl());

    optimization_guide_service->OnNavigationStartOrRedirect(
        context_and_data->navigation_data_.get());
    RunUntilIdle();

    if (redirect_url) {
      context_and_data->navigation_data_->set_navigation_url(*redirect_url);
      optimization_guide_service->OnNavigationStartOrRedirect(
          context_and_data->navigation_data_.get());
      navigation_redirect_chain.push_back(*redirect_url);
      RunUntilIdle();
    }

    optimization_guide_service->OnNavigationFinish(navigation_redirect_chain);
    RunUntilIdle();
  }

  void RegisterWithKeyedService() {
    optimization_guide_service()->RegisterOptimizationTypes(
        {optimization_guide::proto::NOSCRIPT});
  }

  // Calls the `CanApplyOptimizationAsync` and expects `expected_decision` when
  // the decision is returned. `on_decision_callback` is called when the
  // decision is called.
  void VerifyCanApplyOptimizationAsyncDecision(
      NavigationContextAndData* context_and_data,
      base::OnceClosure on_decision_callback,
      optimization_guide::OptimizationGuideDecision expected_decision) {
    optimization_guide_service()->CanApplyOptimization(
        context_and_data->navigation_context_.get()->GetUrl(),
        optimization_guide::proto::NOSCRIPT,
        base::BindOnce(
            [](base::OnceClosure on_decision_callback,
               optimization_guide::OptimizationGuideDecision expected_decision,
               optimization_guide::OptimizationGuideDecision decision,
               const optimization_guide::OptimizationMetadata& metadata) {
              EXPECT_EQ(expected_decision, decision);
              std::move(on_decision_callback).Run();
            },
            std::move(on_decision_callback), expected_decision));
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(bool enabled) {
    url_keyed_anonymized_data_collection_enabled_ = enabled;
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  OptimizationGuideService* optimization_guide_service() {
    return optimization_guide_service_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  TestProfileIOS* profile() { return profile_.get(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  optimization_guide::testing::TestHintsComponentCreator
      test_hints_component_creator_;
  bool url_keyed_anonymized_data_collection_enabled_ = false;
};

TEST_F(OptimizationGuideServiceTest, RemoteFetchingDisabled) {
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", false, 1);
  // TODO(crbug.com/40194448): Verify the optimization guide fetching synthetic
  // field trial is recorded.
}

TEST_F(OptimizationGuideServiceTest,
       NavigateToPageWithHintsButNoRegistrationDoesNotAttemptToLoadHint) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  PushHintsComponentAndWaitForCompletion();

  NavigationContextAndData context_and_data(kHintsURL);
  SimulateNavigation(&context_and_data);

  histogram_tester()->ExpectTotalCount("OptimizationGuide.LoadedHint.Result",
                                       0);

  // Navigate away so UKM get recorded.
  context_and_data = NavigationContextAndData(kHintsURL);
  SimulateNavigation(&context_and_data);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(OptimizationGuideServiceTest,
       NavigateToPageWithAsyncCallbackReturnsAnswerRedirect) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  auto run_loop = std::make_unique<base::RunLoop>();
  NavigationContextAndData context_and_data(kRedirectURL);

  VerifyCanApplyOptimizationAsyncDecision(
      &context_and_data, run_loop->QuitClosure(),
      optimization_guide::OptimizationGuideDecision::kFalse);

  SimulateNavigation(&context_and_data,
                     /*redirect_url=*/GURL(kNoHintsURL));
  run_loop->Run();
}

TEST_F(OptimizationGuideServiceTest,
       NavigateToPageWithAsyncCallbackReturnsAnswer) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  auto run_loop = std::make_unique<base::RunLoop>();
  NavigationContextAndData context_and_data(kHintsURL);

  VerifyCanApplyOptimizationAsyncDecision(
      &context_and_data, run_loop->QuitClosure(),
      optimization_guide::OptimizationGuideDecision::kTrue);

  SimulateNavigation(&context_and_data);
  run_loop->Run();
}

TEST_F(OptimizationGuideServiceTest,
       NavigateToPageWithAsyncCallbackReturnsAnswerEventually) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  auto run_loop = std::make_unique<base::RunLoop>();
  NavigationContextAndData context_and_data(kNoHintsURL);

  VerifyCanApplyOptimizationAsyncDecision(
      &context_and_data, run_loop->QuitClosure(),
      optimization_guide::OptimizationGuideDecision::kFalse);

  SimulateNavigation(&context_and_data);
  run_loop->Run();
}

TEST_F(OptimizationGuideServiceTest, NavigateToPageWithHintsLoadsHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  NavigationContextAndData context_and_data(kHintsURL);
  SimulateNavigation(&context_and_data);

  auto decision = optimization_guide_service()->CanApplyOptimization(
      GURL(kHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);

  // Navigate away so UKM get recorded.
  context_and_data = NavigationContextAndData(kHintsURL);
  SimulateNavigation(&context_and_data);

  // Expect that UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  ASSERT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // NOSCRIPT = 1, so bit mask is 10, which equals 2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      2);
}

TEST_F(OptimizationGuideServiceTest,
       RecordsMetricsWhenNavigationDataDestroyed) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  auto context_and_data = std::make_unique<NavigationContextAndData>(kHintsURL);
  SimulateNavigation(context_and_data.get());
  auto decision = optimization_guide_service()->CanApplyOptimization(
      GURL(kHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);

  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);

  // Make sure metrics get recorded when navigation data is destroyed.
  context_and_data.reset();
  RunUntilIdle();

  // Expect that the optimization guide UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // NOSCRIPT = 1, so bit mask is 10, which equals 2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      2);
}

TEST_F(OptimizationGuideServiceTest,
       NavigateToPageThatRedirectsToUrlWithHintsShouldAttemptTwoLoads) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  NavigationContextAndData context_and_data(kRedirectURL);
  SimulateNavigation(&context_and_data,
                     /*redirect_url=*/GURL(kHintsURL));
  auto decision = optimization_guide_service()->CanApplyOptimization(
      GURL(kHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);

  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 2);

  // Should attempt and succeed to load a hint once for the initial navigation
  // and redirect.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     true, 2);
  // Hint is still applicable so we expect it to be allowed to be applied.
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue, decision);
}

TEST_F(OptimizationGuideServiceTest, NavigateToPageWithoutHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  NavigationContextAndData context_and_data(kNoHintsURL);
  SimulateNavigation(&context_and_data);
  auto decision = optimization_guide_service()->CanApplyOptimization(
      GURL(kNoHintsURL), optimization_guide::proto::NOSCRIPT,
      /*optimization_metadata=*/nullptr);

  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  // There were no hints that match this URL, but there should still be an
  // attempt to load a hint but still fail.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse, decision);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(
          optimization_guide::OptimizationTypeDecision::kNoHintAvailable),
      1);
}

TEST_F(OptimizationGuideServiceTest, CheckForBlocklistFilter) {
  PushHintsComponentAndWaitForCompletion();

  OptimizationGuideService* ogks =
      OptimizationGuideServiceFactory::GetForProfile(profile());

  {
    base::HistogramTester histogram_tester;

    // Register an optimization type with an optimization filter.
    ogks->RegisterOptimizationTypes(
        {optimization_guide::proto::FAST_HOST_HINTS});
    // Wait until filter is loaded. This histogram will record twice: once when
    // the config is found and once when the filter is created.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.OptimizationFilterStatus.FastHostHints", 2);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.FastHostHints",
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.FastHostHints",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);

    EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
              ogks->CanApplyOptimization(
                  GURL("https://blockedhost.com/whatever"),
                  optimization_guide::proto::FAST_HOST_HINTS, nullptr));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ApplyDecision.FastHostHints",
        static_cast<int>(optimization_guide::OptimizationTypeDecision::
                             kNotAllowedByOptimizationFilter),
        1);
  }

  // Register another type with optimization filter.
  {
    base::HistogramTester histogram_tester;
    ogks->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    // Wait until filter is loaded. This histogram will record twice: once when
    // the config is found and once when the filter is created.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 2);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kFoundServerFilterConfig,
        1);

    // The previously loaded filter should still be loaded and give the same
    // result.
    EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
              ogks->CanApplyOptimization(
                  GURL("https://blockedhost.com/whatever"),
                  optimization_guide::proto::FAST_HOST_HINTS, nullptr));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ApplyDecision.FastHostHints",
        static_cast<int>(optimization_guide::OptimizationTypeDecision::
                             kNotAllowedByOptimizationFilter),
        1);
  }
}

TEST_F(OptimizationGuideServiceTest, IncognitoCanStillReadFromComponentHints) {
  // Wait until initialization logic finishes running and component pushed to
  // both incognito and regular browsers.
  PushHintsComponentAndWaitForCompletion();

  // Set up incognito profile and incognito OptimizationGuideService
  // consumer.
  CreateOTRProfile();
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();

  // Instantiate off the record Optimization Guide Service.
  OptimizationGuideService* otr_ogs =
      OptimizationGuideServiceFactory::GetForProfile(otr_profile);
  otr_ogs->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
  // Wait until initialization has stabilized.
  RunUntilIdle();

  // Navigate to a URL that has a hint from a component and wait for that hint
  // to have loaded.
  base::HistogramTester histogram_tester;
  NavigationContextAndData context_and_data(kHintsURL);
  SimulateNavigationInBrowserState(&context_and_data, otr_ogs);
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kTrue,
            otr_ogs->CanApplyOptimization(
                GURL(kHintsURL), optimization_guide::proto::NOSCRIPT, nullptr));
}

TEST_F(OptimizationGuideServiceTest, IncognitoStillProcessesBloomFilter) {
  PushHintsComponentAndWaitForCompletion();

  // Set up incognito browser and incognito OptimizationGuideService
  // consumer.
  CreateOTRProfile();
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();

  // Instantiate off the record Optimization Guide Service.
  OptimizationGuideService* otr_ogs =
      OptimizationGuideServiceFactory::GetForProfile(otr_profile);
  base::HistogramTester histogram_tester;

  // Register an optimization type with an optimization filter.
  otr_ogs->RegisterOptimizationTypes(
      {optimization_guide::proto::FAST_HOST_HINTS});
  // Wait until filter is loaded. This histogram will record twice: once when
  // the config is found and once when the filter is created.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.OptimizationFilterStatus.FastHostHints", 2);

  EXPECT_EQ(optimization_guide::OptimizationGuideDecision::kFalse,
            otr_ogs->CanApplyOptimization(
                GURL("https://blockedhost.com/whatever"),
                optimization_guide::proto::FAST_HOST_HINTS, nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.FastHostHints",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kNotAllowedByOptimizationFilter),
      1);
}

class OptimizationGuideServiceMSBBUserTest
    : public OptimizationGuideServiceTest {
 public:
  void SetUp() override {
    SetUrlKeyedAnonymizedDataCollectionEnabled(true);
    OptimizationGuideServiceTest::SetUp();
  }
};

TEST_F(OptimizationGuideServiceMSBBUserTest, RemoteFetchingEnabled) {
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", true, 1);
  // TODO(crbug.com/40194448): Verify the optimization guide fetching synthetic
  // field trial is recorded.
}
