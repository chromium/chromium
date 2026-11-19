// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_coordinator_delegate.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_commands.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_mutator.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test fixture for `AutoDeletionCoordinator`.
class AutoDeletionCoordinatorTest : public PlatformTest {
 protected:
  AutoDeletionCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    base_view_controller_ = [[UIViewController alloc] init];
    scoped_key_window_.Get().rootViewController = base_view_controller_;
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(AutoDeletionCoordinatorDelegate));
  }

  ~AutoDeletionCoordinatorTest() override {
    @autoreleasepool {
      [coordinator_ stop];
      coordinator_ = nil;
    }
    // Lets any modal dismissal triggered by `-stop` finish before the key
    // window is torn down.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return base_view_controller_.presentedViewController == nil;
    }));
    EXPECT_OCMOCK_VERIFY(delegate_mock_);
  }

  // Returns the testing local state `PrefService`.
  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

  // Creates a coordinator backed by a regular or incognito `TestBrowser` owned
  // by this fixture so that `browser_` outlives `coordinator_`. The coordinator
  // presents from `base_view_controller` if non-nil, and from
  // `base_view_controller_` otherwise.
  AutoDeletionCoordinator* CreateCoordinator(
      bool incognito = false,
      UIViewController* base_view_controller = nil) {
    ProfileIOS* profile =
        incognito ? profile_->GetOffTheRecordProfile() : profile_.get();
    browser_ = std::make_unique<TestBrowser>(profile);
    coordinator_ = [[AutoDeletionCoordinator alloc]
        initWithBaseViewController:base_view_controller ?: base_view_controller_
                           browser:browser_.get()];
    coordinator_.delegate = delegate_mock_;
    return coordinator_;
  }

  // Waits until `base_view_controller_` presents a view controller of class
  // `view_controller_class` and its presentation transition has finished.
  // Returns whether this happened before the timeout.
  bool WaitForPresentedViewControllerOfClass(Class view_controller_class) {
    return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      UIViewController* presented_view_controller =
          base_view_controller_.presentedViewController;
      return [presented_view_controller isKindOfClass:view_controller_class] &&
             !presented_view_controller.beingPresented;
    });
  }

  // Starts an incognito coordinator, which always satisfies the promo
  // eligibility check, and returns the promo view controller once its sheet is
  // fully presented. Returns nil if the promo wasn't presented in time.
  AutoDeletionPromoViewController* StartCoordinatorPresentingPromo() {
    local_state()->SetBoolean(prefs::kDownloadAutoDeletionEnabled, false);
    local_state()->SetBoolean(prefs::kDownloadAutoDeletionIPHShown, false);
    [CreateCoordinator(/*incognito=*/true) start];
    if (!WaitForPresentedViewControllerOfClass(
            [UINavigationController class])) {
      return nil;
    }
    UINavigationController* navigation_controller =
        base::apple::ObjCCastStrict<UINavigationController>(
            base_view_controller_.presentedViewController);
    return base::apple::ObjCCastStrict<AutoDeletionPromoViewController>(
        navigation_controller.topViewController);
  }

  // Declaration order matters: `task_environment_` and `profile_` / `browser_`
  // must outlive `coordinator_`.
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  id delegate_mock_;
  AutoDeletionCoordinator* coordinator_;
};

// Tests that starting the coordinator when auto-deletion is enabled presents
// the enrollment action sheet.
TEST_F(AutoDeletionCoordinatorTest, PresentsActionSheetWhenEnabled) {
  local_state()->SetBoolean(prefs::kDownloadAutoDeletionEnabled, true);
  AutoDeletionCoordinator* coordinator = CreateCoordinator();

  [coordinator start];

  EXPECT_TRUE(WaitForPresentedViewControllerOfClass([UIAlertController class]));
}

// Tests that the coordinator notifies the delegate to stop it when the action
// sheet can't be presented because the base view controller isn't in a window.
TEST_F(AutoDeletionCoordinatorTest, StopsWhenActionSheetCannotBePresented) {
  local_state()->SetBoolean(prefs::kDownloadAutoDeletionEnabled, true);
  UIViewController* detached_view_controller = [[UIViewController alloc] init];
  AutoDeletionCoordinator* coordinator = CreateCoordinator(
      /*incognito=*/false, detached_view_controller);

  OCMExpect(
      [delegate_mock_ autoDeletionCoordinatorWantsToBeStopped:coordinator]);
  [coordinator start];
  EXPECT_EQ(nil, detached_view_controller.presentedViewController);
}

// Tests that starting the coordinator when auto-deletion is disabled and the
// promo has already been shown notifies the delegate to stop immediately.
TEST_F(AutoDeletionCoordinatorTest, StopsImmediatelyWhenDisabledAndPromoShown) {
  local_state()->SetBoolean(prefs::kDownloadAutoDeletionEnabled, false);
  local_state()->SetBoolean(prefs::kDownloadAutoDeletionIPHShown, true);
  AutoDeletionCoordinator* coordinator = CreateCoordinator();

  OCMExpect(
      [delegate_mock_ autoDeletionCoordinatorWantsToBeStopped:coordinator]);
  [coordinator start];
  EXPECT_EQ(nil, base_view_controller_.presentedViewController);
}

// Tests that enabling auto-deletion from the promo dismisses the promo and
// presents the enrollment action sheet.
TEST_F(AutoDeletionCoordinatorTest, EnablingFromPromoTransitionsToActionSheet) {
  AutoDeletionPromoViewController* promo_view_controller =
      StartCoordinatorPresentingPromo();
  ASSERT_NE(nil, promo_view_controller);
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kDownloadAutoDeletionIPHShown));

  [promo_view_controller.mutator enableAutoDeletion];
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kDownloadAutoDeletionEnabled));

  EXPECT_TRUE(WaitForPresentedViewControllerOfClass([UIAlertController class]));
}

// Tests that dismissing the promo notifies the delegate to stop the
// coordinator.
TEST_F(AutoDeletionCoordinatorTest, DismissingPromoStopsCoordinator) {
  AutoDeletionPromoViewController* promo_view_controller =
      StartCoordinatorPresentingPromo();
  ASSERT_NE(nil, promo_view_controller);

  OCMExpect(
      [delegate_mock_ autoDeletionCoordinatorWantsToBeStopped:coordinator_]);
  [promo_view_controller.autoDeletionPromoHandler dismissAutoDeletion];
}

// Tests that swiping down the promo sheet notifies the delegate to stop the
// coordinator.
TEST_F(AutoDeletionCoordinatorTest, SwipingDownPromoStopsCoordinator) {
  AutoDeletionPromoViewController* promo_view_controller =
      StartCoordinatorPresentingPromo();
  ASSERT_NE(nil, promo_view_controller);
  UIPresentationController* presentation_controller =
      promo_view_controller.navigationController.presentationController;
  ASSERT_NE(nil, presentation_controller.delegate);

  OCMExpect(
      [delegate_mock_ autoDeletionCoordinatorWantsToBeStopped:coordinator_]);
  [presentation_controller.delegate
      presentationControllerDidDismiss:presentation_controller];
}
