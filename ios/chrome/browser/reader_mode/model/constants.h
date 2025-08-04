// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Recorded for IOS.ReaderMode.State. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReaderModeState)
enum class ReaderModeState {
  kHeuristicCanceled = 0,
  kHeuristicStarted = 1,
  kHeuristicCompleted = 2,
  kDistillationStarted = 3,
  kDistillationCompleted = 4,
  kReaderShown = 5,
  kDistillationTimedOut = 6,
  kMaxValue = kDistillationTimedOut,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeState)

// Recorded for IOS.ReaderMode.Distiller.Result. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeDistillerResult)
enum class ReaderModeDistillerResult {
  kPageIsNotDistillable = 0,
  kPageIsDistillable = 1,
  kMaxValue = kPageIsDistillable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeDistillerResult)

// Recorded for IOS.ReaderMode.Heuristic.Result. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeHeuristicResult)
enum class ReaderModeHeuristicResult {
  kMalformedResponse = 0,
  kReaderModeEligible = 1,
  kReaderModeNotEligibleContentOnly = 2,
  kReaderModeNotEligibleContentLength = 3,
  kReaderModeNotEligibleContentAndLength = 4,
  kMaxValue = kReaderModeNotEligibleContentAndLength,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeHeuristicResult)

// Recorded for IOS.ReaderMode.Heuristic.Classification.
// Compares the state between the Reader Mode heuristic classification
// and the result of the distiller. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeHeuristicClassification)
enum class ReaderModeHeuristicClassification {
  kPageNotEligibleWithEmptyDistill = 0,
  kPageNotEligibleWithPopulatedDistill = 1,
  kPageEligibleWithEmptyDistill = 2,
  kPageEligibleWithPopulatedDistill = 3,
  kMaxValue = kPageEligibleWithPopulatedDistill,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeHeuristicClassification)

// LINT.IfChange(ReaderModeCustomizationType)
enum class ReaderModeCustomizationType {
  kFontScale = 0,
  kFontFamily = 1,
  kTheme = 2,
  kMaxValue = kTheme,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeCustomizationType)

// Recorded for IOS.ReaderMode.Theme. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeTheme)
enum class ReaderModeTheme {
  kLight = 0,
  kDark = 1,
  kSepia = 2,
  kMaxValue = kSepia,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeTheme)

// Recorded for IOS.ReaderMode.FontFamily. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeFontFamily)
enum class ReaderModeFontFamily {
  kSansSerif = 0,
  kSerif = 1,
  kMonospace = 2,
  kMaxValue = kMonospace,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeFontFamily)

// Recorded for IOS.ReaderMode.AccessPoint. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReaderModeAccessPoint)
enum class ReaderModeAccessPoint {
  kContextualChip = 0,
  kToolsMenu = 1,
  kAIHub = 2,
  kMaxValue = kAIHub,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeAccessPoint)

// Recorded for IOS.ReaderMode.Distiller.Result. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(ReaderModeDistillerOutcome)
enum class ReaderModeDistillerOutcome {
  kContextualChipIsDistillable = 0,
  kContextualChipIsNotDistillable = 1,
  kToolsMenuIsDistillable = 2,
  kToolsMenuIsNotDistillable = 3,
  kAIHubIsDistillable = 4,
  kAIHubIsNotDistillable = 5,
  kMaxValue = kAIHubIsNotDistillable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeDistillerOutcome)

// Reasons for which Reader mode can be deactivated.
enum class ReaderModeDeactivationReason {
  // User deactivated Reader mode using the UI.
  kUserDeactivated,
  // Reader mode was deactivated because a navigation occurred.
  kNavigationDeactivated,
  // Reader mode was deactivated because distillation failed.
  kDistillationFailureDeactivated,
  // Reader mode was deactivated because the host tab was destroyed.
  kHostTabDestructionDeactivated,
};

// Default delay in seconds for triggering Reader Mode distiller heuristic.
// This allows the page to react to the DOM loading and ensures minimal
// interference with the JavaScript execution.
inline constexpr base::TimeDelta kReaderModeHeuristicPageLoadDelay =
    base::Seconds(1);

// Default timeout for distilling a page with Reader Mode enabled.
inline constexpr base::TimeDelta kReaderModeDistillationTimeout =
    base::Seconds(2);

// Histogram name for Reader Mode state.
extern const char kReaderModeStateHistogram[];

// Histogram name for Reader Mode heuristic result.
extern const char kReaderModeHeuristicResultHistogram[];

// Histogram name for Reader Mode heuristic latency.
extern const char kReaderModeHeuristicLatencyHistogram[];

// Histogram name for Reader Mode distillation latency.
extern const char kReaderModeDistillerLatencyHistogram[];

// Histogram name for Reader Mode distillation result.
extern const char kReaderModeDistillerResultHistogram[];

// Histogram name for Reader Mode theme customization.
extern const char kReaderModeThemeCustomizationHistogram[];

// Histogram name for Reader Mode font family customization.
extern const char kReaderModeFontFamilyCustomizationHistogram[];

// Histogram name for Reader Mode font scale customization.
extern const char kReaderModeFontScaleCustomizationHistogram[];

// Histogram name for Reader Mode customization.
extern const char kReaderModeCustomizationHistogram[];

// Histogram name for time spent in Reader Mode.
extern const char kReaderModeTimeSpentHistogram[];

// Histogram name for Reader Mode access point for starting distillation.
extern const char kReaderModeAccessPointHistogram[];

// Returns the Reader mode symbol name.
NSString* GetReaderModeSymbolName();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
