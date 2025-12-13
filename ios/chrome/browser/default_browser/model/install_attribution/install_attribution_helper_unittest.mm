// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_helper.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/time/time_override.h"
#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_acceptance_data.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

// Class representing the incoming data for the GMO Acceptance.
// Do not use AcceptanceData directly to test class redirection.
@interface GMOSKOAcceptanceData : NSObject <NSSecureCoding>

// The placement ID of the promo that was accepted.
@property(nonatomic, copy) NSNumber* placementID;

// The timestamp of when the promo was accepted.
@property(nonatomic, copy) NSDate* timestamp;

@end

@implementation GMOSKOAcceptanceData

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.placementID forKey:@"placementID"];
  [coder encodeObject:self.timestamp forKey:@"timestamp"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    _placementID = [coder decodeObjectOfClass:[NSNumber class]
                                       forKey:@"placementID"];
    _timestamp = [coder decodeObjectOfClass:[NSDate class] forKey:@"timestamp"];
  }
  return self;
}

@end

// Class representing the incoming data for the App Preview Acceptance.
// Do not use AcceptanceData directly to test class redirection.
@interface GCRAppPreviewAcceptanceData : NSObject <NSSecureCoding>

// The placement ID of the promo that was accepted.
@property(nonatomic, copy) NSNumber* placementID;

// The timestamp of when the promo was accepted.
@property(nonatomic, copy) NSDate* timestamp;

@end

@implementation GCRAppPreviewAcceptanceData

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.placementID forKey:@"placementID"];
  [coder encodeObject:self.timestamp forKey:@"timestamp"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  if ((self = [super init])) {
    _placementID = [coder decodeObjectOfClass:[NSNumber class]
                                       forKey:@"placementID"];
    _timestamp = [coder decodeObjectOfClass:[NSDate class] forKey:@"timestamp"];
  }
  return self;
}

@end

namespace install_attribution {

namespace {

// Some test placement ID value.
const int kFakePlacementID = 42;

}  // namespace

// Test fixture for testing the install attribution helper.
class InstallAttributionHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kIOSLogInstallAttribution, kIOSLogAppPreviewInstallAttribution}, {});
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
    [sharedDefaults
        removeObjectForKey:app_group::kAppPreviewInstallAttribution];
  }

  void TearDown() override {
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
    [sharedDefaults
        removeObjectForKey:app_group::kAppPreviewInstallAttribution];
  }

  void VerifyAcceptanceDataCleared(NSString* key,
                                   bool should_be_cleared = true) {
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    NSObject* value = [sharedDefaults objectForKey:key];
    EXPECT_EQ(value == nil, should_be_cleared);
  }

  static base::Time NowOverride() { return now_override_; }

  static void SetNowOverride(base::Time now_override) {
    now_override_ = now_override;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  static base::Time now_override_;
};

base::Time InstallAttributionHelperTest::now_override_;

