// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_feed_enabled_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/feed/core/shared_prefs/pref_names.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests metrics that are recorded and uploaded by
// IOSFeedEnabledMetricsProvider.
class IOSFeedEnabledMetricsProviderTest : public PlatformTest {
  void SetUp() override {
    testing_pref_service_.registry()->RegisterBooleanPref(
        prefs::kArticlesForYouEnabled, true);
    testing_pref_service_.registry()->RegisterBooleanPref(
        prefs::kNTPContentSuggestionsEnabled, true);
    testing_pref_service_.registry()->RegisterBooleanPref(
        feed::prefs::kArticlesListVisible, true);
  }

 protected:
  TestingPrefServiceSimple testing_pref_service_;
  base::HistogramTester histogram_tester_;
};

// Tests the implementation of ProvideCurrentSessionData
TEST_F(IOSFeedEnabledMetricsProviderTest, ProvideCurrentSessionData) {
  IOSFeedEnabledMetricsProvider provider(&testing_pref_service_);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      false, 0);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      true, 1);

  testing_pref_service_.SetBoolean(prefs::kArticlesForYouEnabled, false);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      false, 1);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      true, 1);

  testing_pref_service_.SetBoolean(prefs::kArticlesForYouEnabled, true);
  testing_pref_service_.SetBoolean(prefs::kNTPContentSuggestionsEnabled, false);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      false, 2);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      true, 1);

  testing_pref_service_.SetBoolean(prefs::kNTPContentSuggestionsEnabled, true);
  testing_pref_service_.SetBoolean(feed::prefs::kArticlesListVisible, false);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      false, 3);
  histogram_tester_.ExpectBucketCount("ContentSuggestions.Feed.CanBeShown",
                                      true, 1);
}
