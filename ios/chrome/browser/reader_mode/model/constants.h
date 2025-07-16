// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Recorded for IOS.ReaderMode.State. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/429174292): Add states to capture distillation and display
// of the Reading Mode UI.
// LINT.IfChange(ReaderModeState)
enum class ReaderModeState {
  kHeuristicCanceled = 0,
  kHeuristicStarted = 1,
  kHeuristicCompleted = 2,
  kDistillationStarted = 3,
  kDistillationCompleted = 4,
  kReaderShown = 5,
  kMaxValue = kReaderShown,
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

// Default delay in seconds for triggering Reader Mode distiller heuristic.
// This allows the page to react to the DOM loading and ensures minimal
// interference with the JavaScript execution.
inline constexpr base::TimeDelta kReaderModeDistillerPageLoadDelay =
    base::Seconds(1);

// Histogram name for Reader Mode state.
extern const char kReaderModeStateHistogram[];

// Histogram name for Reader Mode heuristic result.
extern const char kReaderModeHeuristicResultHistogram[];

// Histogram name for Reader Mode heuristic latency.
extern const char kReaderModeHeuristicLatencyHistogram[];

// Histogram name for Reader Mode distillation latency.
extern const char kReaderModeDistillerLatencyHistogram[];

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

// Returns the Reader mode symbol name.
NSString* GetReaderModeSymbolName();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
