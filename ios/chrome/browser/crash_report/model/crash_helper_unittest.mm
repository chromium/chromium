// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_helper.h"

#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_report_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

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
  // Clear previous params for testing sync.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in [defaults dictionaryRepresentation].allKeys) {
    if ([key hasPrefix:previous_session_info_constants::
                           kPreviousSessionInfoParamsPrefix]) {
      [defaults removeObjectForKey:key];
    }
  }

  // Test that the serialized dictionary does not exceed the maximum size of a
  // single crash key. This test should include all keys for
  // CrashReportUserApplicationState, since the whole dictionary is considered a
  // single key.
  crash_keys::SetCurrentlyInBackground(true);
  crash_keys::SetCurrentlySignedIn(true);
  crash_keys::SetMemoryWarningCount(2);
  crash_keys::SetMemoryWarningInProgress(true);
  crash_keys::SetCurrentFreeMemoryInKB(1234);
  crash_keys::SetCurrentTabIsPDF(true);
  crash_keys::SetCurrentOrientation(3, 7);
  crash_keys::SetCurrentHorizontalSizeClass(2);
  crash_keys::SetCurrentUserInterfaceStyle(2);
  crash_keys::SetRegularTabCount(999);
  crash_keys::SetInactiveTabCount(999);
  crash_keys::SetIncognitoTabCount(999);
  crash_keys::SetForegroundScenesCount(999);
  crash_keys::SetConnectedScenesCount(999);
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(true);
  crash_keys::SetGridToVisibleTabAnimation(
      @"to_view_controller", @"presenting_view_controller",
      @"presented_view_controller", @"parent_view_controller");
  crash_keys::MediaStreamPlaybackDidStart();
  crash_keys::SetVoiceOverRunning(true);

  // Set a max-length breadcrumbs string.
  std::string breadcrumbs(breadcrumbs::kMaxDataLength, 'A');
  breadcrumbs::BreadcrumbManager::GetInstance().SetPreviousSessionEvents(
      {breadcrumbs});

  // Confirm keys are synced to user defaults for MetricKit report params.
  NSMutableDictionary* reportParameters = [[NSMutableDictionary alloc] init];
  defaults = [NSUserDefaults standardUserDefaults];
  NSUInteger prefix_length =
      previous_session_info_constants::kPreviousSessionInfoParamsPrefix.length;
  for (NSString* key in [defaults dictionaryRepresentation].allKeys) {
    if ([key hasPrefix:previous_session_info_constants::
                           kPreviousSessionInfoParamsPrefix]) {
      NSString* crash_key = [key substringFromIndex:prefix_length];
      reportParameters[crash_key] = [defaults stringForKey:key];
    }
  }
  EXPECT_NSEQ(reportParameters[@"memory_warning_count"], @"2");
  EXPECT_NSEQ(reportParameters[@"crashed_in_background"], @"yes");
  EXPECT_NSEQ(reportParameters[@"free_memory_in_kb"], @"1234");
  EXPECT_NSEQ(reportParameters[@"user_application_state"],
              @"{\"OTRTabs\":999,\"avplay\":1,\"destroyingAndRebuildingOTR\":1,"
              @"\"fgScenes\":999,\"inactiveTabs\":999,\"orient\":37,\"pdf\":1,"
              @"\"regTabs\":999,\"scenes\":999,\"signIn\":1,\"sizeclass\":2,"
              @"\"user_interface_style\":2,\"voiceOver\":1}");
  EXPECT_NSEQ(reportParameters[@"memory_warning_in_progress"], @"yes");
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
