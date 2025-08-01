// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"

const char kEligibilityHistogram[] = "IOS.Gemini.Eligibility";

const char kEntryPointHistogram[] = "IOS.Gemini.EntryPoint";

const char kFREEntryPointHistogram[] = "IOS.Gemini.FRE.EntryPoint";

const char kPromoActionHistogram[] = "IOS.Gemini.FRE.PromoAction";

const char kConsentActionHistogram[] = "IOS.Gemini.FRE.ConsentAction";

const char kStartupTimeWithFREHistogram[] = "IOS.Gemini.StartupTime.FirstRun";

const char kStartupTimeNoFREHistogram[] = "IOS.Gemini.StartupTime.NotFirstRun";

const char kBWGSessionTimeHistogram[] = "IOS.Gemini.Session.Time";

void RecordFREPromoAction(IOSGeminiFREAction action) {
  base::UmaHistogramEnumeration(kPromoActionHistogram, action);
}

void RecordFREConsentAction(IOSGeminiFREAction action) {
  base::UmaHistogramEnumeration(kPromoActionHistogram, action);
}

void RecordBWGSessionTime(base::TimeDelta session_duration) {
  base::UmaHistogramTimes(kBWGSessionTimeHistogram, session_duration);
}
