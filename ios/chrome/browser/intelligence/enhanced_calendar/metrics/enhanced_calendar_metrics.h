// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_METRICS_ENHANCED_CALENDAR_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_METRICS_ENHANCED_CALENDAR_METRICS_H_

// Enhanced Calendar response status histogram.
extern const char kEnhancedCalendarResponseStatusHistogram[];

// These values are persisted to IOS.EnhancedCalendar.ResponseStatus histograms.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(EnhancedCalendarResponseStatus)
enum class EnhancedCalendarResponseStatus {
  kSuccess = 0,
  kGenericFailure = 1,
  kCancelRequest = 2,
  kMaxValue = kCancelRequest,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnhancedCalendarResponseStatus)

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_METRICS_ENHANCED_CALENDAR_METRICS_H_
