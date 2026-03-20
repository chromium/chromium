// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_

// PageContext histogram constants.
extern const char kPageContextHistogramPrefix[];
extern const char kPageContextLatencyHistogramSuffix[];
extern const char kPageContextByteSizeHistogramSuffix[];

extern const char kPageContextHistogramOverallTask[];
extern const char kPageContextHistogramScreenshotTask[];
extern const char kPageContextHistogramAPCTask[];
extern const char kPageContextHistogramPDFTask[];
extern const char kPageContextHistogramInnerTextTask[];

extern const char kPageContextHistogramSuccessStatus[];
extern const char kPageContextHistogramFailureStatus[];
extern const char kPageContextHistogramTimeoutStatus[];
extern const char kPageContextHistogramPageProtectedStatus[];
extern const char kPageContextHistogramNotExtractableStatus[];

extern const char kPageContextAPCConfigVariantInnerText[];
extern const char kPageContextAPCConfigVariantRich[];
extern const char kPageContextAPCConfigVariantRichActionable[];

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_METRICS_CONSTANTS_H_
