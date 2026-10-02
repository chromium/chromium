// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_

#import <Foundation/Foundation.h>

// Error domain for TTC backend and session errors.
extern NSString* const kTTCErrorDomain;

// Error codes for TTC backend and session failures.
enum class TTCErrorCode : NSInteger {
  // Server-side errors (0-1000):
  kUnknown = 0,
  kRateLimited = 1,
  kSafetyBlocked = 2,
  kInternalBackendError = 3,
  kSessionExpired = 4,
  kMaxServerErrorCode = kSessionExpired,

  // Client-side transport and service errors (1000+):
  kOptimizationGuideUnavailable = 1001,
  kExecutionSessionCreationFailed = 1002,
  kNetworkError = 1003,
  kHandshakeFailed = 1004,
  kMissingConfiguration = 1005,
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_
