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
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/prefs/testing_pref_service.h"
#import "components/universal_optout/prefs.h"
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

class ExtensionServiceTest : public PlatformTest {
 public:
  ExtensionServiceTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetOptedIn(bool opted_in) {
    pref_service_.SetBoolean(universal_optout::prefs::kUniversalOptOutEnabled,
                             opted_in);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
};

// Tests that ExtensionService is ready immediately when initialized with
// kNotEligible.
TEST_F(ExtensionServiceTest, TestInitializationWithNotEligible)
API_AVAILABLE(ios(18.4)) {
  auto fake_controller = std::make_unique<FakeExtensionController>();
  FakeExtensionController* raw_controller = fake_controller.get();

  ExtensionServiceImpl service(pref_service_, std::move(fake_controller));
  service.Initialize(web::UniversalOptOutState::kNotEligible);

  EXPECT_TRUE(service.IsReady());
  EXPECT_EQ(service.GetExtensionController(), raw_controller);
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

}  // namespace
