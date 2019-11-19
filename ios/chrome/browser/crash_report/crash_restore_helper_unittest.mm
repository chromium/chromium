// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Return;

@interface CrashRestoreHelper (Test)
- (NSString*)sessionBackupPath;
@end

namespace {

class CrashRestoreHelperTest : public PlatformTest {
 public:
  CrashRestoreHelperTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    off_the_record_chrome_browser_state_ =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    helper_ = [[CrashRestoreHelper alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ios::ChromeBrowserState* off_the_record_chrome_browser_state_;
  CrashRestoreHelper* helper_;
};

TEST_F(CrashRestoreHelperTest, MoveAsideTest) {
  NSString* backup_path = [helper_ sessionBackupPath];
  NSFileManager* file_manager = [NSFileManager defaultManager];
  [file_manager removeItemAtPath:backup_path error:NULL];

  NSData* data = [NSData dataWithBytes:"hello" length:5];
  ios::ChromeBrowserState* browser_states[] = {
      chrome_browser_state_.get(), off_the_record_chrome_browser_state_,
  };

  for (size_t index = 0; index < base::size(browser_states); ++index) {
    NSString* state_path =
        base::SysUTF8ToNSString(browser_states[index]->GetStatePath().value());
    NSString* session_path =
        [SessionServiceIOS sessionPathForDirectory:state_path];
    [file_manager createFileAtPath:session_path contents:data attributes:nil];
    ASSERT_EQ(YES, [file_manager fileExistsAtPath:session_path]);
  }

  [helper_ moveAsideSessionInformation];

  for (size_t index = 0; index < base::size(browser_states); ++index) {
    NSString* state_path =
        base::SysUTF8ToNSString(browser_states[index]->GetStatePath().value());
    NSString* session_path =
        [SessionServiceIOS sessionPathForDirectory:state_path];
    EXPECT_EQ(NO, [file_manager fileExistsAtPath:session_path]);
  }

  EXPECT_EQ(YES, [file_manager fileExistsAtPath:backup_path]);
  [file_manager removeItemAtPath:backup_path error:NULL];
}

}  // namespace
