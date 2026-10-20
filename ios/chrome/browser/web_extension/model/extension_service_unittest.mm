// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/prefs/testing_pref_service.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/web_extension/model/extension_service_impl.h"
#import "ios/web/public/extension/extension_controller.h"
#import "ios/web/public/web_client.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

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

// A test observer for ExtensionService.
class TestExtensionServiceObserver : public ExtensionService::Observer {
 public:
  void OnGPCLoadErrorChanged(ExtensionService* service,
                             bool has_gpc_load_error) override {
    call_count_++;
    last_has_gpc_load_error_ = has_gpc_load_error;
  }

  int call_count() const { return call_count_; }
  bool last_has_gpc_load_error() const { return last_has_gpc_load_error_; }

 private:
  int call_count_ = 0;
  bool last_has_gpc_load_error_ = false;
};

class ExtensionServiceTest : public PlatformTest {
 public:
  ExtensionServiceTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetOptedIn(bool opted_in) {
    pref_service_.SetBoolean(universal_optout::prefs::kUniversalOptOutEnabled,
                             opted_in);
  }

  void TearDown() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:experimental_flags::kWebExtensionLoadDelay];
    PlatformTest::TearDown();
  }

  void SetWebExtensionLoadDelay(base::TimeDelta delay,
                                bool should_fail = false) {
    NSString* value = nil;
    if (delay == base::Seconds(1)) {
      value = should_fail ? experimental_flags::kWebExtensionLoadDelay1sFail
                          : experimental_flags::kWebExtensionLoadDelay1s;
    } else if (delay == base::Seconds(5)) {
      value = should_fail ? experimental_flags::kWebExtensionLoadDelay5sFail
                          : experimental_flags::kWebExtensionLoadDelay5s;
    } else if (should_fail) {
      value = experimental_flags::kWebExtensionLoadDelayNoDelayFail;
    }
    if (value) {
      [[NSUserDefaults standardUserDefaults]
          setObject:value
             forKey:experimental_flags::kWebExtensionLoadDelay];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:experimental_flags::kWebExtensionLoadDelay];
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
};

// Tests that ExtensionService is ready immediately when initialized with
// kNotEligible, does not load the extension at startup, and loads the
// extension if the pref subsequently changes to enabled.
TEST_F(ExtensionServiceTest, TestInitializationWithNotEligible)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.WebExtensionsWereLoadedAtStartup());
  EXPECT_EQ(service.GetExtensionController(), raw_controller);
  EXPECT_FALSE(raw_controller->HasPendingLoad());
  EXPECT_FALSE(
      raw_controller->IsBuiltInExtensionLoaded(web::BuiltInExtension::kGPC));

  // Toggling the opt-out preference to enabled should trigger loading the
  // extension.
  SetOptedIn(true);
  EXPECT_TRUE(raw_controller->HasPendingLoad());
  EXPECT_EQ(raw_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPC);
  raw_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(
      raw_controller->IsBuiltInExtensionLoaded(web::BuiltInExtension::kGPC));
}

// Tests that ExtensionService triggers extension load when initialized with
// kEnabled.
TEST_F(ExtensionServiceTest, TestInitializationWithEnabled)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPC);
}

// Tests that ExtensionService is ready immediately when initialized with a
// null extension controller.
TEST_F(ExtensionServiceTest, TestInitializationWithNullExtensionController)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);

  ExtensionServiceImpl service(pref_service_,
                               /*extension_controller=*/nullptr);
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_TRUE(service.IsReady());
  EXPECT_EQ(service.GetExtensionController(), nullptr);
}

// Tests that ExtensionService creates the ExtensionController and is ready
// immediately when user is eligible but did not opt in.
TEST_F(ExtensionServiceTest, TestInitializationWithEligibleUserNotOptedIn)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEligible);

  EXPECT_TRUE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());
  EXPECT_FALSE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));
}