// The helper should not crash nor record an attributable install if there is no
// acceptance data.
TEST_F(InstallAttributionHelperTest, NoAcceptanceData) {
  base::HistogramTester histogram_tester;

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// The helper should not crash nor record an attributable install if there is an
// acceptance data key but unrecognized data under it. The data should be
// deleted afterwards.
TEST_F(InstallAttributionHelperTest, UnrecognizedAcceptanceData) {
  base::HistogramTester histogram_tester;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  [sharedDefaults setObject:@"UNRECOGNIZED"
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// The helper should record an attributable install in the short attribution
// window if there is acceptance data that is within the corresponding interval.
// The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, AttributableInstallShortWindow) {
  base::HistogramTester histogram_tester;

  NSDate* oneHourAgo = (base::Time::Now() - base::Hours(1)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = oneHourAgo;
  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within24Hours, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// The helper should record an attributable install in the long attribution
// window if there is acceptance data that is within the corresponding interval.
// The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, AttributableInstallLongWindow) {
  base::HistogramTester histogram_tester;

  NSDate* sevenDaysAgo = (base::Time::Now() - base::Days(7)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = sevenDaysAgo;
  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within15Days, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// The helper should not record anything if there is acceptance data but the
// timestamp is too old. The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, ExpiredAcceptanceData) {
  base::HistogramTester histogram_tester;

  NSDate* thirtyDaysAgo = (base::Time::Now() - base::Days(30)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = thirtyDaysAgo;
  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// Validates placement ID for "short attribution" installs is only recorded in
// appropriate time buckets and not before.
TEST_F(InstallAttributionHelperTest,
       RecordPlacementIDForShortAttributionMidYear) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      &InstallAttributionHelperTest::NowOverride, nullptr, nullptr);
  base::Time acceptance_timestamp;
  base::Time now_override;

  // First, ensure the placement ID is not initially recorded when detecting an
  // attributable install.
  EXPECT_TRUE(
      base::Time::FromUTCString("2025-06-15 10:00:00", &acceptance_timestamp));
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-15 22:00:00", &now_override));
  SetNowOverride(now_override);

  NSDate* timestamp = acceptance_timestamp.ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = timestamp;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within24Hours, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Second, ensure there is still no placement ID recorded 2 weeks later.
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-27", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Finally, ensure placement ID is recorded after entering the next time
  // bucket.
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-05", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// Validates placement ID for "short attribution" installs is only recorded in
// appropriate time buckets and not before, and tests the edge case of the last
// month of the calendar year.
TEST_F(InstallAttributionHelperTest,
       RecordPlacementIDForShortAttributionYearEnd) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      &InstallAttributionHelperTest::NowOverride, nullptr, nullptr);
  base::Time acceptance_timestamp;
  base::Time now_override;

  // First, ensure the placement ID is not initially recorded when detecting an
  // attributable install.
  EXPECT_TRUE(
      base::Time::FromUTCString("2025-12-15 10:00:00", &acceptance_timestamp));
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-15 22:00:00", &now_override));
  SetNowOverride(now_override);

  NSDate* timestamp = acceptance_timestamp.ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = timestamp;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within24Hours, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Second, ensure there is still no placement ID recorded 2 weeks later.
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-28", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Finally, ensure placement ID is recorded after entering the next time
  // bucket.
  EXPECT_TRUE(base::Time::FromUTCString("2026-01-10", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// Validates placement ID for "long attribution" installs is only recorded in
// appropriate time buckets and not before.
TEST_F(InstallAttributionHelperTest,
       RecordPlacementIDForLongAttributionMidYear) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      &InstallAttributionHelperTest::NowOverride, nullptr, nullptr);
  base::Time acceptance_timestamp;
  base::Time now_override;

  // First, ensure the placement ID is not initially recorded when detecting an
  // attributable install.
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-05", &acceptance_timestamp));
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-15", &now_override));
  SetNowOverride(now_override);

  NSDate* timestamp = acceptance_timestamp.ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = timestamp;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within15Days, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Second, ensure there is still no placement ID recorded 2 weeks later.
  EXPECT_TRUE(base::Time::FromUTCString("2025-06-27", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Finally, ensure placement ID is recorded after entering the next time
  // bucket.
  EXPECT_TRUE(base::Time::FromUTCString("2025-07-05", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// Validates placement ID for "long attribution" installs is only recorded in
// appropriate time buckets and not before, and tests the edge case of the last
// month of the calendar year.
TEST_F(InstallAttributionHelperTest,
       RecordPlacementIDForLongAttributionYearEnd) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      &InstallAttributionHelperTest::NowOverride, nullptr, nullptr);
  base::Time acceptance_timestamp;
  base::Time now_override;

  // First, ensure the placement ID is not initially recorded when detecting an
  // attributable install.
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-05", &acceptance_timestamp));
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-15", &now_override));
  SetNowOverride(now_override);

  NSDate* timestamp = acceptance_timestamp.ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = timestamp;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.GMOSKOInstallAttribution",
                                     InstallAttributionType::Within15Days, 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Second, ensure there is still no placement ID recorded 2 weeks later.
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-28", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);

  // Finally, ensure placement ID is recorded after entering the next time
  // bucket.
  EXPECT_TRUE(base::Time::FromUTCString("2026-01-10", &now_override));
  SetNowOverride(now_override);

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow",
      kFakePlacementID, 1);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 1);
  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 1);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution);
}

// Validates that attribution data is not deleted if there is an error
// when calculating the time buckets.
TEST_F(InstallAttributionHelperTest, KeepAcceptanceDataIfTimeError) {
  base::HistogramTester histogram_tester;
  base::subtle::ScopedTimeClockOverrides time_override(
      &InstallAttributionHelperTest::NowOverride, nullptr, nullptr);
  base::Time acceptance_timestamp;
  base::Time now_override;

  // First, ensure the placement ID is not initially recorded when detecting an
  // attributable install.
  EXPECT_TRUE(base::Time::FromUTCString("2025-12-05", &acceptance_timestamp));
  EXPECT_TRUE(base::Time::FromUTCString("1001-01-01", &now_override));
  SetNowOverride(now_override);

  NSDate* timestamp = acceptance_timestamp.ToNSDate();
  GMOSKOAcceptanceData* acceptanceData = [[GMOSKOAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = timestamp;

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kGMOSKOInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.GMOSKOAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kGMOSKOInstallAttribution,
                              /*should_be_cleared=*/false);
}

// The helper should record an attributable install in the short attribution
// window if there is acceptance data that is within the corresponding interval
// for the App Preview source. The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, AppPreviewAttributableInstallShortWindow) {
  base::HistogramTester histogram_tester;

  NSDate* oneHourAgo = (base::Time::Now() - base::Hours(1)).ToNSDate();
  GCRAppPreviewAcceptanceData* acceptanceData =
      [[GCRAppPreviewAcceptanceData alloc] init];
  acceptanceData.placementID = @(kFakePlacementID);
  acceptanceData.timestamp = oneHourAgo;
  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
  NSError* archiveError = nil;
  NSData* archivedData =
      [NSKeyedArchiver archivedDataWithRootObject:acceptanceData
                            requiringSecureCoding:YES
                                            error:&archiveError];
  [sharedDefaults setObject:archivedData
                     forKey:app_group::kAppPreviewInstallAttribution];

  LogInstallAttribution();

  histogram_tester.ExpectBucketCount("IOS.AppPreviewInstallAttribution",
                                     InstallAttributionType::Within24Hours, 1);
  histogram_tester.ExpectTotalCount("IOS.AppPreviewInstallAttribution", 1);
  histogram_tester.ExpectTotalCount(
      "IOS.AppPreviewAttributionPlacementID.ShortAttributionWindow", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.AppPreviewAttributionPlacementID.LongAttributionWindow", 0);
  VerifyAcceptanceDataCleared(app_group::kAppPreviewInstallAttribution);
}

}  // namespace install_attribution
