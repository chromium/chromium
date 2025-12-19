// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/country_codes/country_codes.h"
#import "components/metrics/private_metrics/private_metrics_features.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "third_party/metrics_proto/private_metrics/private_metrics.pb.h"
#import "third_party/metrics_proto/private_metrics/system_profiles/rc_coarse_system_profile.pb.h"

@interface PumaTestCase : ChromeTestCase
@end

@implementation PumaTestCase

- (void)setUp {
  [super setUp];

  // Grant metrics consent and update MetricsServicesManager. This implicitly
  // enables PUMA reporting.
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  GREYAssert(![MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
             @"setUp: Unpaired set/reset of user consent.");
  GREYAssertTrue([MetricsAppInterface isPumaReportingEnabled],
                 @"PUMA reporting should be enabled in setUp.");
}

- (void)tearDownHelper {
  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert([MetricsAppInterface setMetricsAndCrashReportingForTesting:NO],
             @"tearDownHelper: Unpaired set/reset of user consent.");
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];

  [super tearDownHelper];
}

// Enable the PUMA features and set the country code for the test.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      metrics::private_metrics::kPrivateMetricsPuma);
  config.features_enabled.push_back(
      metrics::private_metrics::kPrivateMetricsPumaRc);
  config.features_disabled.push_back(
      metrics::private_metrics::kPrivateMetricsFeature);
  config.additional_args.push_back(base::SysNSStringToUTF8([NSString
      stringWithFormat:@"--%s=BE", switches::kSearchEngineChoiceCountry]));
  return config;
}

#pragma mark - Helpers

// Waits for a new incognito tab to be opened.
- (void)openNewIncognitoTab {
  const NSUInteger incognitoTabCount = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognitoTabCount + 1)];
}

// Waits for the current incognito tab to be closed.
- (void)closeCurrentIncognitoTab {
  const NSUInteger incognitoTabCount = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognitoTabCount - 1)];
}

// Waits for a new tab to be opened.
- (void)openNewRegularTab {
  const NSUInteger tabCount = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:(tabCount + 1)];
}

// Waits for the current regular tab to be closed.
- (void)closeCurrentRegularTab {
  const NSUInteger tabCount = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:(tabCount - 1)];
}

// Switches to the regular tab at `index`.
- (void)switchToRegularTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(
                                          index)] performAction:grey_tap()];
  // Wait for the omnibox to appear after the tab switch.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::DefocusedLocationView()];
}

// Switches to the incognito tab at `index`.
- (void)switchToIncognitoTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(
                                          index)] performAction:grey_tap()];
  // Wait for the omnibox to appear after the tab switch.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::DefocusedLocationView()];
}

#pragma mark - Tests

// LINT.IfChange(VerifyRcCoarseSystemProfile)
// Tests that the RcCoarseSystemProfile is correctly recorded.
- (void)testVerifyRcCoarseSystemProfile {
  // Purge any logs that might have been generated by other tests.
  [MetricsAppInterface purgePumaLogs];
  GREYAssertFalse([MetricsAppInterface hasUnsentPumaLogs],
                  @"Should have no unsent PUMA logs before test.");

  // Record some test metrics.
  [MetricsAppInterface recordTestPumaMetric];

  // Flush the service to write the log to prefs.
  [MetricsAppInterface flushPumaService];
  GREYAssertTrue([MetricsAppInterface hasUnsentPumaLogs],
                 @"Should have unsent PUMA logs.");

  // Get the last RC profile from the log store.
  NSDictionary* rcProfile = [MetricsAppInterface lastPumaRcProfile];
  GREYAssertNotNil(rcProfile, @"Failed to get last PUMA RC profile.");

  // Verify platform.
  GREYAssertEqualObjects(@(::private_metrics::Platform::PLATFORM_IOS),
                         rcProfile[@"platform"], @"Incorrect platform.");

  // Verify milestone.
  GREYAssertEqualObjects(@(version_info::GetMajorVersionNumberAsInt()),
                         rcProfile[@"milestone"], @"Incorrect milestone.");

  // Verify country.
  GREYAssertEqualObjects(@(country_codes::CountryId("BE").Serialize()),
                         rcProfile[@"profile_country_id"],
                         @"Incorrect country.");

  // Verify channel.
  GREYAssertEqualObjects(
      @(::private_metrics::RcCoarseSystemProfile::CHANNEL_UNKNOWN),
      rcProfile[@"channel"], @"Incorrect channel.");
}
// LINT.ThenChange(/chrome/browser/metrics/puma_browsertest.cc:VerifyRcCoarseSystemProfile)

