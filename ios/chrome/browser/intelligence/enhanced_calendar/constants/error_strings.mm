// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/constants/error_strings.h"

#import "base/notreached.h"

namespace ai {

const char kUnspecifiedErrorString[] = "Unspecified error.";
const char kPrimaryAccountChangeErrorString[] = "Primary account was changed.";
const char kServerModelExecutionErrorString[] =
    "Server model execution error: ";
const char kProtoUnmarshallingErrorString[] = "Proto unmarshalling error.";
const char kWebStateDestroyedBeforeRequestErrorString[] =
    "WebState destroyed before executing request.";
const char kServiceShuttingDownErrorString[] =
    "Enhanced Calendar service is shutting down.";

std::string GetEnhancedCalendarErrorString(EnhancedCalendarError error) {
  switch (error) {
    case EnhancedCalendarError::kUnspecifiedError:
      return kUnspecifiedErrorString;
    case EnhancedCalendarError::kPrimaryAccountChangeError:
      return kPrimaryAccountChangeErrorString;
    case EnhancedCalendarError::kServerModelExecutionError:
      return kServerModelExecutionErrorString;
    case EnhancedCalendarError::kProtoUnmarshallingError:
      return kProtoUnmarshallingErrorString;
    case EnhancedCalendarError::kWebStateDestroyedBeforeRequestError:
      return kWebStateDestroyedBeforeRequestErrorString;
    case EnhancedCalendarError::kServiceShuttingDownError:
      return kServiceShuttingDownErrorString;
  }
  NOTREACHED();
}

}  // namespace ai
