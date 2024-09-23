// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/time/time.h"
#import "components/metrics/demographics/demographic_metrics_provider.h"
#import "components/ukm/ukm_service.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "third_party/metrics_proto/user_demographics.pb.h"

namespace {

const metrics::UserDemographicsProto::Gender kTestGender =
    metrics::UserDemographicsProto::GENDER_MALE;

}  // namespace

@interface DemographicsTestCase : ChromeTestCase {
  int birthYear_;
}
@end

@implementation DemographicsTestCase

- (void)setUp {
  [super setUp];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  // Set a network time so that SyncPrefs::GetUserNoisedBirthYearAndGender does
  // not return a UserDemographicsResult for the kCannotGetTime status.
  const base::Time now = base::Time::Now();
  [MetricsAppInterface updateNetworkTime:now];

  // Get the maximum eligible birth year for reporting demographics.
  birthYear_ = [MetricsAppInterface maximumEligibleBirthYearForTime:now];
  [self addUserDemographicsToSyncServerWithBirthYear:birthYear_
                                              gender:kTestGender];
  [self signInAndEnableHistorySync];
  [self grantMetricsConsent];
}

- (void)tearDown {
  [ChromeEarlGrey clearFakeSyncServerData];
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Features are enabled or disabled based on the name of the test that is
  // running. This is done because (A) parameterized tests do not exist in Earl
  // Grey and (B) it is inefficient to use ensureAppLaunchedWithConfiguration
  // for each test.
  //
  // Note that in the if statements, @selector(testSomething) is used rather
  // than @"testSomething" because the former checks that the testSomething
  // method exists somewhere--but not necessarily in this class.
  if ([self isRunningTest:@selector
            (testUKMDemographicsReportingWithFeatureEnabled)]) {
    config.features_enabled.push_back(metrics::kDemographicMetricsReporting);
    config.features_enabled.push_back(
        ukm::kReportUserNoisedUserBirthYearAndGender);
  } else if ([self isRunningTest:@selector
                   (testUKMDemographicsReportingWithFeatureDisabled)]) {
    config.features_disabled.push_back(metrics::kDemographicMetricsReporting);
    config.features_disabled.push_back(
        ukm::kReportUserNoisedUserBirthYearAndGender);
  } else if ([self isRunningTest:@selector
                   (testUMADemographicsReportingWithFeatureEnabled)]) {
    config.features_enabled.push_back(metrics::kDemographicMetricsReporting);
  } else if ([self isRunningTest:@selector
                   (testUMADemographicsReportingWithFeatureDisabled)]) {
    config.features_disabled.push_back(metrics::kDemographicMetricsReporting);
  }
  return config;
}

#pragma mark - Helpers

