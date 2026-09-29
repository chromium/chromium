// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_ENTRY_FLOW_RESULT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_ENTRY_FLOW_RESULT_H_

#import <Foundation/Foundation.h>

// Result of the Gemini entry flow. Callers use this to update their
// own UI (e.g., hide entry points, show snackbar).
typedef NS_ENUM(NSInteger, GeminiEntryFlowResult) {
  // Default value indicating the result was not set.
  kGeminiEntryFlowResultUnknown = 0,
  // Gemini session started successfully.
  kGeminiEntryFlowResultSuccess,
  // User cancelled sign-in or dismissed the flow.
  kGeminiEntryFlowResultCancelled,
  // Account is ineligible due to enterprise policy.
  kGeminiEntryFlowResultAccountIneligibleByEnterprise,
  // Account is ineligible due to Gemini policy restriction.
  kGeminiEntryFlowResultAccountIneligibleByGemini,
  // Account capability restriction prevents Gemini usage.
  kGeminiEntryFlowResultAccountCapabilityRestricted,
  // Current page is not eligible for Gemini.
  kGeminiEntryFlowResultPageIneligible,
  // Eligibility check timed out and Gemini started optimistically.
  kGeminiEntryFlowResultTimeout,
};

// Completion block called with the final result of the Gemini entry flow.
typedef void (^GeminiEntryFlowCompletion)(GeminiEntryFlowResult result);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_ENTRY_FLOW_RESULT_H_
