// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_

#import "base/time/time.h"

// Recorded for IOS.ReaderMode.Heuristic. Entries should not be
// renumbered and numeric values should never be reused.
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

// Default delay in seconds for triggering Reader Mode distiller heuristic.
// This allows the page to react to the DOM loading and ensures minimal
// interference with the JavaScript execution.
inline constexpr base::TimeDelta kReaderModeDistillerPageLoadDelay =
    base::Seconds(1);

// Histogram name for Reader Mode heuristic result.
extern const char kReaderModeHeuristicResultHistogram[];

// Histogram name for Reader Mode heuristic latency.
extern const char kReaderModeHeuristicLatencyHistogram[];

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_CONSTANTS_H_
