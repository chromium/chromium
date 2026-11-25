// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/deferred_first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_ui_handler.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Unit tests for `DeferredFirstRunCoordinator`.
class DeferredFirstRunCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [UIView setAnimationsEnabled:NO];
    scoped_feature_list_.InitAndEnableFeature(
        first_run::kBestFeaturesScreenInFirstRun);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    mock_promos_ui_handler_ =
        OCMStrictProtocolMock(@protocol(PromosManagerUIHandler));
    mock_promos_manager_handler_ =
        OCMStrictProtocolMock(@protocol(PromosManagerCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_promos_manager_handler_
                     forProtocol:@protocol(PromosManagerCommands)];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    EXPECT_TRUE(base::test::RunUntil(
        [&] { return base_view_controller_.presentedViewController == nil; }));
    [scoped_key_window_.Get() setRootViewController:nil];
    base_view_controller_ = nil;
    mock_promos_ui_handler_ = nil;
    mock_promos_manager_handler_ = nil;
    browser_.reset();
    profile_ = nullptr;
    [UIView setAnimationsEnabled:YES];
    PlatformTest::TearDown();
  }

  // Persists `screens` into `prefs::kFirstRunDeferredScreens`.
  void SetDeferredScreens(const std::vector<ScreenType>& screens) {
    base::ListValue list;
    for (ScreenType screen : screens) {
      list.Append(static_cast<int>(screen));
    }
    GetApplicationContext()->GetLocalState()->SetList(
        prefs::kFirstRunDeferredScreens, std::move(list));
  }

  // Returns the current contents of `prefs::kFirstRunDeferredScreens`.
  std::vector<ScreenType> GetDeferredScreens() {
    std::vector<ScreenType> screens;
    const base::ListValue& list =
        GetApplicationContext()->GetLocalState()->GetList(
            prefs::kFirstRunDeferredScreens);
    for (const base::Value& entry : list) {
      screens.push_back(static_cast<ScreenType>(entry.GetInt()));
    }
    return screens;
  }

  // Instantiates `coordinator_` wired to `mock_promos_ui_handler_`.
  void CreateCoordinator() {
    coordinator_ = [[DeferredFirstRunCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    coordinator_.promosUIHandler = mock_promos_ui_handler_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::HistogramTester histogram_tester_;
  ScopedKeyWindow scoped_key_window_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  id mock_promos_ui_handler_;
  id mock_promos_manager_handler_;
  DeferredFirstRunCoordinator* coordinator_;
};

// Tests that `DeferredFirstRunCoordinator` presents deferred screens in order,
// progressively removes each screen from `prefs::kFirstRunDeferredScreens` as
// it is shown, avoids `FirstRun.Stage` histograms, and notifies handlers upon
// completion.
TEST_F(DeferredFirstRunCoordinatorTest,
       PresentsDeferredScreensSequentiallyAndClearsPrefProgressively) {
  SetDeferredScreens(
      {ScreenType::kBestFeatures, ScreenType::kLensAnimatedPromo});
  CreateCoordinator();

  [coordinator_ start];
  UINavigationController* presented_nav_controller =
      base::apple::ObjCCast<UINavigationController>(
          base_view_controller_.presentedViewController);
  ASSERT_NE(presented_nav_controller, nil);
  EXPECT_EQ(presented_nav_controller.modalPresentationStyle,
            UIModalPresentationFormSheet);

  // `kBestFeatures` was removed when shown; `kLensAnimatedPromo` remains.
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kLensAnimatedPromo));

  PromoStyleViewController* first_screen_vc =
      base::apple::ObjCCastStrict<PromoStyleViewController>(
          presented_nav_controller.topViewController);
  ASSERT_NE(first_screen_vc, nil);
  EXPECT_FALSE(first_screen_vc.modalInPresentation);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);

  // Completing the first screen advances to `kLensAnimatedPromo` and removes it
  // from `prefs::kFirstRunDeferredScreens`.
  [first_screen_vc.delegate didTapPrimaryActionButton];
  EXPECT_THAT(GetDeferredScreens(), ::testing::IsEmpty());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return presented_nav_controller.topViewController != first_screen_vc;
  }));

  PromoStyleViewController* second_screen_vc =
      base::apple::ObjCCastStrict<PromoStyleViewController>(
          presented_nav_controller.topViewController);
  ASSERT_NE(second_screen_vc, nil);
  EXPECT_FALSE(second_screen_vc.modalInPresentation);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);

  // Completing the final screen notifies `promosUIHandler` and sends
  // `-hideDeferredFirstRunPromo`.
  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  [second_screen_vc.delegate didTapPrimaryActionButton];
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
  histogram_tester_.ExpectTotalCount(first_run::kFirstRunStageHistogram, 0);
}

