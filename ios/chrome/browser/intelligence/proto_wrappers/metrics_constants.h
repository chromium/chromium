// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_

// PageContext latency histogram constants.
extern const char kPageContextLatencyHistogramPrefix[];
extern const char kPageContextLatencyHistogramSuffix[];

extern const char kPageContextLatencyHistogramOverallTask[];
extern const char kPageContextLatencyHistogramScreenshotTask[];
extern const char kPageContextLatencyHistogramAPCTask[];
extern const char kPageContextLatencyHistogramPDFTask[];
extern const char kPageContextLatencyHistogramInnerTextTask[];

extern const char kPageContextLatencyHistogramSuccessStatus[];
extern const char kPageContextLatencyHistogramFailureStatus[];
extern const char kPageContextLatencyHistogramTimeoutStatus[];
extern const char kPageContextLatencyHistogramPageProtectedStatus[];

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_