// Make sure PumaService does not crash during a graceful browser shutdown.
// LINT.IfChange(PumaServiceCheck)
- (void)testPumaServiceCheck {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to setup histogram tester");

  [MetricsAppInterface recordTestPumaMetric];

  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:1
                    forHistogram:@"PUMA.PumaServiceTestHistogram.Boolean"],
                @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:50
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Linear"],
      @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:0
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Enum"],
      @"Histogram count mismatch");

  [MetricsAppInterface flushPumaService];
  GREYAssertTrue([MetricsAppInterface hasUnsentPumaLogs],
                 @"Should have unsent PUMA logs.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester");
}
// LINT.ThenChange(/chrome/browser/metrics/puma_browsertest.cc:PumaServiceCheck)

// Make sure that PUMA is enabled when an incognito window is open.
// LINT.IfChange(RegularBrowserPlusIncognitoCheck)
- (void)testRegularBrowserPlusIncognitoCheck {
  // Opening an incognito browser should not disable PumaService.
  [self openNewIncognitoTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after opening incognito tab.");

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to setup histogram tester");

  // We should be able to record metrics.
  [MetricsAppInterface recordTestPumaMetric];
  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:1
                    forHistogram:@"PUMA.PumaServiceTestHistogram.Boolean"],
                @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:50
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Linear"],
      @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:1
                             forBucket:0
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Enum"],
      @"Histogram count mismatch");

  [MetricsAppInterface flushPumaService];
  GREYAssertTrue([MetricsAppInterface hasUnsentPumaLogs],
                 @"Should have unsent PUMA logs.");

  // Opening another regular browser should not change the state.
  [self openNewRegularTab];
  GREYAssertTrue([MetricsAppInterface isPumaReportingEnabled],
                 @"PUMA reporting should be enabled after opening new tab.");

  // Opening and closing another Incognito browser should not change the state.
  [self openNewIncognitoTab];
  [self closeCurrentIncognitoTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after closing incognito tab.");

  // Close regular tab should not change the state.
  [self switchToRegularTabAtIndex:0];
  [self closeCurrentRegularTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after closing regular tab.");

  // Closing the first incognito browser should not change the state.
  [self switchToIncognitoTabAtIndex:0];
  [self closeCurrentIncognitoTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after closing first incognito tab.");

  // We should still be able to record metrics.
  [MetricsAppInterface purgePumaLogs];
  GREYAssertFalse([MetricsAppInterface hasUnsentPumaLogs],
                  @"Should have no unsent PUMA logs after purge.");

  [MetricsAppInterface recordTestPumaMetric];
  GREYAssertNil([MetricsAppInterface
                     expectCount:2
                       forBucket:1
                    forHistogram:@"PUMA.PumaServiceTestHistogram.Boolean"],
                @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:2
                             forBucket:50
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Linear"],
      @"Histogram count mismatch");
  GREYAssertNil(
      [MetricsAppInterface expectCount:2
                             forBucket:0
                          forHistogram:@"PUMA.PumaServiceTestHistogram.Enum"],
      @"Histogram count mismatch");

  [MetricsAppInterface flushPumaService];
  GREYAssertTrue([MetricsAppInterface hasUnsentPumaLogs],
                 @"Should have unsent PUMA logs.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester");
}
// LINT.ThenChange(/chrome/browser/metrics/puma_browsertest.cc:RegularBrowserPlusIncognitoCheck)

// Make sure opening a regular browser after Incognito PUMA still get enabled.
// LINT.IfChange(IncognitoPlusRegularBrowserCheck)
- (void)testIncognitoPlusRegularBrowserCheck {
  [self openNewIncognitoTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after opening incognito tab.");

  [self openNewRegularTab];
  GREYAssertTrue([MetricsAppInterface isPumaReportingEnabled],
                 @"PUMA reporting should be enabled after opening new tab.");

  [self closeCurrentRegularTab];
  GREYAssertTrue(
      [MetricsAppInterface isPumaReportingEnabled],
      @"PUMA reporting should be enabled after closing regular tab.");

  [self switchToIncognitoTabAtIndex:0];
  [self closeCurrentIncognitoTab];
}
// LINT.ThenChange(/chrome/browser/metrics/puma_browsertest.cc:IncognitoPlusRegularBrowserCheck)

@end
