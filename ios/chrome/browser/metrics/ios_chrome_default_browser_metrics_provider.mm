// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_default_browser_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeDefaultBrowserMetricsProvider::
    IOSChromeDefaultBrowserMetricsProvider() {}

IOSChromeDefaultBrowserMetricsProvider::
    ~IOSChromeDefaultBrowserMetricsProvider() {}

void IOSChromeDefaultBrowserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  base::UmaHistogramBoolean("IOS.IsDefaultBrowser",
                            IsChromeLikelyDefaultBrowser());
  base::UmaHistogramBoolean("IOS.IsEligibleDefaultBrowserPromoUser",
                            IsLikelyInterestedDefaultBrowserUser());
}
