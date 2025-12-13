// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/coordinator/data_controls_dialog_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

inline constexpr std::string_view kOrganizationDomain = "google.com";

// Helper to find a UIAlertAction by title.
UIAlertAction* GetActionWithTitle(UIAlertController* alert_controller,
                                  NSString* title) {
  for (UIAlertAction* action in alert_controller.actions) {
    if ([action.title isEqualToString:title]) {
      return action;
    }
  }
  return nil;
}

// Test fixture for DataControlsDialogCoordinator.
class DataControlsDialogCoordinatorTest : public PlatformTest {
 public:
  DataControlsDialogCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    original_root_view_controller_ = GetAnyKeyWindow().rootViewController;
    GetAnyKeyWindow().rootViewController = base_view_controller_;
  }

  void TearDown() override {
    GetAnyKeyWindow().rootViewController = original_root_view_controller_;
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  UIViewController* base_view_controller_;
  UIViewController* original_root_view_controller_;
};

// Tests that the coordinator is initialized correctly.
TEST_F(DataControlsDialogCoordinatorTest, Initialization) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  EXPECT_TRUE(coordinator);
  [coordinator stop];
  EXPECT_FALSE(future.Get());
}

// Tests that the start method presents a UIAlertController.
TEST_F(DataControlsDialogCoordinatorTest, StartPresentsAlert) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  [coordinator start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));
  [coordinator stop];
  EXPECT_FALSE(future.Get());
}

// Tests that the presented UIAlertController has the correct contents.
TEST_F(DataControlsDialogCoordinatorTest, AlertContents) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  [coordinator start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));
  UIAlertController* alert = static_cast<UIAlertController*>(
      base_view_controller_.presentedViewController);
  data_controls::WarningDialog dialog = data_controls::GetWarningDialog(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn,
      kOrganizationDomain);
  EXPECT_NSEQ(alert.title, dialog.title);
  EXPECT_NSEQ(alert.message, dialog.label);
  EXPECT_EQ(alert.actions.count, 2u);

  UIAlertAction* cancel_action =
      GetActionWithTitle(alert, dialog.cancel_button_id);
  EXPECT_TRUE(cancel_action);
  EXPECT_EQ(cancel_action.style, UIAlertActionStyleDefault);

  UIAlertAction* ok_action = GetActionWithTitle(alert, dialog.ok_button_id);
  EXPECT_TRUE(ok_action);
  EXPECT_EQ(ok_action.style, UIAlertActionStyleDefault);

  EXPECT_EQ(alert.preferredAction, cancel_action);

  [coordinator stop];
  EXPECT_FALSE(future.Get());
}

// Tests that tapping the cancel button invokes the callback with false.
TEST_F(DataControlsDialogCoordinatorTest, CancelButton) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  [coordinator start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));
  UIAlertController* alert = static_cast<UIAlertController*>(
      base_view_controller_.presentedViewController);
  data_controls::WarningDialog dialog = data_controls::GetWarningDialog(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn,
      kOrganizationDomain);
  UIAlertAction* cancel_action =
      GetActionWithTitle(alert, dialog.cancel_button_id);

  void (^handler)(UIAlertAction*) = [cancel_action valueForKey:@"handler"];
  handler(cancel_action);

  EXPECT_FALSE(future.Get());
}

// Tests that tapping the OK button invokes the callback with true.
TEST_F(DataControlsDialogCoordinatorTest, OkButton) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  [coordinator start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));
  UIAlertController* alert = static_cast<UIAlertController*>(
      base_view_controller_.presentedViewController);
  data_controls::WarningDialog dialog = data_controls::GetWarningDialog(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn,
      kOrganizationDomain);
  UIAlertAction* ok_action = GetActionWithTitle(alert, dialog.ok_button_id);

  void (^handler)(UIAlertAction*) = [ok_action valueForKey:@"handler"];
  handler(ok_action);

  EXPECT_TRUE(future.Get());
}

// Tests that calling stop invokes the callback with false and dismisses the
// alert.
TEST_F(DataControlsDialogCoordinatorTest, Stop) {
  base::test::TestFuture<bool> future;
  auto coordinator = [[DataControlsDialogCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                      dialogType:data_controls::DataControlsDialog::Type::
                                     kClipboardCopyWarn
              organizationDomain:kOrganizationDomain
                        callback:future.GetCallback()];
  [coordinator start];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return base_view_controller_.presentedViewController != nil;
      }));

  [coordinator stop];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return !base_view_controller_.presentedViewController;
      }));
  EXPECT_FALSE(future.Get());
}

}  // namespace
