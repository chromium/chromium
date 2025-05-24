// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/set_up_list_default_browser_promo_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ios::kWaitForUIElementTimeout;
using set_up_list_prefs::SetUpListItemState;

// Tests the SetUpListView and subviews.
class SetUpListDefaultBrowserPromoCoordinatorTest : public PlatformTest {
 public:
  SetUpListDefaultBrowserPromoCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    window_ = [[UIWindow alloc] init];
    window_.rootViewController = [[UIViewController alloc] init];
    [window_ addSubview:window_.rootViewController.view];
    UIView.animationsEnabled = NO;
    mock_application_ = OCMStrictClassMock([UIApplication class]);
    coordinator_ = [[SetUpListDefaultBrowserPromoCoordinator alloc]
        initWithBaseViewController:window_.rootViewController
                           browser:browser_.get()
                       application:mock_application_];
    delegate_ = OCMProtocolMock(
        @protocol(SetUpListDefaultBrowserPromoCoordinatorDelegate));
    coordinator_.delegate = delegate_;
  }

  PrefService* GetLocalState() {
    return GetApplicationContext()->GetLocalState();
  }

  void WaitForItemCompletePref() {
    base::test::ScopedRunLoopTimeout scoped_timeout(FROM_HERE,
                                                    kWaitForUIElementTimeout);
    const char* pref_name =
        set_up_list_prefs::PrefNameForItem(SetUpListItemType::kDefaultBrowser);
    base::RunLoop wait_for_pref;
    PrefChangeRegistrar pref_registrar;
    pref_registrar.Init(GetLocalState());
    pref_registrar.Add(pref_name, wait_for_pref.QuitClosure());
    wait_for_pref.Run();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIWindow* window_;
  SetUpListDefaultBrowserPromoCoordinator* coordinator_;
  id delegate_;
  id mock_application_;
};

#pragma mark - Tests

// Test that touching the primary button calls the correct delegate method
// and opens the settings.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, PrimaryButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:YES]);
  OCMExpect([mock_application_
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil]);
  [coordinator_ didTapPrimaryActionButton];
  [delegate_ verify];

  [coordinator_ stop];
  WaitForItemCompletePref();

  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.SetUpList.Action",
      IOSDefaultBrowserPromoAction::kActionButton, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Appear"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Accepted"));
}

// Test that touching the secondary button calls the correct delegate method.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, SecondaryButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:NO]);
  [coordinator_ didTapSecondaryActionButton];
  [delegate_ verify];

  [coordinator_ stop];
  WaitForItemCompletePref();
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.SetUpList.Action",
      IOSDefaultBrowserPromoAction::kCancel, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Appear"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
}

// Test that touching the secondary button calls the correct delegate method.
TEST_F(SetUpListDefaultBrowserPromoCoordinatorTest, SwipeToDismiss) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  [coordinator_ start];

  OCMExpect([delegate_ setUpListDefaultBrowserPromoDidFinish:NO]);
  UIPresentationController* presentation_controller =
      window_.rootViewController.presentedViewController.presentationController;
  [coordinator_ presentationControllerDidDismiss:presentation_controller];
  [delegate_ verify];

  [coordinator_ stop];
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "IOS.DefaultBrowserPromo.SetUpList.Action",
      IOSDefaultBrowserPromoAction::kCancel, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Appear"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
}
