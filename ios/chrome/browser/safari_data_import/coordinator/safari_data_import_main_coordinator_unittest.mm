// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Helper acting as the coordinator's delegate and UI handler, mirroring
// `SceneCoordinator`'s ownership of `_safariDataImportCoordinator`.
@interface FakeSafariDataImportCoordinatorOwner
    : NSObject <SafariDataImportMainCoordinatorDelegate,
                SafariDataImportUIHandler>

@property(nonatomic, assign) NSUInteger dismissCallCount;
@property(nonatomic, assign) BOOL stopCoordinatorOnDismiss;
@property(nonatomic, readonly) BOOL hasCoordinator;

// Creates and starts `SafariDataImportMainCoordinator`.
- (void)startWithBaseViewController:(UIViewController*)baseViewController
                            browser:(Browser*)browser
                         entryPoint:(SafariDataImportEntryPoint)entryPoint;

// Triggers the primary action on the entry point screen to present the export
// coordinator.
- (void)triggerPrimaryAction;

// Stops and clears the owned `SafariDataImportMainCoordinator`.
- (void)stopSafariDataImportCoordinator;

@end

@implementation FakeSafariDataImportCoordinatorOwner {
  SafariDataImportMainCoordinator* _safariDataImportCoordinator;
}

#pragma mark - Public

- (BOOL)hasCoordinator {
  return _safariDataImportCoordinator != nil;
}

- (void)startWithBaseViewController:(UIViewController*)baseViewController
                            browser:(Browser*)browser
                         entryPoint:(SafariDataImportEntryPoint)entryPoint {
  _safariDataImportCoordinator = [[SafariDataImportMainCoordinator alloc]
          initFromEntryPoint:entryPoint
      withBaseViewController:baseViewController
                     browser:browser];
  _safariDataImportCoordinator.delegate = self;
  _safariDataImportCoordinator.UIHandler = self;
  [_safariDataImportCoordinator start];
}

- (void)triggerPrimaryAction {
  ASSERT_TRUE([_safariDataImportCoordinator
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);
  [(id<ConfirmationAlertActionHandler>)
          _safariDataImportCoordinator confirmationAlertPrimaryAction];
}

- (void)stopSafariDataImportCoordinator {
  [_safariDataImportCoordinator stop];
  _safariDataImportCoordinator = nil;
}

#pragma mark - SafariDataImportMainCoordinatorDelegate

- (void)safariImportWorkflowDidEndForCoordinator:
    (SafariDataImportMainCoordinator*)coordinator {
  [self stopSafariDataImportCoordinator];
}

#pragma mark - SafariDataImportUIHandler

- (void)safariDataImportDidDismiss {
  self.dismissCallCount++;
  if (self.stopCoordinatorOnDismiss) {
    [self stopSafariDataImportCoordinator];
  }
}

@end

// Test fixture for `SafariDataImportMainCoordinator`.
class SafariDataImportMainCoordinatorTest : public PlatformTest {
 protected:
  SafariDataImportMainCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] init];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
};

// Tests that stopping the coordinator when the export coordinator is active
// tears down child coordinators and delegates before invoking the dismissal
// completion handler, avoiding use-after-free crashes if the coordinator is
// deallocated or re-entrantly stopped during the dismissal callback.
TEST_F(SafariDataImportMainCoordinatorTest,
       StopWithExportCoordinatorHandlesReentrancySafely) {
  if (!@available(iOS 18.2, *)) {
    GTEST_SKIP() << "Safari data import requires iOS 18.2+.";
  }

  FakeSafariDataImportCoordinatorOwner* owner =
      [[FakeSafariDataImportCoordinatorOwner alloc] init];
  owner.stopCoordinatorOnDismiss = YES;

  [owner startWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                          entryPoint:SafariDataImportEntryPoint::kFirstRun];
  [owner triggerPrimaryAction];

  // Stopping the coordinator triggers `safariDataImportDidDismiss`, which
  // re-entrantly calls `stopSafariDataImportCoordinator` and releases
  // `_safariDataImportCoordinator` while the outer `-stop` is on the stack.
  [owner stopSafariDataImportCoordinator];

  EXPECT_EQ(1u, owner.dismissCallCount);
  EXPECT_FALSE(owner.hasCoordinator);
}

// Tests that stopping the coordinator dismisses the presented view controller
// hierarchy cleanly when the export coordinator is presented on top of the
// main entry point navigation controller in a window.
TEST_F(SafariDataImportMainCoordinatorTest,
       StopDismissesPresentedExportCoordinatorHierarchy) {
  if (!@available(iOS 18.2, *)) {
    GTEST_SKIP() << "Safari data import requires iOS 18.2+.";
  }

  UIWindow* window = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  window.rootViewController = base_view_controller_;
  [window makeKeyAndVisible];

  FakeSafariDataImportCoordinatorOwner* owner =
      [[FakeSafariDataImportCoordinatorOwner alloc] init];
  [owner startWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                          entryPoint:SafariDataImportEntryPoint::kSetting];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return base_view_controller_.presentedViewController != nil;
      }));
  UIViewController* main_nav = base_view_controller_.presentedViewController;

  // Present the export coordinator on top of `main_nav`.
  [owner triggerPrimaryAction];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return main_nav.presentedViewController != nil;
      }));

  // Stop the coordinator and verify the entire presented hierarchy dismisses
  // and invokes `safariDataImportDidDismiss`.
  [owner stopSafariDataImportCoordinator];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return base_view_controller_.presentedViewController == nil &&
               owner.dismissCallCount == 1u;
      }));
}
