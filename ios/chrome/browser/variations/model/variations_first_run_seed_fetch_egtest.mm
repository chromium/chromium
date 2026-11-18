// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/strcat.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// `NSUserDefaults` key that `VariationsAppStateAgent` uses to skip the
// first-run seed fetch once a seed has been fetched.
NSString* const kLastVariationsSeedFetchTimeKey =
    @"kLastVariationsSeedFetchTime";

// Server URL that refuses connections, so the fetch fails fast and offline.
constexpr char kUnreachableVariationsServerURL[] = "https://127.0.0.1:1/";

}  // namespace

// Tests the first-run variations seed fetch in the real app startup sequence.
@interface VariationsFirstRunSeedFetchTestCase : ChromeTestCase
@end

@implementation VariationsFirstRunSeedFetchTestCase

// Tests that startup reaches the First Run Experience after the first-run seed
// fetch, which runs before web schemes are registered.
- (void)testStartupCompletesAfterFirstRunSeedFetch {
  [ChromeEarlGrey
      removeUserDefaultsObjectForKey:kLastVariationsSeedFetchTimeKey];

  AppLaunchConfiguration config;
  config.additional_args = {
      "-FirstRunForceEnabled",
      "true",
      // An available identity makes First Run open on the sign-in screen.
      base::StrCat({"-", test_switches::kAddFakeIdentitiesAtStartup}),
      // Enables the first-run seed fetch, which is otherwise limited to
      // Chrome-branded builds.
      base::StrCat({"--", variations::switches::kVariationsServerURL, "=",
                    kUnreachableVariationsServerURL}),
      // Keeps the regular variations service from using the fake server.
      base::StrCat({"--", variations::switches::kDisableVariationsSeedFetch}),
  };
  // Killing the app skips the clean shutdown that persists a seed fetch time.
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunSignInScreenAccessibilityIdentifier)];
}

@end
