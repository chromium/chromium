// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/metrics_constants.h"

const char kPageContextHistogramPrefix[] = "IOS.PageContext";
const char kPageContextLatencyHistogramSuffix[] = ".Latency";
const char kPageContextByteSizeHistogramSuffix[] = ".ByteSize";

const char kPageContextHistogramOverallTask[] = ".Overall";
const char kPageContextHistogramScreenshotTask[] = ".Screenshot";
const char kPageContextHistogramAPCTask[] = ".AnnotatedPageContent";
const char kPageContextHistogramPDFTask[] = ".PDF";
const char kPageContextHistogramInnerTextTask[] = ".InnerText";

const char kPageContextHistogramSuccessStatus[] = ".Success";
const char kPageContextHistogramFailureStatus[] = ".Failure";
const char kPageContextHistogramTimeoutStatus[] = ".Timeout";
const char kPageContextHistogramPageProtectedStatus[] = ".PageProtected";
const char kPageContextHistogramNotExtractableStatus[] = ".NotExtractable";

const char kPageContextAPCConfigVariantInnerText[] = ".InnerTextOnly";
const char kPageContextAPCConfigVariantRich[] = ".Rich";
const char kPageContextAPCConfigVariantRichActionable[] = ".RichAndActionable";
