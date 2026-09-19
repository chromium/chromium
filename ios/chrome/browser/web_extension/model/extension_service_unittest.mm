// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "components/universal_optout/universal_optout_service.h"
#import "components/variations/service/test_variations_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns whether the current OS version is supported. The GPC extension is
// only supported on iOS versions between iOS 18.4 and iOS 27.
bool IsSupportedOS() {
  if (@available(iOS 18.4, *)) {
    return !base::ios::IsRunningOnIOS27OrLater();
  }
  return false;
}

// A fake web::ExtensionController for testing.
class API_AVAILABLE(ios(18.4)) FakeExtensionController
    : public web::ExtensionController {
 public:
  FakeExtensionController() = default;
  ~FakeExtensionController() override = default;

  void LoadBuiltInExtension(web::BuiltInExtension extension,
                            base::OnceCallback<void(bool)> callback) override {
    load_call_count_++;
    last_loaded_extension_ = extension;
    load_callback_ = std::move(callback);
  }

  void UnloadBuiltInExtension(
      web::BuiltInExtension extension,
      base::OnceCallback<void(bool)> callback) override {
    unload_call_count_++;
    is_loaded_ = false;
    if (callback) {
      std::move(callback).Run(true);
    }
  }

  bool IsBuiltInExtensionLoaded(
      web::BuiltInExtension extension) const override {
    return is_loaded_;
  }

  bool HasPendingLoad() const { return !load_callback_.is_null(); }

  void CompleteLoad(bool success) {
    is_loaded_ = success;
    if (load_callback_) {
      std::move(load_callback_).Run(success);
    }
  }

  int load_call_count() const { return load_call_count_; }
  int unload_call_count() const { return unload_call_count_; }

  std::optional<web::BuiltInExtension> last_loaded_extension() const {
    return last_loaded_extension_;
  }

 private:
  bool is_loaded_ = false;
  int load_call_count_ = 0;
  int unload_call_count_ = 0;
  std::optional<web::BuiltInExtension> last_loaded_extension_;
  base::OnceCallback<void(bool)> load_callback_;
};

class ExtensionServiceTest : public PlatformTest {
 public:
  ExtensionServiceTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());

    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                            /*enabled=*/true);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());

    base::Time start_time;
    CHECK(base::Time::FromString("2026-08-11T12:00:00Z", &start_time));
    test_clock_.SetNow(start_time);
  }

  void SetEligible(bool eligible) {
    pref_service_.SetBoolean(universal_optout::prefs::kUniversalOptOutEligible,
                             eligible);
  }

  void SetOptedIn(bool opted_in) {
    pref_service_.SetBoolean(universal_optout::prefs::kUniversalOptOutEnabled,
                             opted_in);
  }

  std::unique_ptr<universal_optout::UniversalOptOutService> CreateOptOutService(
      bool eligible = true) {
    auto service = std::make_unique<universal_optout::UniversalOptOutService>(
        pref_service_, *variations_service_,
        *identity_test_env_.identity_manager(), test_clock_);
    SetEligible(eligible);
    return service;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::SimpleTestClock test_clock_;
};

// Tests that ExtensionService is ready immediately when initialized with a
// null UniversalOptOutService.
TEST_F(ExtensionServiceTest, TestInitializationWithNullOptOutService)
API_AVAILABLE(ios(18.4)) {
  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_controller = fake_controller.get();

  ExtensionService service(&pref_service_,
                           /*universal_optout_service=*/nullptr,
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_TRUE(service.IsReady());
  EXPECT_EQ(service.GetExtensionController(), raw_controller);

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

// Tests that ExtensionService is ready immediately when initialized with an
// ineligible user.
TEST_F(ExtensionServiceTest, TestInitializationWithIneligibleUser)
API_AVAILABLE(ios(18.4)) {
  auto optout_service = CreateOptOutService(/*eligible=*/false);
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_TRUE(service.IsReady());
  EXPECT_EQ(service.GetExtensionController(), raw_controller);

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

// Tests that ExtensionService is ready immediately when initialized with a
// null extension controller.
TEST_F(ExtensionServiceTest, TestInitializationWithNullExtensionController)
API_AVAILABLE(ios(18.4)) {
  auto optout_service = CreateOptOutService(/*eligible=*/true);
  SetOptedIn(true);

  ExtensionService service(&pref_service_, optout_service.get(),
                           /*extension_controller=*/nullptr);
  service.Initialize();

  EXPECT_TRUE(service.IsReady());
  EXPECT_EQ(service.GetExtensionController(), nullptr);

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

// Tests that ExtensionService creates the ExtensionController and is ready
// immediately when user is eligible but did not opt in.
TEST_F(ExtensionServiceTest, TestInitializationWithEligibleUserNotOptedIn)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(false);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_TRUE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());
  EXPECT_FALSE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

// Tests that ExtensionService creates the ExtensionController, triggers
// extension load, and pauses ready state until extension finishes loading when
// user is eligible and opted in.
TEST_F(ExtensionServiceTest, TestInitializationWithEligibleUserOptedIn)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_FALSE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPC);

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  raw_fake_controller->CompleteLoad(/*success=*/true);

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));
}

