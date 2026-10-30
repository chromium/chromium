// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_METRICS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_METRICS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#include "base/time/time.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace syncher {

// Constant for timeout while waiting for asynchronous sync and UKM operations.
constexpr base::TimeDelta kSyncUKMOperationsTimeout = base::Seconds(10);

// Constant for timeout while waiting for asynchronous sync and DWA operations.
constexpr base::TimeDelta kSyncDWAOperationsTimeout = base::Seconds(60);

}  // namespace syncher

// MetricsAppInterface contains the app-side implementation for helpers. These
// helpers are compiled into the app binary and can be called from either app or
// test code.
@interface MetricsAppInterface : NSObject

#pragma mark - Synthetic Trials, Field Trials, and Network State

// Overrides metrics and crash reporting for testing. Must be called before
// setting the value.
+ (void)overrideMetricsAndCrashReportingForTesting;

// Stops overriding metrics and crash reporting for testing. Must be called at
// the end of the test for cleanup.
+ (void)stopOverridingMetricsAndCrashReportingForTesting;

// Enables or disables metrics and crash reporting for testing according to
// `enabled`. Returns whether metrics were previously enabled.
+ (BOOL)setMetricsAndCrashReportingForTesting:(BOOL)enabled;

// Returns YES if the ReportUserNoisedUserBirthYearAndGender feature is enabled.
+ (BOOL)isReportUserNoisedUserBirthYearAndGenderEnabled [[nodiscard]];

// Updates the network time to approximately `now`.
+ (void)updateNetworkTime:(base::Time)now;

// Returns the maximum eligible birth year for reporting demographics based on
// `now`.
+ (int)maximumEligibleBirthYearForTime:(base::Time)now;

// Creates a chrome_test_util::UserActionTester that will record every user
// action sent during test. Returns nil on success, or an NSError on failure.
+ (NSError*)setupUserActionTester [[nodiscard]];

// Releases the chrome_test_util::UserActionTester. Returns nil on success, or
// an NSError on failure.
+ (NSError*)releaseUserActionTester [[nodiscard]];

// Checks that the count of `userAction` equals `expectedCount`. Returns nil on
// success, or an NSError on failure.
+ (NSError*)expectCount:(int)expectedCount
          forUserAction:(NSString*)userAction [[nodiscard]];

// Returns YES if advanced reporting migration is completed.
+ (BOOL)isAdvancedReportingProfileMigrationDone;

// Returns YES if advanced reporting is enabled for the profile.
+ (BOOL)isAdvancedReportingEnabled;

// Sets the advanced reporting preference to `enabled` for the profile.
+ (void)setAdvancedReportingEnabledPref:(BOOL)enabled;

#pragma mark - UKM Recording and Verification

// Returns whether UKM recording is `enabled`.
+ (BOOL)checkUKMRecordingEnabled:(BOOL)enabled;

// Returns the current UKM client ID.
+ (uint64_t)UKMClientID;

// Returns whether `sourceID` is registered for UKM.
+ (BOOL)UKMHasDummySource:(int64_t)sourceID;

// Adds a new `sourceID` for UKM.
+ (void)UKMRecordDummySource:(int64_t)sourceID;

// If data are available, creates a UKM Report and stores it in the
// UKM service's UnsentLogStore.
+ (void)buildAndStoreUKMLog;

// Returns YES if the UKM service has logs to send.
+ (BOOL)hasUnsentUKMLogs;

// Returns YES if the UKM service's report has the expected `year` and `gender`.
// The `year` is the un-noised birth year, and `gender` corresponds to the
// options in UserDemographicsProto::Gender.
+ (BOOL)UKMReportHasBirthYear:(int)year
                       gender:(metrics::UserDemographicsProto::Gender)gender;

// Returns YES if the UKM service's report has user demographics.
+ (BOOL)UKMReportHasUserDemographics;

#pragma mark - UMA, DWA, and Histogram Verification

// If data are available, creates an UMA log and stores it in the
// MetricsLogStore.
+ (void)buildAndStoreUMALog;

// Returns YES if the metrics service has logs to send.
+ (BOOL)hasUnsentUMALogs;

// Returns YES if the UMA log has the expected `year` and `gender`. The `year`
// is the un-noised birth year, and `gender` corresponds to the proto enum
// UserDemographicsProto::Gender.
+ (BOOL)UMALogHasBirthYear:(int)year
                    gender:(metrics::UserDemographicsProto::Gender)gender;

// Returns whether DWA recording is `enabled`.
+ (BOOL)checkDWARecordingEnabled:(BOOL)enabled;

// Returns YES if the DWA recorder has entries matching `state`.
+ (BOOL)DWARecorderHasEntries:(BOOL)state;

// Returns YES if the DWA service has unsent logs matching `state`.
+ (BOOL)hasUnsentDWALogs:(BOOL)state;

// Records a new entry metric for DWA.
+ (void)recordTestDWAEntryMetric;

// Returns YES iff DWA allowed-for-all-profiles matches `state`.
+ (BOOL)DWARecorderAllowedForAllProfiles:(BOOL)state;

// Flushes the DWA service with a periodic trigger.
+ (void)DWAServiceFlushCall;

// Purges DWA recorder.
+ (void)clearDWARecorder;

// Creates a chrome_test_util::HistogramTester that will record every histogram
// sent during test. Returns an NSError if setup fails, or nil on success.
+ (NSError*)setupHistogramTester [[nodiscard]];

// Releases the chrome_test_util::HistogramTester. Returns an NSError if release
// fails, or nil on success.
+ (NSError*)releaseHistogramTester [[nodiscard]];

// Checks that `count` samples were recorded for `histogram` since the tester
// was constructed. Returns nil on success, or an NSError on failure.
+ (NSError*)expectTotalCount:(int)count
                forHistogram:(NSString*)histogram [[nodiscard]];

// Checks that `count` samples were recorded in `bucket` for `histogram` since
// the tester was constructed. Returns nil on success, or an NSError on failure.
+ (NSError*)expectCount:(int)count
              forBucket:(int)bucket
           forHistogram:(NSString*)histogram [[nodiscard]];

// Checks that `count` samples were uniquely recorded in `bucket` for
// `histogram` since the tester was constructed. Returns nil on success, or an
// NSError on failure.
+ (NSError*)expectUniqueSampleWithCount:(int)count
                              forBucket:(int)bucket
                           forHistogram:(NSString*)histogram [[nodiscard]];

// Checks that the sum of all samples recorded for `histogram` equals `sum`.
// Returns nil on success, or an NSError on failure.
+ (NSError*)expectSum:(NSInteger)sum
         forHistogram:(NSString*)histogram [[nodiscard]];

@end

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_METRICS_APP_INTERFACE_H_
