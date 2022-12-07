// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/metrics/metrics_features.h"
#import "components/metrics/metrics_log_uploader.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils_test_support.h"
#import "testing/platform_test.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests metrics that are recorded and uploaded by
// IOSChromeDefaultBrowserMetricsProvider.
class IOSChromeDefaultBrowserMetricsProviderTest
    : public PlatformTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
    }
  }

  bool ShouldEmitHistogramsEarlier() const { return GetParam(); }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         IOSChromeDefaultBrowserMetricsProviderTest,
                         testing::Bool());

// Tests the implementation of ProvideCurrentSessionData() and
// OnDidCreateMetricsLog().
TEST_P(IOSChromeDefaultBrowserMetricsProviderTest, ProvideCurrentSessionData) {
  ClearDefaultBrowserPromoData();
  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  if (!ShouldEmitHistogramsEarlier()) {
    metrics::ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);
  } else {
    provider.OnDidCreateMetricsLog();
  }
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 0);

  LogOpenHTTPURLFromExternalURL();
  if (!ShouldEmitHistogramsEarlier()) {
    metrics::ChromeUserMetricsExtension uma_proto;
    provider.ProvideCurrentSessionData(&uma_proto);
  } else {
    provider.OnDidCreateMetricsLog();
  }
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
}
