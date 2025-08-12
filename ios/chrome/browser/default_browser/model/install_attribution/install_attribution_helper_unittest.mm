// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/install_attribution/install_attribution_helper.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/default_browser/model/install_attribution/gmo_sko_acceptance_data.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "testing/platform_test.h"

namespace install_attribution {

namespace {

// Some test placement ID value.
const int kFakePlacementID = 42;

}  // namespace

// Test fixture for testing the install attribution helper.
class InstallAttributionHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kIOSLogInstallAttribution);
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
  }

  void TearDown() override {
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    [sharedDefaults removeObjectForKey:app_group::kGMOSKOInstallAttribution];
  }

  void VerifyAcceptanceDataCleared() {
    NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();
    NSObject* value =
        [sharedDefaults objectForKey:app_group::kGMOSKOInstallAttribution];
    EXPECT_TRUE(value == nil);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The helper should not crash nor record an attributable install if there is no
// acceptance data.
TEST_F(InstallAttributionHelperTest, NoAcceptanceData) {
  base::HistogramTester histogram_tester;

  LogInstallAttribution();

  histogram_tester.ExpectTotalCount("IOS.GMOSKOInstallAttribution", 0);
  VerifyAcceptanceDataCleared();
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
  VerifyAcceptanceDataCleared();
}

// The helper should record an attributable install in the short attribution
// window if there is acceptance data that is within the corresponding interval.
// The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, AttributableInstallShortWindow) {
  base::HistogramTester histogram_tester;

  NSDate* oneHourAgo = (base::Time::Now() - base::Hours(1)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData =
      [[GMOSKOAcceptanceData alloc] initWithPlacementID:@(kFakePlacementID)
                                              timestamp:oneHourAgo];
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
  VerifyAcceptanceDataCleared();
}

// The helper should record an attributable install in the long attribution
// window if there is acceptance data that is within the corresponding interval.
// The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, AttributableInstallLongWindow) {
  base::HistogramTester histogram_tester;

  NSDate* sevenDaysAgo = (base::Time::Now() - base::Days(7)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData =
      [[GMOSKOAcceptanceData alloc] initWithPlacementID:@(kFakePlacementID)
                                              timestamp:sevenDaysAgo];
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
  VerifyAcceptanceDataCleared();
}

// The helper should not record anything if there is acceptance data but the
// timestamp is too old. The data should be deleted afterwards.
TEST_F(InstallAttributionHelperTest, ExpiredAcceptanceData) {
  base::HistogramTester histogram_tester;

  NSDate* thirtyDaysAgo = (base::Time::Now() - base::Days(30)).ToNSDate();
  GMOSKOAcceptanceData* acceptanceData =
      [[GMOSKOAcceptanceData alloc] initWithPlacementID:@(kFakePlacementID)
                                              timestamp:thirtyDaysAgo];
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
  VerifyAcceptanceDataCleared();
}

}  // namespace install_attribution
