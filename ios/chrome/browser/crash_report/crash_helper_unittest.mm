// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_helper.h"

#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kUploadedInRecoveryMode = @"uploaded_in_recovery_mode";

class CrashHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    // Ensure the CrashReporterBreadcrumbObserver singleton is created
    // and registered.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();
    crash_reporter::SetCrashpadRunning(true);
  }

  void TearDown() override {
    crash_reporter::SetCrashpadRunning(false);
    crash_helper::SetEnabled(false);
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment;
};

TEST_F(CrashHelperTest, CrashReportUserApplicationStateAllKeys) {
  // Test that the serialized dictionary does not exceed the maximum size of a
  // single crash key. This test should include all keys for
  // CrashReportUserApplicationState, since the whole dictionary is considered a
  // single key.
  crash_keys::SetCurrentlyInBackground(true);
  crash_keys::SetCurrentlySignedIn(true);
  crash_keys::SetMemoryWarningCount(2);
  crash_keys::SetMemoryWarningInProgress(true);
  crash_keys::SetCurrentFreeMemoryInKB(1234);
  crash_keys::SetCurrentFreeDiskInKB(12345);
  crash_keys::SetCurrentTabIsPDF(true);
  crash_keys::SetCurrentOrientation(3, 7);
  crash_keys::SetCurrentHorizontalSizeClass(2);
  crash_keys::SetCurrentUserInterfaceStyle(2);
  crash_keys::SetRegularTabCount(999);
  crash_keys::SetIncognitoTabCount(999);
  crash_keys::SetForegroundScenesCount(999);
  crash_keys::SetConnectedScenesCount(999);
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(true);
  crash_keys::SetGridToVisibleTabAnimation(
      @"to_view_controller", @"presenting_view_controller",
      @"presented_view_controller", @"parent_view_controller");
  crash_keys::MediaStreamPlaybackDidStart();

  // Set a max-length breadcrumbs string.
  std::string breadcrumbs(breadcrumbs::kMaxDataLength, 'A');
  breadcrumbs::BreadcrumbManager::GetInstance().SetPreviousSessionEvents(
      {breadcrumbs});
}

TEST_F(CrashHelperTest, IsUploadingEnabled) {
  crash_helper::common::SetUserEnabledUploading(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());
  crash_helper::SetEnabled(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  crash_helper::SetEnabled(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());

  crash_helper::common::SetUserEnabledUploading(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  crash_helper::SetEnabled(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  crash_helper::SetEnabled(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());
}

}  // namespace
