// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_stability_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/stability_metrics_helper.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

web::WebState* const kNullWebState = nullptr;

class IOSChromeStabilityMetricsProviderTest : public PlatformTest {
 protected:
  IOSChromeStabilityMetricsProviderTest() {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs_.registry());
  }

  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple prefs_;
};

}  // namespace

TEST_F(IOSChromeStabilityMetricsProviderTest,
       DidStartNavigationEventShouldIncrementPageLoadCount) {
  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.site.com"));
  context.SetIsSameDocument(false);
  IOSChromeStabilityMetricsProvider provider(&prefs_);

  // A navigation should not increment metrics if recording is disabled.
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 0);
  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix(
                      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric)
                  .empty());

  // A navigation should increment metrics if recording is enabled.
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 1);
  histogram_tester_.ExpectUniqueSample(
      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric,
      static_cast<base::HistogramBase::Sample>(
          IOSChromeStabilityMetricsProvider::PageLoadCountNavigationType::
              PAGE_LOAD_NAVIGATION),
      1);
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       SameDocumentNavigationShouldNotLogPageLoad) {
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);

  IOSChromeStabilityMetricsProvider provider(&prefs_);
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectUniqueSample(
      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric,
      static_cast<base::HistogramBase::Sample>(
          IOSChromeStabilityMetricsProvider::PageLoadCountNavigationType::
              SAME_DOCUMENT_WEB_NAVIGATION),
      1);

  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 0);
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       ChromeUrlNavigationShouldNotLogPageLoad) {
  web::FakeNavigationContext context;
  context.SetUrl(GURL("chrome://newtab"));
  context.SetIsSameDocument(false);

  IOSChromeStabilityMetricsProvider provider(&prefs_);
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectUniqueSample(
      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric,
      static_cast<base::HistogramBase::Sample>(
          IOSChromeStabilityMetricsProvider::PageLoadCountNavigationType::
              CHROME_URL_NAVIGATION),
      1);
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 0);
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       SameDocumentChromeUrlNavigationShouldNotLogPageLoad) {
  web::FakeNavigationContext context;
  context.SetUrl(GURL("chrome://newtab"));
  context.SetIsSameDocument(true);

  IOSChromeStabilityMetricsProvider provider(&prefs_);
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectUniqueSample(
      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric,
      static_cast<base::HistogramBase::Sample>(
          IOSChromeStabilityMetricsProvider::PageLoadCountNavigationType::
              CHROME_URL_NAVIGATION),
      1);
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 0);
}

TEST_F(IOSChromeStabilityMetricsProviderTest, WebNavigationShouldLogPageLoad) {
  web::FakeNavigationContext context;
  IOSChromeStabilityMetricsProvider provider(&prefs_);
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  histogram_tester_.ExpectUniqueSample(
      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric,
      static_cast<base::HistogramBase::Sample>(
          IOSChromeStabilityMetricsProvider::PageLoadCountNavigationType::
              PAGE_LOAD_NAVIGATION),
      1);
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kPageLoad, 1);
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       LogRendererCrashShouldIncrementCrashCount) {
  IOSChromeStabilityMetricsProvider provider(&prefs_);

  // A crash should not increment the renderer crash count if recording is
  // disabled.
  provider.LogRendererCrash();
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 0);
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kExtensionCrash, 0);

  // A crash should increment the renderer crash count if recording is
  // enabled.
  provider.OnRecordingEnabled();
  provider.LogRendererCrash();
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kRendererCrash, 1);
  histogram_tester_.ExpectBucketCount(
      "Stability.Counts2", metrics::StabilityEventType::kExtensionCrash, 0);
}
