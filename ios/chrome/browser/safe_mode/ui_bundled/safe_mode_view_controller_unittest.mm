// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_view_controller.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/crashpad/crashpad/client/crash_report_database.h"
#import "third_party/crashpad/crashpad/client/crashpad_client.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class SafeModeViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_FALSE(crash_reporter::internal::GetCrashReportDatabase());
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    database_dir_path_ = database_dir_.GetPath();
    ASSERT_TRUE(client_.StartCrashpadInProcessHandler(
        database_dir_path_, "", {},
        crashpad::CrashpadClient::ProcessPendingReportsObservationCallback()));
    database_ = crashpad::CrashReportDatabase::Initialize(database_dir_path_);
    crash_reporter::internal::SetCrashReportDatabaseForTesting(
        database_.get(), &database_dir_path_);

    crash_reporter::SetCrashpadRunning(true);
  }

  void TearDown() override {
    client_.ResetForTesting();
    crash_reporter::SetCrashpadRunning(false);
    crash_reporter::internal::SetCrashReportDatabaseForTesting(nullptr,
                                                               nullptr);
    PlatformTest::TearDown();
  }

 protected:
  crashpad::CrashpadClient client_;
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
  base::FilePath database_dir_path_;
  base::ScopedTempDir database_dir_;
  base::test::TaskEnvironment task_environment;
};

// Verify that +[SafeModeViewController hasSuggestions] returns YES if and only
// if crash reporting is enabled by the user and there are multiple crash
// reports to upload. +[SafeModeViewController hasSuggestions] does not depend
// on the value of crash_helper::IsEnabled or
// crash_helper::IsUploadingEnabled.
TEST_F(SafeModeViewControllerTest, HasSuggestions) {
  crash_helper::common::SetUserEnabledUploading(false);
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  crash_reporter::DumpWithoutCrashing();
  crash_reporter::DumpWithoutCrashing();
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  crash_helper::common::SetUserEnabledUploading(true);
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
}

}  // namespace