// Tests that swiping down to dismiss mid-sequence preserves any screens not yet
// shown in `prefs::kFirstRunDeferredScreens` and notifies handlers once.
TEST_F(DeferredFirstRunCoordinatorTest,
       SwipeToDismissPreservesUnshownScreensInPref) {
  SetDeferredScreens(
      {ScreenType::kBestFeatures, ScreenType::kLensAnimatedPromo});
  CreateCoordinator();

  [coordinator_ start];
  UINavigationController* presented_nav_controller =
      base::apple::ObjCCast<UINavigationController>(
          base_view_controller_.presentedViewController);
  ASSERT_NE(presented_nav_controller, nil);
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kLensAnimatedPromo));

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  id<UIAdaptivePresentationControllerDelegate> presentation_delegate =
      static_cast<id<UIAdaptivePresentationControllerDelegate>>(coordinator_);
  [presentation_delegate
      presentationControllerDidDismiss:presented_nav_controller
                                           .presentationController];

  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kLensAnimatedPromo));
}

// Tests that when `prefs::kFirstRunDeferredScreens` is empty, `-start` finishes
// synchronously without presenting `_navigationController`.
TEST_F(DeferredFirstRunCoordinatorTest,
       FinishesSynchronouslyWithoutPresentingWhenPrefEmpty) {
  CreateCoordinator();

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  [coordinator_ start];

  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
}

// Tests that when the only deferred screen (`kHistorySync`) auto-skips
// synchronously during `-start` because the user is signed out, the pref entry
// is removed and the coordinator tears down cleanly without presenting an empty
// navigation controller.
TEST_F(DeferredFirstRunCoordinatorTest,
       FinishesSynchronouslyWhenHistorySyncSkipsWhileSignedOut) {
  SetDeferredScreens({ScreenType::kHistorySync});
  CreateCoordinator();

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  [coordinator_ start];

  EXPECT_THAT(GetDeferredScreens(), ::testing::IsEmpty());
  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
}

// Tests that if `kBestFeatures` was persisted in `LocalState` and its feature
// flag is disabled before `DeferredFirstRunCoordinator` runs, it skips cleanly
// without crashing and removes the stale entry from `LocalState`.
TEST_F(DeferredFirstRunCoordinatorTest,
       SkipsBestFeaturesWhenDisabledAfterDeferral) {
  base::test::ScopedFeatureList disable_best_features;
  disable_best_features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{first_run::kBestFeaturesScreenInFirstRun});

  SetDeferredScreens({ScreenType::kBestFeatures});
  CreateCoordinator();

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  [coordinator_ start];

  EXPECT_THAT(GetDeferredScreens(), ::testing::IsEmpty());
  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
}

// Tests that stopping `DeferredFirstRunCoordinator` externally while a screen
// is active still notifies `promosUIHandler` once.
TEST_F(DeferredFirstRunCoordinatorTest,
       ExternalStopNotifiesPromosUIHandlerOnce) {
  SetDeferredScreens({ScreenType::kBestFeatures});
  CreateCoordinator();

  [coordinator_ start];
  EXPECT_NE(base_view_controller_.presentedViewController, nil);

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  [coordinator_ stop];
  coordinator_ = nil;
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
}

// Tests that non-deferrable `ScreenType` values, out-of-range integers, and
// non-integer entries in `prefs::kFirstRunDeferredScreens` are discarded while
// valid deferrable entries are presented in order.
TEST_F(DeferredFirstRunCoordinatorTest,
       IgnoresNonDeferrableAndMalformedEntries) {
  base::ListValue deferred_list;
  deferred_list.Append(static_cast<int>(ScreenType::kSignIn));
  deferred_list.Append(static_cast<int>(ScreenType::kChoice));
  deferred_list.Append("invalid_entry");
  deferred_list.Append(-1);
  deferred_list.Append(static_cast<int>(ScreenType::kBestFeatures));
  deferred_list.Append(static_cast<int>(ScreenType::kStepsCompleted));
  deferred_list.Append(999);
  GetApplicationContext()->GetLocalState()->SetList(
      prefs::kFirstRunDeferredScreens, std::move(deferred_list));

  CreateCoordinator();
  [coordinator_ start];

  UINavigationController* presented_nav_controller =
      base::apple::ObjCCast<UINavigationController>(
          base_view_controller_.presentedViewController);
  ASSERT_NE(presented_nav_controller, nil);

  PromoStyleViewController* first_screen_vc =
      base::apple::ObjCCastStrict<PromoStyleViewController>(
          presented_nav_controller.topViewController);
  ASSERT_NE(first_screen_vc, nil);

  OCMExpect([mock_promos_ui_handler_ promoWasDismissed]);
  OCMExpect([mock_promos_manager_handler_ hideDeferredFirstRunPromo])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        coordinator_ = nil;
      });

  [first_screen_vc.delegate didTapPrimaryActionButton];
  EXPECT_THAT(GetDeferredScreens(), ::testing::IsEmpty());
  EXPECT_OCMOCK_VERIFY(mock_promos_ui_handler_);
  EXPECT_OCMOCK_VERIFY(mock_promos_manager_handler_);
}

}  // namespace
