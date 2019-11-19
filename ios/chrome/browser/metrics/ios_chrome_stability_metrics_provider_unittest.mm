// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/metrics_proto/system_profile.pb.h"

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
       DidStartLoadingEventShouldIncrementPageLoadCount) {
  if (base::FeatureList::IsEnabled(
          web::features::kLogLoadStartedInDidStartNavigation))
    return;
  IOSChromeStabilityMetricsProvider provider(&prefs_);

  // A load should not increment metrics if recording is disabled.
  provider.WebStateDidStartLoading(nullptr);

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().page_load_count());
  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix(
                      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric)
                  .empty());

  // A load should increment metrics if recording is enabled.
  provider.OnRecordingEnabled();
  provider.WebStateDidStartLoading(nullptr);

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(1, system_profile.stability().page_load_count());
  histogram_tester_.ExpectTotalCount(
      IOSChromeStabilityMetricsProvider::kPageLoadCountLoadingStartedMetric, 1);
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       DidStartNavigationEventShouldIncrementPageLoadCount) {
  if (!base::FeatureList::IsEnabled(
          web::features::kLogLoadStartedInDidStartNavigation))
    return;

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://www.site.com"));
  context.SetIsSameDocument(false);
  IOSChromeStabilityMetricsProvider provider(&prefs_);

  // A navigation should not increment metrics if recording is disabled.
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().page_load_count());
  EXPECT_TRUE(histogram_tester_
                  .GetTotalCountsForPrefix(
                      IOSChromeStabilityMetricsProvider::kPageLoadCountMetric)
                  .empty());

  // A navigation should increment metrics if recording is enabled.
  provider.OnRecordingEnabled();
  provider.WebStateDidStartNavigation(kNullWebState, &context);

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(1, system_profile.stability().page_load_count());
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

  metrics::SystemProfileProto system_profile;
  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(0, system_profile.stability().page_load_count());
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

  metrics::SystemProfileProto system_profile;
  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(0, system_profile.stability().page_load_count());
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

  metrics::SystemProfileProto system_profile;
  provider.ProvideStabilityMetrics(&system_profile);
  EXPECT_EQ(0, system_profile.stability().page_load_count());
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

  metrics::SystemProfileProto system_profile;
  provider.ProvideStabilityMetrics(&system_profile);
  int page_load_count = base::FeatureList::IsEnabled(
                            web::features::kLogLoadStartedInDidStartNavigation)
                            ? 1
                            : 0;
  EXPECT_EQ(page_load_count, system_profile.stability().page_load_count());
}

TEST_F(IOSChromeStabilityMetricsProviderTest,
       LogRendererCrashShouldIncrementCrashCount) {
  IOSChromeStabilityMetricsProvider provider(&prefs_);

  // A crash should not increment the renderer crash count if recording is
  // disabled.
  provider.LogRendererCrash();

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(0, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());

  // A crash should increment the renderer crash count if recording is
  // enabled.
  provider.OnRecordingEnabled();
  provider.LogRendererCrash();

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(1, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(0, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());
}
