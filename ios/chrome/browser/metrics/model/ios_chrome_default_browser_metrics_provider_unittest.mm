// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_default_browser_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/metrics_log_uploader.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "testing/platform_test.h"

// Tests metrics that are recorded and uploaded by
// IOSChromeDefaultBrowserMetricsProvider.
class IOSChromeDefaultBrowserMetricsProviderTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
};

// Tests the implementation of OnDidCreateMetricsLog().
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest, OnDidCreateMetricsLog) {
  ClearDefaultBrowserPromoData();
  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.OnDidCreateMetricsLog();
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 0);

  LogOpenHTTPURLFromExternalURL();
  provider.OnDidCreateMetricsLog();
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
}
