// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"

// Tests which cover the metrics utilities functions
// in home_metrics.
class HomeMetricsTest : public PlatformTest {
 public:
  HomeMetricsTest() {}

  PrefService* pref_service() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(HomeMetricsTest, TestShorctutsNoFreshnessSignal) {
  EXPECT_EQ(
      -1,
      pref_service()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kShortcuts);
  EXPECT_EQ(
      -1,
      pref_service()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
}

TEST_F(HomeMetricsTest, TestShorctutsFreshnessSignalPresent) {
  pref_service()->SetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness, 42);
  LogTopModuleImpressionForType(ContentSuggestionsModuleType::kShortcuts);
  EXPECT_EQ(
      43,
      pref_service()->GetInteger(
          prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness));
}
