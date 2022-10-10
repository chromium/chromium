// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/safari_download_coordinator.h"

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/download/download_test_util.h"
#import "ios/chrome/browser/download/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Returns the absolute path for the .mobileconfig file in the test data
// directory.
base::FilePath GetMobileConfigFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_MODULE, &file_path);
  file_path =
      file_path.Append(FILE_PATH_LITERAL(testing::kMobileConfigFilePath));
  return file_path;
}

// Returns the absolute path for the .ics file in the test data directory.
base::FilePath GetCalendarFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_MODULE, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL(testing::kCalendarFilePath));
  return file_path;
}

class SafariDownloadCoordinatorTest : public PlatformTest {
 protected:
  SafariDownloadCoordinatorTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[SafariDownloadCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    // The Coordinator should install itself as delegate for the existing
    // SafariDownloadTabHelper instances once started.
    auto web_state = std::make_unique<web::FakeWebState>();
    auto* web_state_ptr = web_state.get();
    SafariDownloadTabHelper::CreateForWebState(web_state_ptr);
    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_NO_FLAGS,
                                                WebStateOpener());
    [coordinator_ start];
  }

  ~SafariDownloadCoordinatorTest() override { [coordinator_ stop]; }

  SafariDownloadTabHelper* tab_helper() {
    return SafariDownloadTabHelper::FromWebState(
        browser_->GetWebStateList()->GetWebStateAt(0));
  }

  // Needed for test browser state created by TestBrowser().
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  SafariDownloadCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::HistogramTester histogram_tester_;
};

// Tests that the coordinator installs itself as a SafariDownloadTabHelper
// delegate when SafariDownloadTabHelper instances become available.
TEST_F(SafariDownloadCoordinatorTest, InstallDelegates) {
  // Coordinator should install itself as delegate for a new web state.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr2 = web_state2.get();
  SafariDownloadTabHelper::CreateForWebState(web_state_ptr2);
  EXPECT_FALSE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr2)->delegate());
  browser_->GetWebStateList()->InsertWebState(0, std::move(web_state2),
                                              WebStateList::INSERT_NO_FLAGS,
                                              WebStateOpener());
  EXPECT_TRUE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr2)->delegate());

  // Coordinator should install itself as delegate for a web state replacing an
  // existing one.
  auto web_state3 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr3 = web_state3.get();
  SafariDownloadTabHelper::CreateForWebState(web_state_ptr3);
  EXPECT_FALSE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr3)->delegate());
  browser_->GetWebStateList()->ReplaceWebStateAt(0, std::move(web_state3));
  EXPECT_TRUE(
      SafariDownloadTabHelper::FromWebState(web_state_ptr3)->delegate());
}

// Tests presenting an UI alert before downloading a valid .mobileconfig file.
TEST_F(SafariDownloadCoordinatorTest, ValidMobileConfigFile) {
  base::FilePath path = GetMobileConfigFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  [tab_helper()->delegate() presentMobileConfigAlertFromURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadMobileConfigFileUI,
      static_cast<base::HistogramBase::Sample>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .mobileconfig file.
TEST_F(SafariDownloadCoordinatorTest, InvalidMobileConfigFile) {
  [tab_helper()->delegate() presentMobileConfigAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadMobileConfigFileUI,
      static_cast<base::HistogramBase::Sample>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      0);
}

// Tests presenting an UI alert before downloading a valid .ics file.
TEST_F(SafariDownloadCoordinatorTest, ValidCalendarFile) {
  base::FilePath path = GetCalendarFilePath();
  NSURL* fileURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];

  [tab_helper()->delegate() presentCalendarAlertFromURL:fileURL];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadCalendarFileUI,
      static_cast<base::HistogramBase::Sample>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .ics file.
TEST_F(SafariDownloadCoordinatorTest, InvalidCalendarFile) {
  [tab_helper()->delegate() presentCalendarAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadCalendarFileUI,
      static_cast<base::HistogramBase::Sample>(
          SafariDownloadFileUI::kWarningAlertIsPresented),
      0);
}

}  // namespace
