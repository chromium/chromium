// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/ios_search_engine_choice_service_client.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/version.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/signin_util_internal.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns the URL for the backed up sentinel file.
NSURL* GetSentinelThatIsBackedUpURLPath() {
  const base::FilePath path = PathForSentinel(kSentinelThatIsBackedUp);
  NSString* path_string = base::SysUTF8ToNSString(path.value());
  return [NSURL fileURLWithPath:path_string];
}

// Returns the URL for the not backed up sentinel file.
NSURL* GetSentinelThatIsNotBackedUpURLPath() {
  const base::FilePath path = PathForSentinel(kSentinelThatIsNotBackedUp);
  NSString* path_string = base::SysUTF8ToNSString(path.value());
  return [NSURL fileURLWithPath:path_string];
}

// Creates a file with a timestamp based on `url`.
void CreateDeviceRestoreSentinelFile(NSURL* url, base::Time timestamp) {
  NSDictionary* not_backed_up_attributes =
      @{NSFileCreationDate : timestamp.ToNSDate()};
  [[NSFileManager defaultManager] createFileAtPath:url.path
                                          contents:nil
                                        attributes:not_backed_up_attributes];
}

// Removes both sentinel files related to the device restore (the backed one and
// the not backed one).
void ResetSentinelFiles() {
  NSFileManager* file_manager = [NSFileManager defaultManager];
  [file_manager removeItemAtPath:GetSentinelThatIsNotBackedUpURLPath().path
                           error:nil];
  [file_manager removeItemAtPath:GetSentinelThatIsBackedUpURLPath().path
                           error:nil];
}

}  // namespace

namespace ios {

class IOSSearchEngineChoiceServiceClientTest : public PlatformTest {
 public:
  IOSSearchEngineChoiceServiceClientTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    ResetDeviceRestoreDataForTesting();
    ResetSentinelFiles();
  }

  void TearDown() override {
    ResetSentinelFiles();
    ResetDeviceRestoreDataForTesting();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSSearchEngineChoiceServiceClient ios_search_engine_choice_service_client_;
};

TEST_F(IOSSearchEngineChoiceServiceClientTest,
       IsProfileEligibleForDseGuestPropagation) {
  EXPECT_FALSE(ios_search_engine_choice_service_client_
                   .IsProfileEligibleForDseGuestPropagation());
}

// Tests IsDeviceRestoreDetectedInCurrentSession() and
// DoesChoicePredateDeviceRestore() when the restore happened in a previous
// session, and the search engine choice happened after the restore.
TEST_F(IOSSearchEngineChoiceServiceClientTest, NoBackupRestore) {
  const base::Time reference = base::Time::Now();
  const base::Time backed_up_creation_timestamp = reference;
  const base::Time not_backed_up_creation_timestamp = reference;
  const base::Time choice_completion_timestamp = reference + base::Minutes(1);

  CreateDeviceRestoreSentinelFile(GetSentinelThatIsBackedUpURLPath(),
                                  backed_up_creation_timestamp);
  CreateDeviceRestoreSentinelFile(GetSentinelThatIsNotBackedUpURLPath(),
                                  not_backed_up_creation_timestamp);
  base::RunLoop run_loop;
  // Call IsFirstSessionAfterDeviceRestore() explicitly to make sure sentinel
  // files related to backup/restore are fully created before the end of the
  // test.
  IsFirstSessionAfterDeviceRestore(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(ios_search_engine_choice_service_client_
                   .IsDeviceRestoreDetectedInCurrentSession());
  search_engines::ChoiceCompletionMetadata metadata = {
      .timestamp = choice_completion_timestamp,
      .version = base::Version("1.2.3.4")};
  EXPECT_FALSE(
      ios_search_engine_choice_service_client_.DoesChoicePredateDeviceRestore(
          metadata));
}

// Tests IsDeviceRestoreDetectedInCurrentSession() and
// DoesChoicePredateDeviceRestore() when the restore happened in a previous
// session, and the search engine choice happened before the restore.
TEST_F(IOSSearchEngineChoiceServiceClientTest,
       RestoreHappenedInPreviousSession) {
  const base::Time reference = base::Time::Now();
  const base::Time backed_up_creation_timestamp = reference;
  const base::Time choice_completion_timestamp = reference + base::Minutes(1);
  const base::Time not_backed_up_creation_timestamp =
      reference + base::Minutes(2);

  CreateDeviceRestoreSentinelFile(GetSentinelThatIsBackedUpURLPath(),
                                  backed_up_creation_timestamp);
  CreateDeviceRestoreSentinelFile(GetSentinelThatIsNotBackedUpURLPath(),
                                  not_backed_up_creation_timestamp);
  base::RunLoop run_loop;
  // Call IsFirstSessionAfterDeviceRestore() explicitly to make sure sentinel
  // files related to backup/restore are fully created before the end of the
  // test.
  IsFirstSessionAfterDeviceRestore(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(ios_search_engine_choice_service_client_
                   .IsDeviceRestoreDetectedInCurrentSession());
  search_engines::ChoiceCompletionMetadata metadata = {
      .timestamp = choice_completion_timestamp,
      .version = base::Version("1.2.3.4")};
  EXPECT_TRUE(
      ios_search_engine_choice_service_client_.DoesChoicePredateDeviceRestore(
          metadata));
}

// Tests IsDeviceRestoreDetectedInCurrentSession() and
// DoesChoicePredateDeviceRestore() when the restore happened in the current
// session, and the search engine choice happened before the restore.
TEST_F(IOSSearchEngineChoiceServiceClientTest,
       RestoreHappenedInCurrentSession) {
  const base::Time reference = base::Time::Now();
  const base::Time backed_up_creation_timestamp = reference;
  const base::Time choice_completion_timestamp = reference + base::Minutes(1);

  CreateDeviceRestoreSentinelFile(GetSentinelThatIsBackedUpURLPath(),
                                  backed_up_creation_timestamp);
  base::RunLoop run_loop;
  // Call IsFirstSessionAfterDeviceRestore() explicitly to make sure sentinel
  // files related to backup/restore are fully created before the end of the
  // test.
  IsFirstSessionAfterDeviceRestore(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(ios_search_engine_choice_service_client_
                  .IsDeviceRestoreDetectedInCurrentSession());
  search_engines::ChoiceCompletionMetadata metadata = {
      .timestamp = choice_completion_timestamp,
      .version = base::Version("1.2.3.4")};
  EXPECT_TRUE(
      ios_search_engine_choice_service_client_.DoesChoicePredateDeviceRestore(
          metadata));
}

}  // namespace ios
