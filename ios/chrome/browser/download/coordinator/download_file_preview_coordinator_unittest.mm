// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_file_preview_coordinator.h"

#import <QuickLook/QuickLook.h>

#import <memory>

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Test file name for creating temporary files.
const char kTestFileName[] = "test_document.pdf";
const char kTestFileContent[] = "Test PDF content for preview";

// Returns a temporary file path for testing.
base::FilePath GetTestFilePath() {
  base::FilePath temp_dir;
  base::GetTempDir(&temp_dir);
  return temp_dir.AppendASCII(kTestFileName);
}

// Creates a test file at the given path.
bool CreateTestFile(const base::FilePath& file_path) {
  std::string content(kTestFileContent);
  return base::WriteFile(file_path, content);
}

}  // namespace

class DownloadFilePreviewCoordinatorTest : public PlatformTest {
 protected:
  DownloadFilePreviewCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    coordinator_ = [[DownloadFilePreviewCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  ~DownloadFilePreviewCoordinatorTest() override {
    [coordinator_ stop];

    // Clean up test file if it exists.
    if (!test_file_path_.empty()) {
      base::DeleteFile(test_file_path_);
    }
  }

  // Creates a test file and returns its URL.
  NSURL* CreateTestFileURL() {
    test_file_path_ = GetTestFilePath();
    if (!CreateTestFile(test_file_path_)) {
      return nil;
    }
    NSString* path = base::SysUTF8ToNSString(test_file_path_.value());
    return [NSURL fileURLWithPath:path];
  }

  // Needed for test profile created by TestBrowser().
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  DownloadFilePreviewCoordinator* coordinator_;
  ScopedKeyWindow scoped_key_window_;
  base::FilePath test_file_path_;
};

// Tests that coordinator conforms to required protocols.
TEST_F(DownloadFilePreviewCoordinatorTest, ProtocolConformance) {
  EXPECT_TRUE([coordinator_
      conformsToProtocol:@protocol(QLPreviewControllerDataSource)]);
  EXPECT_TRUE(
      [coordinator_ conformsToProtocol:@protocol(QLPreviewControllerDelegate)]);
}

// Tests coordinator initialization and basic properties.
TEST_F(DownloadFilePreviewCoordinatorTest, Initialization) {
  EXPECT_TRUE(coordinator_);
  EXPECT_EQ(base_view_controller_, coordinator_.baseViewController);
  EXPECT_EQ(browser_.get(), coordinator_.browser);
}

// Tests presenting a valid file URL.
TEST_F(DownloadFilePreviewCoordinatorTest, PresentValidFileURL) {
  NSURL* file_url = CreateTestFileURL();
  ASSERT_TRUE(file_url);

  // Present the file.
  [coordinator_ presentFilePreviewWithURL:file_url];

  // Wait for QLPreviewController to be presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));
}

// Tests stop method properly cleans up.
TEST_F(DownloadFilePreviewCoordinatorTest, Stop) {
  NSURL* file_url = CreateTestFileURL();
  ASSERT_TRUE(file_url);

  // Present the file.
  [coordinator_ presentFilePreviewWithURL:file_url];

  // Wait for QLPreviewController to be presented.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  // Stop should dismiss the preview.
  [coordinator_ stop];

  // Wait for dismissal.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Tests coordinator behavior with multiple method calls.
TEST_F(DownloadFilePreviewCoordinatorTest, MultipleMethodCalls) {
  NSURL* file_url = CreateTestFileURL();
  ASSERT_TRUE(file_url);

  // First call should present.
  [coordinator_ presentFilePreviewWithURL:file_url];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  UIViewController* first_presented =
      base_view_controller_.presentedViewController;

  // Second call should reuse the same controller and reload data.
  [coordinator_ presentFilePreviewWithURL:file_url];

  // Wait for potential change.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.1));

  // Should still have the same QLPreviewController presented.
  EXPECT_EQ(first_presented, base_view_controller_.presentedViewController);
  EXPECT_TRUE([base_view_controller_.presentedViewController class] ==
              [QLPreviewController class]);

  // Stop should clean up properly.
  [coordinator_ stop];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}

// Tests that coordinator handles start/stop lifecycle properly.
TEST_F(DownloadFilePreviewCoordinatorTest, StartStopLifecycle) {
  [coordinator_ start];

  NSURL* file_url = CreateTestFileURL();
  ASSERT_TRUE(file_url);

  // Present file after starting.
  [coordinator_ presentFilePreviewWithURL:file_url];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return [base_view_controller_.presentedViewController class] ==
           [QLPreviewController class];
  }));

  [coordinator_ stop];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return base_view_controller_.presentedViewController == nil;
  }));
}