// Tests that ExtensionService creates the ExtensionController, triggers
// extension load, and pauses ready state until extension finishes loading when
// user is eligible and opted in.
TEST_F(ExtensionServiceTest, TestInitializationWithEligibleUserOptedIn)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPC);

  base::test::TestFuture<void> future;
  base::CallbackListSubscription subscription =
      service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Milliseconds(150));
  raw_fake_controller->CompleteLoad(/*success=*/true);

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  histogram_tester.ExpectUniqueSample("IOS.WebExtension.LoadSuccess", true, 1);
  histogram_tester.ExpectUniqueTimeSample("IOS.WebExtension.LoadDelay",
                                          base::Milliseconds(150), 1);
  histogram_tester.ExpectUniqueTimeSample("IOS.WebExtension.ReadyDelay",
                                          base::Milliseconds(150), 1);
}

// Tests that ExtensionService times out after 2 seconds and marks itself ready
// if the extension takes too long to load.
TEST_F(ExtensionServiceTest, TestInitializationLoadingTimeout)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.IsReady());
  ASSERT_EQ(service.GetExtensionController(), raw_fake_controller);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  base::test::TestFuture<void> future;
  base::CallbackListSubscription subscription =
      service.RunWhenReady(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  // Fast forward by 2 seconds to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(future.IsReady());

  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectUniqueTimeSample("IOS.WebExtension.ReadyDelay",
                                          base::Seconds(2), 1);

  // Complete load after timeout and verify that the actual load delay and
  // success are recorded.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(service.IsReady());
  histogram_tester.ExpectUniqueSample("IOS.WebExtension.LoadSuccess", true, 1);
  histogram_tester.ExpectUniqueTimeSample("IOS.WebExtension.LoadDelay",
                                          base::Milliseconds(2500), 1);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 1);
}

// Tests that ExtensionService cancels the timeout timer if loading finishes
// before 2 seconds.
TEST_F(ExtensionServiceTest, TestInitializationLoadingSuccessBeforeTimeout)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.IsReady());

  base::test::TestFuture<void> future;
  base::CallbackListSubscription subscription =
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

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEligible);

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

  auto fake_controller = std::make_unique<FakeExtensionController>();
  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

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

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.IsReady());
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  base::test::TestFuture<void> future;
  base::CallbackListSubscription subscription =
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

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

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

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPC));

  // Re-asserting enabled pref does not load again.
  SetOptedIn(true);
  EXPECT_EQ(raw_fake_controller->load_call_count(), 1);
}

// Tests that ExtensionService blocks startup when
// kUniversalOptOutExtensionSilent is enabled, and becomes ready after timeout.
TEST_F(ExtensionServiceTest, TestSilentModeBlocksStartupWithTimeout)
API_AVAILABLE(ios(18.4)) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      universal_optout::features::kUniversalOptOutExtensionSilent);

  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_FALSE(service.IsReady());
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPCTest);

  base::test::TestFuture<void> ready_future;
  base::CallbackListSubscription ready_subscription =
      service.RunWhenReady(ready_future.GetCallback());
  EXPECT_FALSE(ready_future.IsReady());

  // Advance time past 2 seconds to trigger timeout.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Should be ready now due to timeout.
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(ready_future.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
}

// Tests that ExtensionService becomes ready when extension completes loading
// before timeout in silent mode.
TEST_F(ExtensionServiceTest, TestSilentModeLoadsBeforeTimeout)
API_AVAILABLE(ios(18.4)) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      universal_optout::features::kUniversalOptOutExtensionSilent);

  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_FALSE(service.IsReady());
  EXPECT_EQ(raw_fake_controller->last_loaded_extension(),
            web::BuiltInExtension::kGPCTest);

  base::test::TestFuture<void> ready_future;
  base::CallbackListSubscription ready_subscription =
      service.RunWhenReady(ready_future.GetCallback());
  EXPECT_FALSE(ready_future.IsReady());

  task_environment_.FastForwardBy(base::Milliseconds(300));
  raw_fake_controller->CompleteLoad(/*success=*/true);

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(ready_future.IsReady());
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPCTest));

  // Pref change should not unload the extension in silent mode.
  SetOptedIn(false);
  EXPECT_TRUE(raw_fake_controller->IsBuiltInExtensionLoaded(
      web::BuiltInExtension::kGPCTest));
}

