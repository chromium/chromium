// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/ar_quick_look_coordinator.h"

#import <QuickLook/QuickLook.h>

#import <memory>

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Returns the absolute path for the test file in the test data directory.
base::FilePath GetTestFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_ASSETS, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL(testing::kUsdzFilePath));
  return file_path;
}

}  // namespace

class ARQuickLookCoordinatorTest : public PlatformTest {
 protected:
  ARQuickLookCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[ARQuickLookCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    // The Coordinator should install itself as delegate for the existing
    // ARQuickLookTabHelper instances once started.
    auto web_state = std::make_unique<web::FakeWebState>();
    auto* web_state_ptr = web_state.get();
    ARQuickLookTabHelper::GetOrCreateForWebState(web_state_ptr);
    browser_->GetWebStateList()->InsertWebState(std::move(web_state));
    [coordinator_ start];
  }

  ~ARQuickLookCoordinatorTest() override { [coordinator_ stop]; }

  web::WebState* web_state() {
    DCHECK_GE(browser_->GetWebStateList()->count(), 1);
    return browser_->GetWebStateList()->GetWebStateAt(0);
  }

  ARQuickLookTabHelper* tab_helper() {
    return ARQuickLookTabHelper::GetOrCreateForWebState(web_state());
  }

  // Needed for test profile created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  ARQuickLookCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::HistogramTester histogram_tester_;
};

// Tests that the coordinator installs itself as a ARQuickLookTabHelper
// delegate when ARQuickLookTabHelper instances become available.
TEST_F(ARQuickLookCoordinatorTest, InstallDelegates) {
  // Coordinator should install itself as delegate for a new web state.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr2 = web_state2.get();
  EXPECT_FALSE(
      ARQuickLookTabHelper::GetOrCreateForWebState(web_state_ptr2)->delegate());
  browser_->GetWebStateList()->InsertWebState(std::move(web_state2));
  EXPECT_TRUE(
      ARQuickLookTabHelper::GetOrCreateForWebState(web_state_ptr2)->delegate());

  // Coordinator should install itself as delegate for a web state replacing an
  // existing one.
  auto web_state3 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr3 = web_state3.get();
  EXPECT_FALSE(
      ARQuickLookTabHelper::GetOrCreateForWebState(web_state_ptr3)->delegate());
  browser_->GetWebStateList()->ReplaceWebStateAt(0, std::move(web_state3));
  EXPECT_TRUE(
      ARQuickLookTabHelper::GetOrCreateForWebState(web_state_ptr3)->delegate());
}

// Tests presenting a valid USDZ file.
TEST_F(ARQuickLookCoordinatorTest, ValidUSDZFile) {
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  [tab_helper()->delegate() presentUSDZFileWithURL:fileURL
                                      canonicalURL:nil
                                          webState:web_state()
                               allowContentScaling:YES];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kSuccessful),
      1);
}

// Tests attempting to present an invalid USDZ file.
TEST_F(ARQuickLookCoordinatorTest, InvalidUSDZFile) {
  [tab_helper()->delegate() presentUSDZFileWithURL:nil
                                      canonicalURL:nil
                                          webState:web_state()
                               allowContentScaling:YES];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kInvalidFile),
      1);
}

// Tests presenting multiple USDZ files. The subsequent attempts get ignored.
TEST_F(ARQuickLookCoordinatorTest, MultipleValidUSDZFiles) {
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  [tab_helper()->delegate() presentUSDZFileWithURL:fileURL
                                      canonicalURL:nil
                                          webState:web_state()
                               allowContentScaling:YES];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kSuccessful),
      1);

  // Attempt to present QLPreviewController again.
  UIViewController* presented_view_controller =
      base_view_controller_.presentedViewController;

  [tab_helper()->delegate() presentUSDZFileWithURL:fileURL
                                      canonicalURL:nil
                                          webState:web_state()
                               allowContentScaling:YES];

  // The attempt is ignored.
  EXPECT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectBucketCount(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kAnotherQLPreviewControllerIsPresented),
      1);
}

// Tests presenting a valid USDZ file while a view controller is presented.
TEST_F(ARQuickLookCoordinatorTest, AnotherViewControllerIsPresented) {
  // Present a view controller.
  UIViewController* presented_view_controller = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:presented_view_controller
                                      animated:YES
                                    completion:nil];
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForUIElementTimeout, ^{
        return presented_view_controller ==
               base_view_controller_.presentedViewController;
      }));

  // Attempt to present QLPreviewController.
  base::FilePath path = GetTestFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  [tab_helper()->delegate() presentUSDZFileWithURL:fileURL
                                      canonicalURL:nil
                                          webState:web_state()
                               allowContentScaling:YES];

  // The attempt is ignored.
  EXPECT_EQ(presented_view_controller,
            base_view_controller_.presentedViewController);

  histogram_tester_.ExpectUniqueSample(
      kIOSPresentQLPreviewControllerHistogram,
      static_cast<base::HistogramBase::Sample>(
          PresentQLPreviewController::kAnotherViewControllerIsPresented),
      1);
}