// Adds user demographics, which are DataType::PRIORITY_PREFERENCES, to the
// fake sync server. `rawBirthYear` is the true birth year, pre-noise, and the
// gender corresponds to the options in UserDemographicsProto::Gender.
//
// Also, verifies (A) that before adding the demographics, the server has no
// priority preferences and (B) that after adding the demographics, the server
// has one priority preference.
- (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender {
  GREYAssertEqual(
      [ChromeEarlGrey
          numberOfSyncEntitiesWithType:syncer::PRIORITY_PREFERENCES],
      0, @"The fake sync server should have no priority preferences.");

  [ChromeEarlGrey addUserDemographicsToSyncServerWithBirthYear:rawBirthYear
                                                        gender:gender];

  GREYAssertEqual(
      [ChromeEarlGrey
          numberOfSyncEntitiesWithType:syncer::PRIORITY_PREFERENCES],
      1, @"The fake sync server should have one priority preference.");
}

// Signs into Chrome with a fake identity, enables history sync, and then waits
// up to kSyncUKMOperationsTimeout for sync to initialize.
- (void)signInAndEnableHistorySync {
  // Note that there is only one profile on iOS. Additionally, URL-keyed
  // anonymized data collection is turned on as part of the flow to Sign in to
  // Chrome and enable history sync. This matches the main user flow that
  // enables UKM.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey
      waitForSyncEngineInitialized:YES
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
}

// Adds a dummy UKM source to the UKM service's recordings. The presence of this
// dummy source allows UKM reports to be built and logged.
- (void)addDummyUKMSource {
  const uint64_t sourceId = 0x54321;
  [MetricsAppInterface UKMRecordDummySource:sourceId];
  GREYAssert([MetricsAppInterface UKMHasDummySource:sourceId],
             @"Failed to record dummy source.");
}

// Turns on metrics collection for testing and verifies that this has been
// successfully done.
- (void)grantMetricsConsent {
  GREYAssertFalse(
      [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
      @"User consent has already been granted.");
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM recording is enabled.");

  // The client ID is non-zero after metrics uploading permissions are updated.
  GREYAssertNotEqual(0U, [MetricsAppInterface UKMClientID],
                     @"Client ID should be non-zero.");
}

// Adds dummy data,  stores it in the UKM service's UnsentLogStore, and verifies
// that the UnsentLogStore has an unsent log.
- (void)buildAndStoreUKMLog {
  // Record a source in the UKM service so that there is data with which to
  // generate a UKM Report.
  [self addDummyUKMSource];
  [MetricsAppInterface buildAndStoreUKMLog];
  GREYAssertTrue([MetricsAppInterface hasUnsentUKMLogs],
                 @"The UKM service should have unsent logs.");
}

#pragma mark - Tests

// The tests in this file should correspond to the demographics-related tests in
// //chrome/browser/metrics/ukm_browsertest.cc and
// //chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc.

// Tests that user demographics are synced, recorded by UKM, and logged in
// histograms.
//
// Corresponds to AddSyncedUserBirthYearAndGenderToProtoData in
// //chrome/browser/metrics/ukm_browsertest.cc with features enabled.
- (void)testUKMDemographicsReportingWithFeatureEnabled {
  // See `appConfigurationForTestCase` for feature set-up. The kUkmFeature is
  // enabled by default.
  GREYAssertTrue([ChromeEarlGrey isDemographicMetricsReportingEnabled] &&
                     [MetricsAppInterface
                         isReportUserNoisedUserBirthYearAndGenderEnabled] &&
                     [ChromeEarlGrey isUKMEnabled],
                 @"Failed to enable the requisite features.");

  [self buildAndStoreUKMLog];

  GREYAssertTrue([MetricsAppInterface UKMReportHasBirthYear:birthYear_
                                                     gender:kTestGender],
                 @"The report should contain the specified user demographics");
}

// Tests that user demographics are neither recorded by UKM nor logged in
// histograms when the user is signed-in and history sync is on.
//
// Corresponds to AddSyncedUserBirthYearAndGenderToProtoData in
// //chrome/browser/metrics/ukm_browsertest.cc with features disabled.
- (void)testUKMDemographicsReportingWithFeatureDisabled {
  // See `appConfigurationForTestCase` for feature set-up. The kUkmFeature is
  // enabled by default.
  GREYAssertFalse([ChromeEarlGrey isDemographicMetricsReportingEnabled],
                  @"Failed to disable kDemographicMetricsReporting.");
  GREYAssertFalse(
      [MetricsAppInterface isReportUserNoisedUserBirthYearAndGenderEnabled],
      @"Failed to disable kReportUserNoisedUserBirthYearAndGender.");
  GREYAssertTrue([ChromeEarlGrey isUKMEnabled],
                 @"Failed to enable kUkmFeature.");

  [self buildAndStoreUKMLog];

  GREYAssertFalse([MetricsAppInterface UKMReportHasUserDemographics],
                  @"The report should not contain user demographics.");
}

// Tests that user demographics are synced, recorded by UMA, and logged in
// histograms.
//
// Corresponds to AddSyncedUserBirthYearAndGenderToProtoData in
// //chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc
// with features enabled.
- (void)testUMADemographicsReportingWithFeatureEnabled {
  // See `appConfigurationForTestCase` for feature set-up. The kUkmFeature is
  // enabled by default.
  GREYAssertTrue([ChromeEarlGrey isDemographicMetricsReportingEnabled],
                 @"Failed to enable kDemographicMetricsReporting.");

  [MetricsAppInterface buildAndStoreUMALog];
  GREYAssertTrue([MetricsAppInterface hasUnsentUMALogs],
                 @"The UKM service should have unsent logs.");

  GREYAssertTrue([MetricsAppInterface UMALogHasBirthYear:birthYear_
                                                  gender:kTestGender],
                 @"The report should contain the specified user demographics");

  const int success =
      static_cast<int>(metrics::UserDemographicsStatus::kSuccess);
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:success
                                   forHistogram:@"UMA.UserDemographics.Status"],
                @"Unexpected histogram contents");
}

// Tests that user demographics are neither recorded by UMA nor logged in
// histograms when the user is signed-in and history sync is on.
//
// Corresponds to AddSyncedUserBirthYearAndGenderToProtoData in
// //chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc
// with features disabled.
- (void)testUMADemographicsReportingWithFeatureDisabled {
  // See `appConfigurationForTestCase` for feature set-up.
  GREYAssertFalse([ChromeEarlGrey isDemographicMetricsReportingEnabled],
                  @"Failed to disable kDemographicMetricsReporting.");

  [MetricsAppInterface buildAndStoreUMALog];
  GREYAssertTrue([MetricsAppInterface hasUnsentUMALogs],
                 @"The UKM service should have unsent logs.");

  GREYAssertNil([MetricsAppInterface expectSum:0
                                  forHistogram:@"UMA.UserDemographics.Status"],
                @"Unexpected histogram contents.");
}

@end
