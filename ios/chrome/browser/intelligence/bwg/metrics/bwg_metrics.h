// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_

// UMA histogram key for IOS.BWG.Eligibility.
extern const char kEligibilityHistogram[];

// UMA histogram key for IOS.BWG.EntryPoint.
extern const char kEntryPointHistogram[];

// UMA histogram key for IOS.BWG.FRE.EntryPoint.
extern const char kFREEntryPointHistogram[];

// UMA histogram key for IOS.Gemini.FRE.PromoAction.
extern const char kPromoActionHistogram[];

// Enum for the IOS.Gemini.FRE.PromoAction histogram.
// LINT.IfChange(IOSGeminiFREPromoAction)
enum class IOSGeminiFREPromoAction {
  kAccept = 0,
  kDismiss = 1,
  kMaxValue = kDismiss,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFREPromoAction)

// Records the user action on the FRE Promo.
void RecordFREPromoAction(IOSGeminiFREPromoAction action);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
