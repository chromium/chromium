// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_

namespace base {
class TimeDelta;
}  // namespace base

// UMA histogram key for IOS.Gemini.Eligibility.
extern const char kEligibilityHistogram[];

// UMA histogram key for IOS.Gemini.EntryPoint.
extern const char kEntryPointHistogram[];

// UMA histogram key for IOS.Gemini.FRE.EntryPoint.
extern const char kFREEntryPointHistogram[];

// UMA histogram key for IOS.Gemini.FRE.PromoAction.
extern const char kPromoActionHistogram[];

// UMA histogram key for IOS.Gemini.FRE.ConsentAction.
extern const char kConsentActionHistogram[];

// Enum for the IOS.Gemini.FRE.PromoAction and IOS.Gemini.FRE.ConsentAction
// histograms.
// LINT.IfChange(IOSGeminiFREAction)
enum class IOSGeminiFREAction {
  kAccept = 0,
  kDismiss = 1,
  kMaxValue = kDismiss,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFREAction)

// Records the user action on the FRE Promo.
void RecordFREPromoAction(IOSGeminiFREAction action);

// Records the user action on the FRE Consent Screen.
void RecordFREConsentAction(IOSGeminiFREAction action);

// UMA histogram key for IOS.Gemini.StartupTime.FirstRun.
extern const char kStartupTimeWithFREHistogram[];

// UMA histogram key for IOS.Gemini.StartupTime.NotFirstRun.
extern const char kStartupTimeNoFREHistogram[];

// UMA histogram key for IOS.Gemini.Session.Time.
extern const char kBWGSessionTimeHistogram[];

// Records the duration of a Gemini session.
void RecordBWGSessionTime(base::TimeDelta session_duration);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