// Tests that a failed extension load in silent mode does not set
// HasGPCLoadError and does not notify observers.
TEST_F(ExtensionServiceTest, TestSilentModeLoadFailureDoesNotSetGPCLoadError)
API_AVAILABLE(ios(18.4)) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      universal_optout::features::kUniversalOptOutExtensionSilent);

  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_FALSE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fail silent extension load.
  raw_fake_controller->CompleteLoad(/*success=*/false);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  service.RemoveObserver(&observer);
}

// Tests that ExtensionService does not load extension and is ready immediately
// when kUniversalOptOutExtensionSilent is enabled with "controller-only".
TEST_F(ExtensionServiceTest, TestSilentModeControllerOnlyDoesNotLoadExtension)
API_AVAILABLE(ios(18.4)) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      universal_optout::features::kUniversalOptOutExtensionSilent,
      {{universal_optout::features::
            kUniversalOptOutExtensionSilentControllerOnly.name,
        "true"}});

  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.WebExtensionsWereLoadedAtStartup());
  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->load_call_count(), 0);
  EXPECT_NE(service.GetExtensionController(), nullptr);
}

// Tests that silent extension is not loaded if kUniversalOptOutSettings is
// enabled.
TEST_F(ExtensionServiceTest, TestSilentModeDisabledWhenSettingsEnabled)
API_AVAILABLE(ios(18.4)) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOutExtensionSilent,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});

  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.WebExtensionsWereLoadedAtStartup());
  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());
  EXPECT_EQ(raw_fake_controller->load_call_count(), 0);
}

// Tests that a failed extension load sets HasGPCLoadError to true and notifies
// observers, and that disabling the setting resets HasGPCLoadError to false.
TEST_F(ExtensionServiceTest, TestLoadFailureSetsLoadErrorAndResetOnDisable)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fail extension load.
  raw_fake_controller->CompleteLoad(/*success=*/false);
  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());

  // Disabling the setting should reset the load error.
  SetOptedIn(false);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_FALSE(observer.last_has_gpc_load_error());

  // Enabling the setting should trigger load attempt.
  SetOptedIn(true);
  EXPECT_FALSE(service.HasGPCLoadError());
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);

  service.RemoveObserver(&observer);
}

// Tests that timing out when loading an extension sets HasGPCLoadError to true
// and notifies observers.
TEST_F(ExtensionServiceTest, TestLoadTimeoutSetsLoadError)
API_AVAILABLE(ios(18.4)) {
  SetOptedIn(true);

  auto fake_controller = std::make_unique<FakeExtensionController>();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fast forward past the 10-second loading timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());

  // Disabling the setting should reset the load error.
  SetOptedIn(false);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_FALSE(observer.last_has_gpc_load_error());

  service.RemoveObserver(&observer);
}

// Tests that timing out when manually loading an extension sets
// `HasGPCLoadError` to true and notifies observers.
TEST_F(ExtensionServiceTest, TestManualLoadTimeoutSetsLoadError)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Manually enable the setting to trigger loading.
  SetOptedIn(true);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fast forward past the 2-second loading timeout.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());

  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);

  // Disabling the setting should reset the load error.
  SetOptedIn(false);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_FALSE(observer.last_has_gpc_load_error());

  // Enabling the setting should trigger another load attempt.
  SetOptedIn(true);
  EXPECT_FALSE(service.HasGPCLoadError());
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);
  histogram_tester.ExpectUniqueSample("IOS.WebExtension.LoadSuccess", true, 1);

  service.RemoveObserver(&observer);
}

// Tests that manually loading an extension cancels the timeout timer if loading
// finishes before 2 seconds.
TEST_F(ExtensionServiceTest, TestManualLoadSuccessBeforeTimeout)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(false);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());

  // Manually enable the setting to trigger loading.
  SetOptedIn(true);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  // Advance time by 1 second (less than the 2-second timeout).
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(service.HasGPCLoadError());

  // Complete load successfully.
  raw_fake_controller->CompleteLoad(/*success=*/true);
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);
  histogram_tester.ExpectUniqueSample("IOS.WebExtension.LoadSuccess", true, 1);
  histogram_tester.ExpectUniqueTimeSample("IOS.WebExtension.LoadDelay",
                                          base::Seconds(1), 1);

  // Fast forward another 2 seconds and verify that timeout does not fire.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  service.RemoveObserver(&observer);
}

