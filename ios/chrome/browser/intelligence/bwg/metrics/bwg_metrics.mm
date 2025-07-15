// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"

#import "base/metrics/histogram_functions.h"

const char kEligibilityHistogram[] = "IOS.BWG.Eligibility";

const char kEntryPointHistogram[] = "IOS.BWG.EntryPoint";

const char kFREEntryPointHistogram[] = "IOS.BWG.FRE.EntryPoint";

const char kPromoActionHistogram[] = "IOS.Gemini.FRE.PromoAction";

void RecordFREPromoAction(IOSGeminiFREPromoAction action) {
  base::UmaHistogramEnumeration(kPromoActionHistogram, action);
}
