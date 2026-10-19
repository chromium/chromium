// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_session_metrics_manager.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"

@implementation GeminiSessionMetricsManager {
  // Timestamp when the Gemini flow started.
  base::TimeTicks _startTime;
}

#pragma mark - Session Lifecycle & Startup

- (void)geminiWillStart {
  _startTime = base::TimeTicks::Now();
}

- (void)geminiStarted:(BOOL)isFirstSession {
  if (_startTime.is_null()) {
    return;
  }
  base::TimeDelta startupTime = base::TimeTicks::Now() - _startTime;
  _startTime = base::TimeTicks();
  base::UmaHistogramLongTimes(isFirstSession ? kStartupTimeWithFirstRunHistogram
                                             : kStartupTimeNoFirstRunHistogram,
                              startupTime);
}

@end
