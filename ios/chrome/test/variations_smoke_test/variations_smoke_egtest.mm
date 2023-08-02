// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/command_line.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/variations_smoke_test/variations_smoke_test_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Test switch to verify that the fetch happened within app launch of current
// test.
static const char kVerifyFetchedInCurrentLaunch[] =
    "verify-fetched-in-current-launch";

// Timeout to wait for a successful variations seed fetch after the test method
// starts.
static const NSTimeInterval kWaitForFetchTimeout = 30.0;

}  // namespace

// Test case to try to update a variations seed.
@interface VariationsSmokeTestCase : ChromeTestCase
@end

@implementation VariationsSmokeTestCase

// Waits for variations seed to appear in Local State prefs. The test will only
// pass in official build with accepted EULA in Local State prefs.
- (void)testVariationsSeedPresentsInPrefs {
  BOOL verifyFetchedInCurrentLaunch =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kVerifyFetchedInCurrentLaunch);
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Waiting for variations seed fetch."
                  block:^BOOL {
                    BOOL variationsSeedExists = [VariationsSmokeTestAppInterface
                        variationsSeedInLocalStatePrefs];
                    BOOL expectedLastFetchTimeCondition =
                        !verifyFetchedInCurrentLaunch ||
                        [VariationsSmokeTestAppInterface
                            variationsSeedFetchedInCurrentLaunch];
                    return variationsSeedExists &&
                           expectedLastFetchTimeCondition;
                  }];
  GREYAssert([condition waitWithTimeout:kWaitForFetchTimeout],
             @"Failed to fetch variations seed within timeout.");
  // Writes prefs to Local State file. This might be used in launcher script.
  [VariationsSmokeTestAppInterface localStatePrefsCommitPendingWrite];
}

@end
