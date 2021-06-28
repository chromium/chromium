// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/mobileconfig_coordinator.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/download/download_test_util.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper_delegate.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/download/features.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Returns the absolute path for the test file in the test data directory.
base::FilePath GetTestFilePath() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_MODULE, &file_path);
  file_path =
      file_path.Append(FILE_PATH_LITERAL(testing::kMobileConfigFilePath));
  return file_path;
}

class MobileConfigCoordinatorTest : public PlatformTest {
 protected:
  MobileConfigCoordinatorTest()
      : base_view_controller_([[UIViewController alloc] init]),
        browser_(std::make_unique<TestBrowser>()),
        coordinator_([[MobileConfigCoordinator alloc]
            initWithBaseViewController:base_view_controller_
                               browser:browser_.get()]) {
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    feature_list_.InitAndEnableFeature(kDownloadMobileConfigFile);

    // The Coordinator should install itself as delegate for the existing
    // MobileConfigTabHelper instances once started.
    auto web_state = std::make_unique<web::FakeWebState>();
    auto* web_state_ptr = web_state.get();
    MobileConfigTabHelper::CreateForWebState(web_state_ptr);
    browser_->GetWebStateList()->InsertWebState(0, std::move(web_state),
                                                WebStateList::INSERT_NO_FLAGS,
                                                WebStateOpener());
    [coordinator_ start];
  }

  ~MobileConfigCoordinatorTest() override { [coordinator_ stop]; }

  MobileConfigTabHelper* tab_helper() {
    return MobileConfigTabHelper::FromWebState(
        browser_->GetWebStateList()->GetWebStateAt(0));
  }

  // Needed for test browser state created by TestBrowser().
  base::test::TaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;
  UIViewController* base_view_controller_;
  std::unique_ptr<Browser> browser_;
  MobileConfigCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::HistogramTester histogram_tester_;
};

// Tests that the coordinator installs itself as a MobileConfigTabHelper
// delegate when MobileConfigTabHelper instances become available.
TEST_F(MobileConfigCoordinatorTest, InstallDelegates) {
  // Coordinator should install itself as delegate for a new web state.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr2 = web_state2.get();
  MobileConfigTabHelper::CreateForWebState(web_state_ptr2);
  EXPECT_FALSE(MobileConfigTabHelper::FromWebState(web_state_ptr2)->delegate());
  browser_->GetWebStateList()->InsertWebState(0, std::move(web_state2),
                                              WebStateList::INSERT_NO_FLAGS,
                                              WebStateOpener());
  EXPECT_TRUE(MobileConfigTabHelper::FromWebState(web_state_ptr2)->delegate());

  // Coordinator should install itself as delegate for a web state replacing an
  // existing one.
  auto web_state3 = std::make_unique<web::FakeWebState>();
  auto* web_state_ptr3 = web_state3.get();
  MobileConfigTabHelper::CreateForWebState(web_state_ptr3);
  EXPECT_FALSE(MobileConfigTabHelper::FromWebState(web_state_ptr3)->delegate());
  browser_->GetWebStateList()->ReplaceWebStateAt(0, std::move(web_state3));
  EXPECT_TRUE(MobileConfigTabHelper::FromWebState(web_state_ptr3)->delegate());
}

// Tests presenting an UI alert before downloading a valid .mobileconfig file.
TEST_F(MobileConfigCoordinatorTest, ValidMobileConfigFile) {
  base::FilePath path = GetTestFilePath();
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
          DownloadMobileConfigFileUI::KWarningAlertIsPresented),
      1);
}

// Tests attempting to download an invalid .mobileconfig file
TEST_F(MobileConfigCoordinatorTest, InvalidMobileConfigFile) {
  [tab_helper()->delegate() presentMobileConfigAlertFromURL:nil];

  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [UIAlertController class];
  }));

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadMobileConfigFileUI,
      static_cast<base::HistogramBase::Sample>(
          DownloadMobileConfigFileUI::KWarningAlertIsPresented),
      0);
}

}  // namespace
