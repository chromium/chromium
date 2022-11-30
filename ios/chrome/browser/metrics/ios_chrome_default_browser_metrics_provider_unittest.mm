// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/metrics_log_uploader.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests metrics that are recorded and uploaded by
// IOSChromeDefaultBrowserMetricsProvider.
class IOSChromeDefaultBrowserMetricsProviderTest : public PlatformTest {
 protected:
  base::HistogramTester histogram_tester_;
};

// Tests the implementation of ProvideCurrentSessionData
TEST_F(IOSChromeDefaultBrowserMetricsProviderTest, ProvideCurrentSessionData) {
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kLastHTTPURLOpenTime];
  IOSChromeDefaultBrowserMetricsProvider provider(
      metrics::MetricsLogUploader::MetricServiceType::UMA);
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 0);

  [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                            forKey:kLastHTTPURLOpenTime];
  provider.ProvideCurrentSessionData(nullptr /* uma_proto */);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", true, 1);
  histogram_tester_.ExpectBucketCount("IOS.IsDefaultBrowser", false, 1);
}
