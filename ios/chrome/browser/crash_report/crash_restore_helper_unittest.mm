// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_restore_helper.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Return;

namespace {

class CrashRestoreHelperTest : public PlatformTest {
 public:
  CrashRestoreHelperTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    off_the_record_chrome_browser_state_ =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    test_browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    helper_ = [[CrashRestoreHelper alloc] initWithBrowser:test_browser_.get()];
  }

 protected:
  // Creates the session for `session_id`, if `session_id` is nil a session
  // will be created in the default location.
  // Returns `true` if the creation was successful.
  bool CreateSession(NSString* session_id) {
    NSFileManager* file_manager = [NSFileManager defaultManager];
    ChromeBrowserState* browser_states[] = {
        chrome_browser_state_.get(),
        off_the_record_chrome_browser_state_,
    };
    NSData* data = [NSData dataWithBytes:"hello" length:5];
    for (size_t index = 0; index < std::size(browser_states); ++index) {
      const base::FilePath& state_path = browser_states[index]->GetStatePath();
      NSString* backup_path =
          [CrashRestoreHelper backupPathForSessionID:session_id
                                           directory:state_path];
      [file_manager removeItemAtPath:backup_path error:nil];
      NSString* session_path =
          [SessionServiceIOS sessionPathForSessionID:session_id
                                           directory:state_path];
      NSString* directory = [session_path stringByDeletingLastPathComponent];
      if (![file_manager fileExistsAtPath:directory]) {
        [file_manager createDirectoryAtPath:directory
                withIntermediateDirectories:YES
                                 attributes:nil
                                      error:nil];
      }
      [file_manager createFileAtPath:session_path contents:data attributes:nil];
      if (![file_manager fileExistsAtPath:session_path])
        return false;
    }
    return true;
  }

  // Returns `true` if session for `session_id` was erased from its default
  // location. if `session_id` is nil, the default session location is used.
  bool IsSessionErased(NSString* session_id) {
    NSFileManager* file_manager = [NSFileManager defaultManager];
    ChromeBrowserState* browser_states[] = {
        chrome_browser_state_.get(),
        off_the_record_chrome_browser_state_,
    };

    for (size_t index = 0; index < std::size(browser_states); ++index) {
      const base::FilePath& state_path = browser_states[index]->GetStatePath();
      NSString* session_path =
          [SessionServiceIOS sessionPathForSessionID:session_id
                                           directory:state_path];
      if ([file_manager fileExistsAtPath:session_path])
        return false;
    }
    return true;
  }

  // Returns `true` if the session with `session_id` was backed up correctly,
  // and deletes the backup file. if `session_id` is nil, the default backup
  // session location is used.
  bool CheckAndDeleteSessionBackedUp(NSString* session_id,
                                     ChromeBrowserState* browser_state) {
    NSFileManager* file_manager = [NSFileManager defaultManager];
    NSString* backup_path = [CrashRestoreHelper
        backupPathForSessionID:session_id
                     directory:browser_state->GetStatePath()];
    if (![file_manager fileExistsAtPath:backup_path])
      return false;
    [file_manager removeItemAtPath:backup_path error:nil];
    return true;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> test_browser_;
  ChromeBrowserState* off_the_record_chrome_browser_state_;
  CrashRestoreHelper* helper_;
};

// Tests that moving session work correctly when multiple windows are supported.
TEST_F(CrashRestoreHelperTest, MoveAsideMultipleSessions) {
  NSSet<NSString*>* session_ids =
      [NSSet setWithObjects:@"session_1", @"session_2", nil];
  for (NSString* session_id in session_ids) {
    ASSERT_TRUE(CreateSession(session_id));
  }

  [CrashRestoreHelper moveAsideSessions:session_ids
                        forBrowserState:chrome_browser_state_.get()];
  for (NSString* session_id in session_ids) {
    EXPECT_TRUE(IsSessionErased(session_id));
    EXPECT_EQ(YES, CheckAndDeleteSessionBackedUp(session_id,
                                                 chrome_browser_state_.get()));
  }
}

}  // namespace
