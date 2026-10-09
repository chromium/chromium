// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/model/first_run_metrics.h"

namespace first_run {

const char kFirstRunStageHistogram[] = "FirstRun.Stage";

const char kDefaultBrowserPromoSegmentationResultHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SegmentationResult";

const char kDefaultBrowserPromoSegmentationLatencySuccessHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SegmentationLatency.Success";

const char kDefaultBrowserPromoSegmentationLatencyFailureHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SegmentationLatency.Failure";

const char kDefaultBrowserPromoSwitcherInfoResultHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SwitcherInfoResult";

const char kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SwitcherInfoLatency.Success";

const char kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram[] =
    "IOS.FirstRun.DefaultBrowserPromo.SwitcherInfoLatency.Failure";

}  // namespace first_run
