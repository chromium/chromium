// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/command_line.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/variations_smoke_test/variations_smoke_test_app_interface.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Test switch to verify that the fetch happened within app launch of current
// test.
static const char kVerifyFetchedInCurrentLaunch[] =
    "verify-fetched-in-current-launch";

// Seed and signature arguments. If exists, no seed will be fetched and the
// seed will be applied.
static const char kSeedArg[] = "seed";
static const char kSignatureArg[] = "signature";

// Variations channel. If exists, the app will be assigned to it.
static const char kVariationsChannelArg[] = "variations-channel";

// Timeout to wait for a successful variations seed fetch after the test method
// starts.
static const NSTimeInterval kWaitForFetchTimeout = 30.0;

}  // namespace

// Test case to try to update a variations seed.
@interface VariationsSmokeTestCase : ChromeTestCase
@end

@implementation VariationsSmokeTestCase

#pragma mark - Helpers
// Helper method to synchronously wait for the async hasSafeSeed check
- (BOOL)isVariationsSeedStored {
  XCTestExpectation* expectation =
      [self expectationWithDescription:@"Wait for hasSafeSeed check"];
  __block BOOL safeSeedPresent = NO;
  [VariationsSmokeTestAppInterface isVariationsSeedStored:^(BOOL hasSeed) {
    safeSeedPresent = hasSeed;
    [expectation fulfill];
  }];
  NSTimeInterval timeout = 5.0;
  [self waitForExpectationsWithTimeout:timeout handler:nil];

  return safeSeedPresent;
}

#pragma mark - Lifecycle
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSeedArg)) {
    config.relaunch_policy = ForceRelaunchByCleanShutdown;
    config.additional_args.push_back("--disable-variations-seed-fetch");
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kVariationsChannelArg)) {
    std::string channel =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kVariationsChannelArg);
    config.relaunch_policy = ForceRelaunchByCleanShutdown;
    config.additional_args.push_back(
        base::StrCat({"--fake-variations-channel=", channel}));
  }
  return config;
}

#pragma mark - Tests
// Waits for variations seed to appear in Local State prefs. The test will only
// pass in official build with accepted EULA in Local State prefs.
- (void)testVariationsSeedPresentsInPrefs {
  BOOL verifyFetchedInCurrentLaunch =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kVerifyFetchedInCurrentLaunch);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSeedArg)) {
    std::string seed_arg =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSeedArg);
    std::string signature_arg =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kSignatureArg);
    NSString* seed = base::SysUTF8ToNSString(seed_arg.c_str());
    NSString* signature = base::SysUTF8ToNSString(signature_arg.c_str());
    // Store the seed data and signature from the arguments
    [VariationsSmokeTestAppInterface storeSeed:seed andSignature:signature];

    // Restart chrome so the new seed is loaded.
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];
  }
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Waiting for variations seed fetch."
                  block:^BOOL {
                    BOOL variationsSeedExists = [self isVariationsSeedStored];
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
