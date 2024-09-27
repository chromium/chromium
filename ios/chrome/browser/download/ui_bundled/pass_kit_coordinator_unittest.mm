// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/pass_kit_coordinator.h"

#import <PassKit/PassKit.h>

#import <memory>

#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/fakes/fake_web_content_handler.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test fixture for PassKitCoordinator class.
class PassKitCoordinatorTest : public PlatformTest {
 protected:
  PassKitCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[PassKitCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    auto web_state = std::make_unique<web::FakeWebState>();
    test_navigation_manager_ = std::make_unique<web::FakeNavigationManager>();
    web_state->SetNavigationManager(std::move(test_navigation_manager_));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    web_state_ = browser_->GetWebStateList()->GetActiveWebState();
    handler_ = [[FakeWebContentHandler alloc] init];

    PassKitTabHelper::GetOrCreateForWebState(web_state_)
        ->SetWebContentsHandler(handler_);
    InfoBarManagerImpl::CreateForWebState(web_state_);

    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  ~PassKitCoordinatorTest() override { [coordinator_ stop]; }

  PassKitTabHelper* tab_helper() {
    return PassKitTabHelper::GetOrCreateForWebState(web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  PassKitCoordinator* coordinator_;
  // Weak pointer to the test web state; browser_'s web state list owns it.
  raw_ptr<web::WebState> web_state_;
  FakeWebContentHandler* handler_;
  ScopedKeyWindow scoped_key_window_;
  std::unique_ptr<web::NavigationManager> test_navigation_manager_;
  base::HistogramTester histogram_tester_;
};

// Tests that PassKitCoordinator presents PKAddPassesViewController for the
// valid PKPass object.
TEST_F(PassKitCoordinatorTest, ValidPassKitObject) {
  std::string data = testing::GetTestFileContents(testing::kPkPassFilePath);
  NSData* nsdata = [NSData dataWithBytes:data.c_str() length:data.size()];
  PKPass* pass = [[PKPass alloc] initWithData:nsdata error:nil];
  ASSERT_TRUE(pass);

  coordinator_.passes = @[ pass ];
  [coordinator_ start];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Wallet app is not supported on iPads.
  } else {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return [base_view_controller_.presentedViewController class] ==
             [PKAddPassesViewController class];
    }));

    [coordinator_ stop];

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
      return base_view_controller_.presentedViewController == nil;
    }));

    histogram_tester_.ExpectUniqueSample(
        kUmaPresentAddPassesDialogResult,
        static_cast<base::HistogramBase::Sample>(
            PresentAddPassesDialogResult::kSuccessful),
        1);
  }

  EXPECT_FALSE(coordinator_.passes);
}

// Tests presenting multiple valid PKPass objects.
TEST_F(PassKitCoordinatorTest, MultiplePassKitObjects) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Wallet app is not supported on iPads.
    return;
  }

  std::string data = testing::GetTestFileContents(testing::kPkPassFilePath);
  NSData* nsdata = [NSData dataWithBytes:data.c_str() length:data.size()];
  PKPass* pass = [[PKPass alloc] initWithData:nsdata error:nil];
  ASSERT_TRUE(pass);

  coordinator_.passes = @[ pass ];
  [coordinator_ start];

  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForUIElementTimeout, ^{
        return [base_view_controller_.presentedViewController class] ==
               [PKAddPassesViewController class];
      }));

  histogram_tester_.ExpectUniqueSample(
      kUmaPresentAddPassesDialogResult,
      static_cast<base::HistogramBase::Sample>(
          PresentAddPassesDialogResult::kSuccessful),
      1);

  UIViewController* presented_controller =
      base_view_controller_.presentedViewController;

  coordinator_.passes = @[ pass ];
  [coordinator_ start];

  // New UI presentation is ignored.
  EXPECT_EQ(presented_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectBucketCount(
      kUmaPresentAddPassesDialogResult,
      static_cast<base::HistogramBase::Sample>(
          PresentAddPassesDialogResult::
              kAnotherAddPassesViewControllerIsPresented),
      1);

  // Previously presented view controller can be dismissed.
  [coordinator_ stop];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Tests presenting valid PKPass object, while another view controller is
// already presented.
TEST_F(PassKitCoordinatorTest, AnotherViewControllerIsPresented) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // Wallet app is not supported on iPads.
    return;
  }

  // Present another view controller.
  UIViewController* presented_controller = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:presented_controller
                                      animated:YES
                                    completion:nil];
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForUIElementTimeout, ^{
        return presented_controller ==
               base_view_controller_.presentedViewController;
      }));

  // Attempt to present "Add pkpass UI".
  std::string data = testing::GetTestFileContents(testing::kPkPassFilePath);
  NSData* nsdata = [NSData dataWithBytes:data.c_str() length:data.size()];
  PKPass* pass = [[PKPass alloc] initWithData:nsdata error:nil];
  ASSERT_TRUE(pass);

  coordinator_.passes = @[ pass ];
  [coordinator_ start];

  // New UI presentation is ignored.
  EXPECT_EQ(presented_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectBucketCount(
      kUmaPresentAddPassesDialogResult,
      static_cast<base::HistogramBase::Sample>(
          PresentAddPassesDialogResult::kAnotherViewControllerIsPresented),
      1);
}

// Tests that PassKitCoordinator presents error infobar for invalid PKPass
// object.
TEST_F(PassKitCoordinatorTest, InvalidPassKitObject) {
  coordinator_.passes = nil;
  [coordinator_ start];

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  ASSERT_EQ(1U, infobar_manager->infobars().size());
  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  ASSERT_TRUE(infobar->delegate());
  auto* delegate = infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  DCHECK_EQ(l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR),
            delegate->GetMessageText());
  EXPECT_FALSE(coordinator_.passes);

  histogram_tester_.ExpectTotalCount(kUmaPresentAddPassesDialogResult, 0);
}

// Tests that PassKitCoordinator presents error infobar for invalid PKPass
// object.
TEST_F(PassKitCoordinatorTest, EmptyPassKitObject) {
  coordinator_.passes = @[];
  [coordinator_ start];

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  ASSERT_EQ(1U, infobar_manager->infobars().size());
  infobars::InfoBar* infobar = infobar_manager->infobars()[0];
  ASSERT_TRUE(infobar->delegate());
  auto* delegate = infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  DCHECK_EQ(l10n_util::GetStringUTF16(IDS_IOS_GENERIC_PASSKIT_ERROR),
            delegate->GetMessageText());
  EXPECT_FALSE(coordinator_.passes);

  histogram_tester_.ExpectTotalCount(kUmaPresentAddPassesDialogResult, 0);
}
