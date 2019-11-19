// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/previous_session_info.h"

#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/metrics/previous_session_info_private.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key in the UserDefaults for a boolean value keeping track of memory warnings.
NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating =
    previous_session_info_constants::
        kDidSeeMemoryWarningShortlyBeforeTerminating;

// Key in the NSUserDefaults for a string value that stores the version of the
// last session.
NSString* const kLastRanVersion = @"LastRanVersion";
// Key in the NSUserDefaults for a string value that stores the language of the
// last session.
NSString* const kLastRanLanguage = @"LastRanLanguage";

using PreviousSessionInfoTest = PlatformTest;

TEST_F(PreviousSessionInfoTest, InitializationWithEmptyDefaults) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the default values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
  EXPECT_FALSE([sharedInstance previousSessionVersion]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameLanguage) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Set the current language as the last ran language.
  NSString* currentVersion = [[NSLocale preferredLanguages] objectAtIndex:0];
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithDifferentLanguage) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Set the current language as the last ran language.
  NSString* currentVersion = @"Fake Language";
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_NSEQ(currentVersion, [sharedInstance previousSessionVersion]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_NSEQ(currentVersion, [sharedInstance previousSessionVersion]);
}

TEST_F(PreviousSessionInfoTest, InitializationDifferentVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_NSEQ(@"Fake Version", [sharedInstance previousSessionVersion]);
}

TEST_F(PreviousSessionInfoTest, InitializationDifferentVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_NSEQ(@"Fake Version", [sharedInstance previousSessionVersion]);
}

// Creates conditions that exist on the first app run and tests
// OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationWithoutSystemStartTime) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:previous_session_info_constants::kOSStartTime];

  EXPECT_FALSE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

// Creates conditions that exist when OS was restarted after the previous app
// run and tests OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationAfterOSRestart) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  // For the previous session OS started 60 seconds before OS has started for
  // this session.
  NSTimeInterval current_system_start_time =
      NSDate.timeIntervalSinceReferenceDate -
      NSProcessInfo.processInfo.systemUptime;
  [[NSUserDefaults standardUserDefaults]
      setDouble:current_system_start_time - 60
         forKey:previous_session_info_constants::kOSStartTime];

  EXPECT_TRUE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

// Creates conditions that exist when OS was not restarted after the previous
// app run and tests OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationForSecondSessionAfterOSRestart) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  // OS startup time is the same for this and previous session.
  NSTimeInterval current_system_start_time =
      NSDate.timeIntervalSinceReferenceDate -
      NSProcessInfo.processInfo.systemUptime;
  [[NSUserDefaults standardUserDefaults]
      setDouble:current_system_start_time
         forKey:previous_session_info_constants::kOSStartTime];

  EXPECT_FALSE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

TEST_F(PreviousSessionInfoTest, BeginRecordingCurrentSession) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  // Check that the version has been updated.
  EXPECT_NSEQ(base::SysUTF8ToNSString(version_info::GetVersionNumber()),
              [defaults stringForKey:kLastRanVersion]);

  // Check that the memory warning flag has been reset.
  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest, SetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Call the flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest,
       ResetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest, MemoryWarningFlagMethodsAfterRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Launch the recording of the session.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

}  // namespace