// Tests that ExtensionService times out after 2 seconds and marks itself ready
// if the extension takes too long to load.
TEST_F(ExtensionServiceTest, TestInitializationLoadingTimeout)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_FALSE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Fast forward by 2 seconds to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());

  // Complete load after timeout and verify it does not cause errors.
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(service.IsReady());
}

// Tests that ExtensionService cancels the timeout timer if loading finishes
// before 2 seconds.
TEST_F(ExtensionServiceTest, TestInitializationLoadingSuccessBeforeTimeout)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_FALSE(service.IsReady());

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Advance time by 1 second (less than the 2-second timeout).
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(service.IsReady());

  // Complete load successfully.
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());

  // Fast forward another 2 seconds and verify ready state remains true.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(service.IsReady());
}

// Tests that ExtensionService listens to the pref and loads or unloads the
// extension when the enable state changes.
TEST_F(ExtensionServiceTest, TestPrefChangedLoadsAndUnloadsExtension)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(false);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_TRUE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_FALSE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  // User opts in via pref -> loads extension.
  SetOptedIn(true);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  // User opts out via pref -> unloads extension.
  SetOptedIn(false);
  EXPECT_FALSE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));
}

// Tests that Shutdown clears the ExtensionController and pending callbacks.
TEST_F(ExtensionServiceTest, TestShutdown)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_NE(service.GetExtensionController(), nullptr);
  service.Shutdown();
  EXPECT_EQ(service.GetExtensionController(), nullptr);
}

// Tests that when the preference is disabled while the extension is loading,
// the service unloads the extension once loading completes and transitions
// to ready upon finishing startup loading.
TEST_F(ExtensionServiceTest, TestPrefDisabledWhileExtensionIsLoading)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_FALSE(service.IsReady());
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  base::test::TestFuture<void> future;
  service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Disable the preference while the extension is still loading.
  SetOptedIn(false);

  // Preference changes do not affect the startup readiness state.
  EXPECT_FALSE(service.IsReady());
  EXPECT_FALSE(future.IsReady());

  // Complete the pending load; the service should immediately unload the
  // extension and become ready for startup.
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_FALSE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));
  EXPECT_EQ(raw_fake_controller->unload_call_count(), 1);
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());
}

// Tests that toggling the preference while the extension is currently loading
// does not trigger duplicate load calls.
TEST_F(ExtensionServiceTest,
       TestPrefToggledWhileExtensionIsLoadingDoesNotDuplicateLoad)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  // Toggle off then on while still loading.
  SetOptedIn(false);
  SetOptedIn(true);

  // Should not have triggered a second load call because it was already
  // loading.
  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);

  // Complete loading successfully.
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));
  EXPECT_EQ(raw_fake_controller->unload_call_count(), 0);
}

// Tests that setting the preference to enabled when the extension is already
// loaded does not trigger another load call.
TEST_F(ExtensionServiceTest,
       TestPrefEnabledWhileExtensionAlreadyLoadedDoesNotDuplicateLoad)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);
  auto optout_service = CreateOptOutService(/*eligible=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionService service(&pref_service_, optout_service.get(),
                           std::move(fake_controller));
  service.Initialize();

  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  // Re-asserting enabled pref does not load again.
  SetOptedIn(true);
  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);
}

class ExtensionServiceFactoryTest : public PlatformTest {
 public:
  ExtensionServiceFactoryTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());

    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                            /*enabled=*/true);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());

    base::Time start_time;
    CHECK(base::Time::FromString("2026-08-11T12:00:00Z", &start_time));
    test_clock_.SetNow(start_time);
  }
  ~ExtensionServiceFactoryTest() override = default;

  std::unique_ptr<KeyedService> CreateOptOutService(bool eligible,
                                                    ProfileIOS* profile) {
    auto service = std::make_unique<universal_optout::UniversalOptOutService>(
        *profile->GetPrefs(), *variations_service_,
        *identity_test_env_.identity_manager(), test_clock_);
    profile->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEligible, eligible);
    return service;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::SimpleTestClock test_clock_;
};

// Tests that ExtensionService is not created if the extension flag is disabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenExtensionFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut},
      /*disabled_features=*/{
          universal_optout::features::kUniversalOptOutExtension});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if kUniversalOptOut is disabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenUniversalOptOutFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{universal_optout::features::kUniversalOptOut});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if the user is not eligible.
TEST_F(ExtensionServiceFactoryTest, TestFactoryReturnsNullWhenIneligible) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/false));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if UniversalOptOutService is null.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenOptOutServiceNull) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  // UniversalOptOutServiceFactory has kNoServiceForTests, so it returns null.
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is created when eligible and both flags are
// enabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsServiceWhenEligibleAndFlagsEnabled) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);
}

// Tests that the service is redirected in incognito.
TEST_F(ExtensionServiceFactoryTest, TestFactoryRedirectedInIncognito) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* regular_service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  ExtensionService* otr_service =
      ExtensionServiceFactory::GetForProfile(profile->GetOffTheRecordProfile());
  EXPECT_NE(regular_service, nullptr);
  EXPECT_EQ(regular_service, otr_service);
}

// Tests that ExtensionService is not created on iOS 27 or later.
TEST_F(ExtensionServiceFactoryTest, TestFactoryReturnsNullOnIOS27OrLater) {
  if (!base::ios::IsRunningOnIOS27OrLater()) {
    GTEST_SKIP() << "Only runs on iOS 27 or later.";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

}  // namespace
