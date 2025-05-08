// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_CONSTANTS_ERROR_STRINGS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_CONSTANTS_ERROR_STRINGS_H_

#import <string>

namespace ai {

// Error message for unspecified error.
extern const char kUnspecifiedErrorString[];

// Error message primary account changed during the request.
extern const char kPrimaryAccountChangeErrorString[];

// Error message server model execution.
extern const char kServerModelExecutionErrorString[];

// Error message proto unmarshalling.
extern const char kProtoUnmarshallingErrorString[];

// Error message when the WebState is destroyed before executing the request.
extern const char kWebStateDestroyedBeforeRequestErrorString[];

// Error message when the service is shutting down.
extern const char kServiceShuttingDownErrorString[];

// Response error type for Enhanced Calendar.
enum class EnhancedCalendarError {
  kUnspecifiedError = 0,
  kPrimaryAccountChangeError = 1,
  kServerModelExecutionError = 2,
  kProtoUnmarshallingError = 3,
  kWebStateDestroyedBeforeRequestError = 4,
  kServiceShuttingDownError = 5,
};

// Retrieves the corresponding string for a given EnhancedCalendarError
// enum value.
std::string GetEnhancedCalendarErrorString(EnhancedCalendarError error);

}  // namespace ai

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_CONSTANTS_ERROR_STRINGS_H_
