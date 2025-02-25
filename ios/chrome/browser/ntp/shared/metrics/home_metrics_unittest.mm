// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"

#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

// Tests which cover the metrics utilities functions in home_metrics.
class HomeMetricsTest : public PlatformTest {
 protected:
  HomeMetricsTest() { RegisterProfilePrefs(pref_service_.registry()); }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Verifies Most Visited module doesn't update impression count when no
// freshness signal exists
TEST_F(HomeMetricsTest, TestMostVisitedNoFreshnessSignal) {
  EXPECT_EQ(-1,
            pref_service_.GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kMostVisited,
                                &pref_service_);
  EXPECT_EQ(-1,
            pref_service_.GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness));
}

// Verifies Most Visited module increments impression count when freshness
// signal exists
TEST_F(HomeMetricsTest, TestMostVisitedFreshnessSignalPresent) {
  pref_service_.SetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness, 5);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kMostVisited,
                                &pref_service_);
  EXPECT_EQ(6,
            pref_service_.GetInteger(
                prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness));
}

// Verifies Shortcuts module doesn't update impression count when no freshness
// signal exists
TEST_F(HomeMetricsTest, TestShortcutsNoFreshnessSignal) {
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kShortcuts,
                                &pref_service_);
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
}

// Verifies Shortcuts module increments impression count when freshness signal
// exists
TEST_F(HomeMetricsTest, TestShortcutsFreshnessSignalPresent) {
  pref_service_.SetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness, 42);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kShortcuts,
                                &pref_service_);
  EXPECT_EQ(
      43,
      pref_service_.GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
}

// Verifies Safety Check module doesn't update impression count when no
// freshness signal exists
TEST_F(HomeMetricsTest, TestSafetyCheckNoFreshnessSignal) {
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kSafetyCheck,
                                &pref_service_);
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness));
}

// Verifies Safety Check module increments impression count when freshness
// signal exists
TEST_F(HomeMetricsTest, TestSafetyCheckFreshnessSignalPresent) {
  pref_service_.SetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
      10);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kSafetyCheck,
                                &pref_service_);
  EXPECT_EQ(
      11,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness));
}

// Verifies Tab Resumption module doesn't update impression count when no
// freshness signal exists
TEST_F(HomeMetricsTest, TestTabResumptionNoFreshnessSignal) {
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kTabResumption,
                                &pref_service_);
  EXPECT_EQ(
      -1,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness));
}

// Verifies Tab Resumption module increments impression count when freshness
// signal exists
TEST_F(HomeMetricsTest, TestTabResumptionFreshnessSignalPresent) {
  pref_service_.SetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness,
      7);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kTabResumption,
                                &pref_service_);
  EXPECT_EQ(
      8,
      pref_service_.GetInteger(
          prefs::
              kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness));
}

// Verifies Parcel Tracking module doesn't update local state impression count
// when no freshness signal exists
TEST_F(HomeMetricsTest, TestParcelTrackingNoFreshnessSignal) {
  EXPECT_EQ(
      -1,
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kParcelTracking,
                                &pref_service_);
  EXPECT_EQ(
      -1,
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness));
}

// Verifies Parcel Tracking module increments local state impression count when
// freshness signal exists
TEST_F(HomeMetricsTest, TestParcelTrackingFreshnessSignalPresent) {
  local_state()->SetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness,
      15);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kParcelTracking,
                                &pref_service_);
  EXPECT_EQ(
      16,
      local_state()->GetInteger(
          prefs::
              kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness));
}
