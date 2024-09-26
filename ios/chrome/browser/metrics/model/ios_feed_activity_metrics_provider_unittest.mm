// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_feed_activity_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/metrics/model/constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Tests metrics that are recorded and uploaded by
// IOSFeedActivityMetricsProvider.
class IOSFeedActivityMetricsProviderTest : public PlatformTest {
 public:
  IOSFeedActivityMetricsProviderTest() = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void AddProfile(const std::string& name, FeedActivityBucket bucket) {
    TestProfileIOS* profile = profile_manager_.AddProfileWithBuilder(
        std::move(TestProfileIOS::Builder().SetName(name)));

    profile->GetPrefs()->SetInteger(kActivityBucketKey,
                                    base::to_underlying(bucket));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::HistogramTester histogram_tester_;
};

// Tests the implementation of ProvideCurrentSessionData when there is
// no ProfileIOS loaded.
TEST_F(IOSFeedActivityMetricsProviderTest,
       ProvideCurrentSessionData_NoBrowserState) {
  IOSFeedActivityMetricsProvider provider;

  // Check that no data is recorded if there are no loaded BrowserState.
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kAllFeedsActivityBucketsByProviderHistogram),
              ::testing::ElementsAre());
}

// Tests the implementation of ProvideCurrentSessionData when there is
// one ProfileIOS loaded, with low activity bucket.
TEST_F(IOSFeedActivityMetricsProviderTest,
       ProvideCurrentSessionData_OneBrowserState) {
  IOSFeedActivityMetricsProvider provider;

  AddProfile("Default", FeedActivityBucket::kLowActivity);

  // Check that the data is recorded as expected.
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kAllFeedsActivityBucketsByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(1, 1)));
}

// Tests the implementation of ProvideCurrentSessionData when there are
// multiple ProfileIOS loaded, with different activity buckets.
TEST_F(IOSFeedActivityMetricsProviderTest,
       ProvideCurrentSessionData_MultipleProfileIOS) {
  IOSFeedActivityMetricsProvider provider;

  AddProfile("Profile1", FeedActivityBucket::kLowActivity);
  AddProfile("Profile2", FeedActivityBucket::kLowActivity);
  AddProfile("Profile3", FeedActivityBucket::kHighActivity);

  // Check that the data is recorded as expected.
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  kAllFeedsActivityBucketsByProviderHistogram),
              ::testing::ElementsAre(base::Bucket(1, 2), base::Bucket(3, 1)));
}