// Tests that simulating load failure with no delay causes extension loading to
// fail immediately.
TEST_F(ExtensionServiceTest, TestLoadDelayNoDelayFail)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);
  SetWebExtensionLoadDelay(base::TimeDelta(), /*should_fail=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());

  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

// Tests that simulating a 1-second delay delays successful load completion so
// that it takes at least 1 second.
TEST_F(ExtensionServiceTest, TestLoadDelay1sPass)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);
  SetWebExtensionLoadDelay(base::Seconds(1));

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  EXPECT_FALSE(service.IsReady());

  // Underlying load finishes at 100ms.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  raw_fake_controller->CompleteLoad(/*success=*/true);

  // Still not ready before 1s has elapsed.
  EXPECT_FALSE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fast forward remaining 900ms to reach 1 second.
  task_environment_.FastForwardBy(base::Milliseconds(900));

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

// Tests that simulating a 1-second delay with failure waits 1 second then
// fails.
TEST_F(ExtensionServiceTest, TestLoadDelay1sFail)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);
  SetWebExtensionLoadDelay(base::Seconds(1), /*should_fail=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());
  EXPECT_FALSE(service.IsReady());

  // Fast forward 500ms (less than 1s).
  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 0);

  // Fast forward remaining 500ms to reach 1s.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

// Tests that simulating a 5-second delay times out at 2 seconds and then clears
// the load error when loading completes at 5 seconds.
TEST_F(ExtensionServiceTest, TestLoadDelay5sPassTimesOutThenRecovers)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);
  SetWebExtensionLoadDelay(base::Seconds(5));

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());
  raw_fake_controller->CompleteLoad(/*success=*/true);

  // At 2 seconds, the loading timeout should trigger.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());

  // At 5 seconds (advance another 3s), the delayed load completes successfully.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_FALSE(observer.last_has_gpc_load_error());
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

// Tests that simulating a 5-second delay with failure times out at 2 seconds
// and then fails at 5 seconds.
TEST_F(ExtensionServiceTest, TestLoadDelay5sFailTimesOutAndFails)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(true);
  SetWebExtensionLoadDelay(base::Seconds(5), /*should_fail=*/true);

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEnabled);

  EXPECT_FALSE(raw_fake_controller->HasPendingLoad());

  // At 2 seconds, the loading timeout should trigger.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(service.IsReady());
  EXPECT_TRUE(service.HasGPCLoadError());
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_TRUE(observer.last_has_gpc_load_error());

  // At 5 seconds (advance another 3s), the delay timer fires with failure.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(service.HasGPCLoadError());
  // The error state remains true, so observer is not called again.
  EXPECT_EQ(observer.call_count(), 1);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

// Tests that manually loading an extension respects simulated load delay.
TEST_F(ExtensionServiceTest, TestManualLoadWithDelay1s)
API_AVAILABLE(ios(18.4)) {
  base::HistogramTester histogram_tester;
  SetOptedIn(false);
  SetWebExtensionLoadDelay(base::Seconds(1));

  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_fake_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  TestExtensionServiceObserver observer;
  service.AddObserver(&observer);

  service.Initialize(web::UniversalOptOutState::kEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_FALSE(service.HasGPCLoadError());

  // Manually enable the setting to trigger loading.
  SetOptedIn(true);
  EXPECT_TRUE(raw_fake_controller->HasPendingLoad());

  // Complete load at 200ms.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  raw_fake_controller->CompleteLoad(/*success=*/true);

  // Still delayed before reaching 1s.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_FALSE(service.HasGPCLoadError());

  // Advance remaining 300ms to reach 1s.
  task_environment_.FastForwardBy(base::Milliseconds(300));
  EXPECT_FALSE(service.HasGPCLoadError());
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadSuccess", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.LoadDelay", 0);
  histogram_tester.ExpectTotalCount("IOS.WebExtension.ReadyDelay", 0);

  service.RemoveObserver(&observer);
}

}  // namespace
