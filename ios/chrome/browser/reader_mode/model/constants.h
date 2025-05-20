// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

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

// Recorded for IOS.ReaderMode.Distiller.Amp.
// Compares the state between the distillation success and the
// usage of AMP for the web page.
// LINT.IfChange(ReaderModeAmpClassification)
enum class ReaderModeAmpClassification {
  kEmptyDistillNoAmp = 0,
  kPopulatedDistillNoAmp = 1,
  kEmptyDistillWithAmp = 2,
  kPopulatedDistillWithAmp = 3,
  kMaxValue = kPopulatedDistillWithAmp,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ReaderModeAmpClassification)

// Default delay in seconds for triggering Reader Mode distiller heuristic.
// This allows the page to react to the DOM loading and ensures minimal
// interference with the JavaScript execution.
inline constexpr base::TimeDelta kReaderModeDistillerPageLoadDelay =
    base::Seconds(1);

// Histogram name for Reader Mode heuristic result.
extern const char kReaderModeHeuristicResultHistogram[];

// Histogram name for Reader Mode heuristic latency.
extern const char kReaderModeHeuristicLatencyHistogram[];

// Histogram name for Reader Mode distillation latency.
extern const char kReaderModeDistillerLatencyHistogram[];

// Histogram name for comparison between the AMP usage in the web state and
// the distillation success.
extern const char kReaderModeAmpClassificationHistogram[];

// Returns the Reader mode symbol name.
NSString* GetReaderModeSymbolName();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
